#define _XOPEN_SOURCE // for crypt
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
#include "swaylock.h"

void initialize_pw_backend(int argc, char **argv) {
	if (geteuid() != 0) {
		swaylock_log(LOG_ERROR,
				"swaylock needs to be setuid to read /etc/shadow");
		exit(EXIT_FAILURE);
	}

	if (!spawn_comm_child()) {
		exit(EXIT_FAILURE);
	}

	if (setgid(getgid()) != 0) {
		swaylock_log_errno(LOG_ERROR, "Unable to drop root");
		exit(EXIT_FAILURE);
	}
	if (setuid(getuid()) != 0) {
		swaylock_log_errno(LOG_ERROR, "Unable to drop root");
		exit(EXIT_FAILURE);
	}
	if (setuid(0) != -1) {
		swaylock_log_errno(LOG_ERROR, "Unable to drop root (we shouldn't be "
			"able to restore it after setuid)");
		exit(EXIT_FAILURE);
	}
}

void run_pw_backend_child(void) {
	/* This code runs as root */
	struct passwd *pwent = getpwuid(getuid());
	if (!pwent) {
		swaylock_log_errno(LOG_ERROR, "failed to getpwuid");
		exit(EXIT_FAILURE);
	}
	char *encpw = pwent->pw_passwd;
	if (strcmp(encpw, "x") == 0) {
		struct spwd *swent = getspnam(pwent->pw_name);
		if (!swent) {
			swaylock_log_errno(LOG_ERROR, "failed to getspnam");
			exit(EXIT_FAILURE);
		}
		encpw = swent->sp_pwdp;
	}

	/* We don't need any additional logging here because the parent process will
	 * also fail here and will handle logging for us. */
	if (setgid(getgid()) != 0) {
		exit(EXIT_FAILURE);
	}
	if (setuid(getuid()) != 0) {
		exit(EXIT_FAILURE);
	}
	if (setuid(0) != -1) {
		exit(EXIT_FAILURE);
	}

	/* This code does not run as root */
	swaylock_log(LOG_DEBUG, "Prepared to authorize user %s", pwent->pw_name);

	while (1) {
		char *buf;
		ssize_t size = read_comm_request(&buf);
		if (size < 0) {
			exit(EXIT_FAILURE);
		} else if (size == 0) {
			break;
		}

		char *c = crypt(buf, encpw);
		if (c == NULL) {
			swaylock_log_errno(LOG_ERROR, "crypt failed");
			clear_buffer(buf, size);
			exit(EXIT_FAILURE);
		}
		bool success = strcmp(c, encpw) == 0;

		if (!write_comm_reply(success)) {
			clear_buffer(buf, size);
			exit(EXIT_FAILURE);
		}

		// We don't want to keep it in memory longer than necessary,
		// so clear *before* sleeping.
		clear_buffer(buf, size);
		free(buf);

		sleep(2);
	}

	clear_buffer(encpw, strlen(encpw));
	exit(EXIT_SUCCESS);
}