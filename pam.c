#define _POSIX_C_SOURCE 200809L
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "swaylock.h"

void initialize_pw_backend(void) {
	// TODO: only call pam_start once. keep the same handle the whole time
}

static int function_conversation(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *data) {
	struct swaylock_password *pw = data;
	/* PAM expects an array of responses, one for each message */
	struct pam_response *pam_reply = calloc(
			num_msg, sizeof(struct pam_response));
	*resp = pam_reply;
	for (int i = 0; i < num_msg; ++i) {
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			pam_reply[i].resp = strdup(pw->buffer); // PAM clears and frees this
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

bool attempt_password(struct swaylock_password *pw) {
	struct passwd *passwd = getpwuid(getuid());
	char *username = passwd->pw_name;

	bool success = false;

	const struct pam_conv local_conversation = {
		.conv = function_conversation,
		.appdata_ptr = pw,
	};
	pam_handle_t *local_auth_handle = NULL;
	if (pam_start("swaylock", username, &local_conversation, &local_auth_handle)
			!= PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "pam_start failed");
		goto out;
	}

	int pam_status = pam_authenticate(local_auth_handle, 0);
	if (pam_status == PAM_SUCCESS) {
		swaylock_log(LOG_DEBUG, "pam_authenticate succeeded");
		success = true;
	} else {
		swaylock_log(LOG_ERROR, "pam_authenticate failed: %s",
			get_pam_auth_error(pam_status));
	}

	if (pam_end(local_auth_handle, pam_status) != PAM_SUCCESS) {
		swaylock_log(LOG_ERROR, "pam_end failed");
		success = false;
	}

out:
	clear_password_buffer(pw);
	return success;
}
