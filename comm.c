#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "comm.h"
#include "log.h"
#include "swaylock.h"
#include "password-buffer.h"

static int comm[2][2] = {{-1, -1}, {-1, -1}};

ssize_t read_string(int fd, char *(*alloc)(size_t), char **output) {
	size_t size;
	ssize_t amt = read(fd, &size, sizeof(size));
	if (amt <= 0) {
		swaylock_log_errno(LOG_ERROR, "Failed to read string size");
		return amt;
	}
	if (size == 0) {
		return 0;
	}
	char *buf = alloc(size);
	if (!buf) {
		return -1;
	}
	size_t offs = 0;
	do {
		ssize_t amt = read(fd, &buf[offs], size - offs);
		if (amt <= 0) {
			swaylock_log_errno(LOG_ERROR, "Failed to read string");
			return -1;
		}
		offs += (size_t)amt;
	} while (offs < size);

	*output = buf;
	return size;
}

ssize_t read_comm_request(char **buf_ptr) {
	ssize_t amt = read_string(comm[0][0], password_buffer_create, buf_ptr);
	swaylock_log(LOG_DEBUG, "received pw check request");
	if (amt == 0) {
		return 0;
	} else if (amt < 0) {
		swaylock_log(LOG_ERROR, "read pw request");
		return -1;
	}
	return amt;
}

bool write_comm_reply(bool success) {
	if (write(comm[1][1], &success, sizeof(success)) != sizeof(success)) {
		swaylock_log_errno(LOG_ERROR, "failed to write pw check result");
		return false;
	}
	return true;
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

	size_t len = pw->len + 1;
	size_t offs = 0;
	if (write(comm[0][1], &len, sizeof(len)) < 0) {
		swaylock_log_errno(LOG_ERROR, "Failed to request pw check");
		goto out;
	}

	do {
		ssize_t amt = write(comm[0][1], &pw->buffer[offs], len - offs);
		if (amt < 0) {
			swaylock_log_errno(LOG_ERROR, "Failed to write pw buffer");
			goto out;
		}
		offs += amt;
	} while (offs < len);

	result = true;

out:
	clear_password_buffer(pw);
	return result;
}

bool read_comm_reply(void) {
	bool result = false;
	if (read(comm[1][0], &result, sizeof(result)) != sizeof(result)) {
		swaylock_log_errno(LOG_ERROR, "Failed to read pw result");
		result = false;
	}
	return result;
}

int get_comm_reply_fd(void) {
	return comm[1][0];
}
