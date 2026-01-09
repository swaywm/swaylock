#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/prctl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm.h"
#include "log.h"
#include "password-buffer.h"
#include "swaylock.h"

static char *pw_buf = NULL;

void initialize_pw_backend(int argc, char **argv) {
	if (getuid() != geteuid() || getgid() != getegid()) {
		swaylock_log(LOG_ERROR,
			"swaylock is setuid, but was compiled with the PAM"
			" backend. Run 'chmod a-s %s' to fix. Aborting.", argv[0]);
		exit(EXIT_FAILURE);
	}
	if (!spawn_comm_child()) {
		exit(EXIT_FAILURE);
	}
}

static int handle_conversation(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *data) {
	/* PAM expects an array of responses, one for each message */
	struct pam_response *pam_reply =
		calloc(num_msg, sizeof(struct pam_response));
	if (pam_reply == NULL) {
		swaylock_log(LOG_ERROR, "Allocation failed");
		return PAM_ABORT;
	}
	*resp = pam_reply;
	for (int i = 0; i < num_msg; ++i) {
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			pam_reply[i].resp = strdup(pw_buf); // PAM clears and frees this
			if (pam_reply[i].resp == NULL) {
				swaylock_log(LOG_ERROR, "Allocation failed");
				return PAM_ABORT;
			}
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			break;
		}
	}
	return PAM_SUCCESS;
}

void run_pw_backend_child(void) {
	struct passwd *passwd = getpwuid(getuid());
	if (!passwd) {
		swaylock_log_errno(LOG_ERROR, "getpwuid failed");
		exit(EXIT_FAILURE);
	}

	char *username = passwd->pw_name;

	const struct pam_conv conv = {
		.conv = handle_conversation,
		.appdata_ptr = NULL,
	};
	pam_handle_t *auth_handle = NULL;
	if (pam_start("swaylock", username, &conv, &auth_handle) != PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "pam_start failed");
		exit(EXIT_FAILURE);
	}

	/* This code does not run as root */
	swaylock_log(LOG_DEBUG, "Prepared to authorize user %s", username);

	int pam_status = PAM_SUCCESS;
	while (1) {
		ssize_t size = read_comm_request(&pw_buf);
		if (size < 0) {
			exit(EXIT_FAILURE);
		} else if (size == 0) {
			break;
		}

		int pam_status = pam_authenticate(auth_handle, 0);
		password_buffer_destroy(pw_buf, size);
		pw_buf = NULL;

		bool success = pam_status == PAM_SUCCESS;
		if (!success) {
			swaylock_log(LOG_ERROR, "pam_authenticate failed: %s",
				pam_strerror(auth_handle, pam_status));
		}

		if (!write_comm_reply(success)) {
			exit(EXIT_FAILURE);
		}

		if (success) {
			/* Unsuccessful requests may be queued after a successful one;
			 * do not process them. */
			break;
		}
	}

	pam_setcred(auth_handle, PAM_REFRESH_CRED);

	if (pam_end(auth_handle, pam_status) != PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "pam_end failed");
		exit(EXIT_FAILURE);
	}

	exit((pam_status == PAM_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int fingerprint_pipe[2] = {-1, -1};

// Conversation function for fingerprint auth - always returns empty password.
// pam_fprintd will handle the actual authentication via fingerprint.
static int fingerprint_conversation(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *data) {
	struct pam_response *pam_reply = calloc(num_msg, sizeof(*pam_reply));
	if (pam_reply == NULL) {
		return PAM_ABORT;
	}
	*resp = pam_reply;
	for (int i = 0; i < num_msg; ++i) {
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			pam_reply[i].resp = strdup("");
			if (pam_reply[i].resp == NULL) {
				return PAM_ABORT;
			}
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			break;
		}
	}
	return PAM_SUCCESS;
}

// Child process that continuously attempts fingerprint authentication.
// Blocks on pam_authenticate() until pam_fprintd detects a fingerprint.
static void run_fingerprint_child(void) {
	struct passwd *passwd = getpwuid(getuid());
	if (!passwd) {
		swaylock_log_errno(LOG_ERROR, "getpwuid failed");
		exit(EXIT_FAILURE);
	}

	const struct pam_conv conv = {
		.conv = fingerprint_conversation,
		.appdata_ptr = NULL,
	};
	pam_handle_t *auth_handle = NULL;
	int pam_status;

	pam_status = pam_start("swaylock-fingerprint", passwd->pw_name, &conv, &auth_handle);
	if (pam_status != PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "fingerprint: pam_start failed (%d)", pam_status);
		exit(EXIT_FAILURE);
	}

	while (1) {
		swaylock_log(LOG_DEBUG, "fingerprint: waiting");
		int pam_status = pam_authenticate(auth_handle, 0);

		bool success = pam_status == PAM_SUCCESS;
		write(fingerprint_pipe[1], &success, sizeof(success));

		if (success) {
			swaylock_log(LOG_INFO, "fingerprint: authentication successful");
			break;
		}

		// pam_fprintd returns PAM_AUTHINFO_UNAVAIL if no finger detected,
		// PAM_AUTH_ERR if wrong finger. Keep trying.
		swaylock_log(LOG_ERROR, "fingerprint: pam_authenticate failed: %s",
			pam_strerror(auth_handle, pam_status));
	}

	pam_setcred(auth_handle, PAM_REFRESH_CRED);
	pam_end(auth_handle, PAM_SUCCESS);
	exit(EXIT_SUCCESS);
}

bool spawn_fingerprint_child(void) {
	if (pipe(fingerprint_pipe) != 0) {
		swaylock_log_errno(LOG_ERROR, "failed to create fingerprint pipe");
		return false;
	}

	pid_t child = fork();
	if (child < 0) {
		swaylock_log_errno(LOG_ERROR, "failed to fork fingerprint child");
		return false;
	} else if (child == 0) {
		prctl(PR_SET_PDEATHSIG, SIGTERM);
		struct sigaction sa = { .sa_handler = SIG_IGN };
		sigaction(SIGUSR1, &sa, NULL);
		close(fingerprint_pipe[0]);
		run_fingerprint_child();
	}

	if (fcntl(fingerprint_pipe[0], F_SETFL, O_NONBLOCK) == -1) {
		swaylock_log(LOG_ERROR, "Failed to make pipe end nonblocking");
		return false;
	}

	close(fingerprint_pipe[1]);
	return true;
}

int get_fingerprint_fd(void) {
	return fingerprint_pipe[0];
}
