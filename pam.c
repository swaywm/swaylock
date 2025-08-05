#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "comm.h"
#include "log.h"
#include "password-buffer.h"
#include "swaylock.h"

static char *pw_buf = NULL;
static void *pam_handle = NULL;

// Function pointers for PAM symbols
static int (*fp_pam_start)(const char *, const char *, const struct pam_conv *, pam_handle_t **) = NULL;
static int (*fp_pam_authenticate)(pam_handle_t *, int) = NULL;
static int (*fp_pam_end)(pam_handle_t *, int) = NULL;
static int (*fp_pam_setcred)(pam_handle_t *, int) = NULL;

bool load_pam_library(void) {
	pam_handle = dlopen("libpam.so.0", RTLD_NOW);
	if (!pam_handle) {
		swaylock_log(LOG_ERROR, "Failed to load libpam.so.0: %s", dlerror());
		return false;
	}

	fp_pam_start = dlsym(pam_handle, "pam_start");
	fp_pam_authenticate = dlsym(pam_handle, "pam_authenticate");
	fp_pam_end = dlsym(pam_handle, "pam_end");
	fp_pam_setcred = dlsym(pam_handle, "pam_setcred");

	if (!fp_pam_start || !fp_pam_authenticate || !fp_pam_end || !fp_pam_setcred) {
		swaylock_log(LOG_ERROR, "Failed to load one or more PAM symbols");
		dlclose(pam_handle);
		pam_handle = NULL;
		return false;
	}
	return true;
}

bool is_pam_loaded(void) {
	return pam_handle != NULL;
}

static void unload_pam_library(void) {
	if (pam_handle) {
		dlclose(pam_handle);
		pam_handle = NULL;
	}
}

void initialize_pam_backend(int argc, char **argv) {
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

void run_pam_backend_child(void) {
	struct passwd *passwd = getpwuid(getuid());
	if (!passwd) {
		swaylock_log_errno(LOG_ERROR, "getpwuid failed");
		unload_pam_library();
		exit(EXIT_FAILURE);
	}

	char *username = passwd->pw_name;

	const struct pam_conv conv = {
		.conv = handle_conversation,
		.appdata_ptr = NULL,
	};
	pam_handle_t *auth_handle = NULL;
	if (fp_pam_start("swaylock", username, &conv, &auth_handle) != PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "pam_start failed");
		unload_pam_library();
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

		int pam_status = fp_pam_authenticate(auth_handle, 0);
		password_buffer_destroy(pw_buf, size);
		pw_buf = NULL;

		bool success = pam_status == PAM_SUCCESS;
		if (!success) {
			swaylock_log(LOG_ERROR, "pam_authenticate failed: %s",
				get_pam_auth_error(pam_status));
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

	fp_pam_setcred(auth_handle, PAM_REFRESH_CRED);

	if (fp_pam_end(auth_handle, pam_status) != PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "pam_end failed");
		unload_pam_library();
		exit(EXIT_FAILURE);
	}

	unload_pam_library();
	exit((pam_status == PAM_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE);
}
