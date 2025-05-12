#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "comm.h"
#include "log.h"
#include "swaylock.h"
#include "password-buffer.h"

static int comm[2][2] = {{-1, -1}, {-1, -1}};

static ssize_t read_full(int fd, void *dst, size_t size) {
	char *buf = dst;
	size_t offset = 0;
	while (offset < size) {
		ssize_t n = read(fd, &buf[offset], size - offset);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			swaylock_log_errno(LOG_ERROR, "read() failed");
			return -1;
		} else if (n == 0) {
			if (offset == 0) {
				return 0;
			}
			swaylock_log(LOG_ERROR, "read() failed: unexpected EOF");
			return -1;
		}
		offset += n;
	}
	return offset;
}

static bool write_full(int fd, const void *src, size_t size) {
	const char *buf = src;
	size_t offset = 0;
	while (offset < size) {
		ssize_t n = write(fd, &buf[offset], size - offset);
		if (n <= 0) {
			assert(n != 0);
			if (errno == EINTR) {
				continue;
			}
			swaylock_log_errno(LOG_ERROR, "write() failed");
			return false;
		}
		offset += n;
	}
	return true;
}

ssize_t read_comm_request(char **buf_ptr) {
	int fd = comm[0][0];

	size_t size;
	ssize_t n = read_full(fd, &size, sizeof(size));
	if (n <= 0) {
		return n;
	}
	assert(size > 0);

	swaylock_log(LOG_DEBUG, "received pw check request");

	char *buf = password_buffer_create(size);
	if (!buf) {
		return -1;
	}

	if (read_full(fd, buf, size) <= 0) {
		swaylock_log_errno(LOG_ERROR, "failed to read pw");
		return -1;
	}

	assert(buf[size - 1] == '\0');
	*buf_ptr = buf;
	return size;
}

bool write_comm_reply(bool success) {
	return write_full(comm[1][1], &success, sizeof(success));
}

bool spawn_comm_child(void) {
	if (pipe(comm[0]) != 0) {
		swaylock_log_errno(LOG_ERROR, "failed to create pipe");
		return false;
	}
	if (pipe(comm[1]) != 0) {
		swaylock_log_errno(LOG_ERROR, "failed to create pipe");
		return false;
	}
	pid_t child = fork();
	if (child < 0) {
		swaylock_log_errno(LOG_ERROR, "failed to fork");
		return false;
	} else if (child == 0) {
		struct sigaction sa = {
			.sa_handler = SIG_IGN,
		};
		sigaction(SIGUSR1, &sa, NULL);
		close(comm[0][1]);
		close(comm[1][0]);
		run_pw_backend_child();
	}
	close(comm[0][0]);
	close(comm[1][1]);
	return true;
}

bool write_comm_request(struct swaylock_password *pw) {
	bool result = false;
	int fd = comm[0][1];

	size_t size = pw->len + 1;
	if (!write_full(fd, &size, sizeof(size))) {
		swaylock_log_errno(LOG_ERROR, "Failed to write pw size");
		goto out;
	}

	if (!write_full(fd, pw->buffer, size)) {
		swaylock_log_errno(LOG_ERROR, "Failed to write pw buffer");
		goto out;
	}

	result = true;

out:
	clear_password_buffer(pw);
	return result;
}

bool read_comm_reply(bool *auth_success) {
	if (read_full(comm[1][0], auth_success, sizeof(*auth_success)) <= 0) {
		swaylock_log(LOG_ERROR, "Failed to read pw result");
		return false;
	}
	return true;
}

int get_comm_reply_fd(void) {
	return comm[1][0];
}
