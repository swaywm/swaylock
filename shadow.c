#define _XOPEN_SOURCE // for crypt
#include <assert.h>
#include <pwd.h>
#include <shadow.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __GLIBC__
// GNU, you damn slimy bastard
#include <crypt.h>
#endif
#include "comm.h"
#include "log.h"
#include "password-buffer.h"
#include "swaylock.h"

char *encpw = NULL;

void initialize_pw_backend(int argc, char **argv) {
	/* This code runs as root */
	struct passwd *pwent = getpwuid(getuid());
	if (!pwent) {
		swaylock_log_errno(LOG_ERROR, "failed to getpwuid");
		exit(EXIT_FAILURE);
	}
	encpw = pwent->pw_passwd;
	if (strcmp(encpw, "x") == 0) {
		struct spwd *swent = getspnam(pwent->pw_name);
		if (!swent) {
			swaylock_log_errno(LOG_ERROR, "failed to getspnam");
			exit(EXIT_FAILURE);
		}
		encpw = swent->sp_pwdp;
	}

	if (setgid(getgid()) != 0) {
		swaylock_log_errno(LOG_ERROR, "Unable to drop root");
		exit(EXIT_FAILURE);
	}
	if (setuid(getuid()) != 0) {
		swaylock_log_errno(LOG_ERROR, "Unable to drop root");
		exit(EXIT_FAILURE);
	}
	if (setuid(0) != -1 || setgid(0) != -1) {
		swaylock_log_errno(LOG_ERROR, "Unable to drop root (we shouldn't be "
			"able to restore it after setuid/setgid)");
		exit(EXIT_FAILURE);
	}

	/* This code does not run as root */
	swaylock_log(LOG_DEBUG, "Prepared to authorize user %s", pwent->pw_name);

	if (!spawn_comm_child()) {
		exit(EXIT_FAILURE);
	}

	/* Buffer is only used by the child */
	clear_buffer(encpw, strlen(encpw));
	encpw = NULL;
}

void run_pw_backend_child(void) {
	assert(encpw != NULL);
	while (1) {
		char *buf;
		ssize_t size = read_comm_request(&buf);
		if (size < 0) {
			exit(EXIT_FAILURE);
		} else if (size == 0) {
			break;
		}

		const char *c = crypt(buf, encpw);
		password_buffer_destroy(buf, size);
		buf = NULL;

		if (c == NULL) {
			swaylock_log_errno(LOG_ERROR, "crypt failed");
			exit(EXIT_FAILURE);
		}
		bool success = strcmp(c, encpw) == 0;

		if (!write_comm_reply(success)) {
			exit(EXIT_FAILURE);
		}

		sleep(2);
	}

	clear_buffer(encpw, strlen(encpw));
	exit(EXIT_SUCCESS);
}
