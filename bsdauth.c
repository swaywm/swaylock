#include <pwd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <login_cap.h>
#include <bsd_auth.h>
#include <grp.h>

#include "comm.h"
#include "log.h"
#include "password-buffer.h"
#include "swaylock.h"

void initialize_pw_backend(int argc, char **argv) {
	if (!spawn_comm_child()) {
		exit(EXIT_FAILURE);
	}
}
void run_pw_backend_child(void) {
	struct passwd *pwent = getpwuid(getuid());
	if (!pwent) {
		swaylock_log_errno(LOG_ERROR, "failed to getpwuid");
		exit(EXIT_FAILURE);
	}
	struct group *authg = getgrnam("auth");
	if (!authg || !authg->gr_name || !*authg->gr_name) {
		exit(EXIT_FAILURE);
	}
	/* we need setgid(auth) to use auth_userokay() */
	if (setgid(authg->gr_gid)) {
		exit(EXIT_FAILURE);
	}
	while (1) {
		char *buf;
		ssize_t size = read_comm_request(&buf);
		if (size < 0) {
			exit(EXIT_FAILURE);
		} else if (size == 0) {
			break;
		}
		bool success = auth_userokay((char *)pwent->pw_name, NULL, "swaylock", buf);
		explicit_bzero(buf, strlen(buf));
		if (!write_comm_reply(success)) {
			exit(EXIT_FAILURE);
		}

		sleep(2);
	}

	exit(EXIT_SUCCESS);
}
