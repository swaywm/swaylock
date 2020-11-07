#define _POSIX_C_SOURCE 200809L
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm.h"
#include "log.h"
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

static const char *get_pam_auth_error(int pam_status) {
	switch (pam_status) {
	case PAM_AUTH_ERR:
		return "invalid credentials";
	case PAM_CRED_INSUFFICIENT:
		return "swaylock cannot authenticate users; check /etc/pam.d/swaylock "
			"has been installed properly";
	case PAM_AUTHINFO_UNAVAIL:
		return "authentication information unavailable";
	case PAM_MAXTRIES:
		return "maximum number of authentication tries exceeded";
	default:;
		static char msg[64];
		snprintf(msg, sizeof(msg), "unknown error (%d)", pam_status);
		return msg;
	}
}

void run_pw_backend_child(void) {
	struct passwd *passwd = getpwuid(getuid());
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
		bool success = pam_status == PAM_SUCCESS;
		if (!success) {
			swaylock_log(LOG_ERROR, "pam_authenticate failed: %s",
				get_pam_auth_error(pam_status));
		}

		if (!write_comm_reply(success)) {
			clear_buffer(pw_buf, size);
			exit(EXIT_FAILURE);
		}

		clear_buffer(pw_buf, size);
		free(pw_buf);
		pw_buf = NULL;
	}

	pam_setcred(auth_handle, PAM_REFRESH_CRED);

	if (pam_end(auth_handle, pam_status) != PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "pam_end failed");
		exit(EXIT_FAILURE);
	}

	exit((pam_status == PAM_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE);
}
