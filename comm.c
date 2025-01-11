#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "comm.h"
#include "log.h"
#include "swaylock.h"
#include "password-buffer.h"

static int comm[2][2] = {{-1, -1}, {-1, -1}};

ssize_t write_string(int fd, const char *string, size_t len) {
	size_t offs = 0;
	if (write(fd, &len, sizeof(len)) < 0) {
		swaylock_log_errno(LOG_ERROR, "Failed to write string size");
		return -1;
	}

	do {
		ssize_t amt = write(fd, &string[offs], len - offs);
		if (amt < 0) {
			swaylock_log_errno(LOG_ERROR, "Failed to write string");
			//TODO: different return value for different error?
			return -1;
		}
		offs += amt;
	} while (offs < len);

	return (ssize_t) len;
}

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

ssize_t read_comm_prompt_response(char **buf_ptr) {
	ssize_t amt = read_string(comm[0][0], password_buffer_create, buf_ptr);
	swaylock_log(LOG_DEBUG, "received response to prompt");
	if (amt == 0) {
		return 0;
	} else if (amt < 0) {
		swaylock_log(LOG_ERROR, "Error reading prompt response");
		return -1;
	}
	return amt;
}

ssize_t write_comm_text_message_from_backend(const char *msg) {
	enum backend_message_type msg_type = BACKEND_MESSAGE_TYPE_TEXT;
	if (write(comm[1][1], &msg_type, sizeof(msg_type)) != sizeof(msg_type)) {
		swaylock_log_errno(LOG_ERROR, "failed to write message type");
		return -1;
	}
	return write_string(comm[1][1], msg, strlen(msg) + 1);
}

ssize_t write_comm_auth_result_from_backend(bool success) {
	enum backend_message_type msg_type = BACKEND_MESSAGE_TYPE_AUTH_RESULT;
	if (write(comm[1][1], &msg_type, sizeof(msg_type)) != sizeof(msg_type)) {
		swaylock_log_errno(LOG_ERROR, "failed to write message type");
		return -1;
	}

	if (write(comm[1][1], &success, sizeof(success)) != sizeof(success)) {
		swaylock_log_errno(LOG_ERROR, "failed to write authentication result");
		return -1;
	}
	return sizeof(success);
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

bool write_comm_prompt_response(struct swaylock_password *pw) {
	bool result = false;
	ssize_t amt = write_string(comm[0][1], pw->buffer, pw->len + 1);

	if (amt < 0) {
		swaylock_log_errno(LOG_ERROR, "Failed to write prompt response");
	} else {
		result = true;
	}

	clear_password_buffer(pw);
	return result;
}

char *malloc_str(size_t size) {
	char *res = (char *) malloc(size * sizeof(char));
	if (!res) {
		swaylock_log_errno(LOG_ERROR, "failed to allocate string");
		return NULL;
	}
	return res;
}

ssize_t read_comm_message_from_backend(enum backend_message_type *msg_type, void **data) {
	enum backend_message_type read_type;
	void *read_data;

	if (read(comm[1][0], &read_type, sizeof(read_type)) != sizeof(read_type)) {
		swaylock_log_errno(LOG_ERROR, "Failed to read message type from backend");
		return -1;
	}

	ssize_t amt;
	switch(read_type) {
	case BACKEND_MESSAGE_TYPE_TEXT:
		amt = read_string(comm[1][0], malloc_str, (char **) &read_data);
		if (amt < 0) {
			swaylock_log(LOG_ERROR, "Error reading string from backend");
			return -1;
		} else if (amt == 0) {
			read_data = NULL; //TODO: good?
		}
		break;

	case BACKEND_MESSAGE_TYPE_AUTH_RESULT:
		read_data = malloc(sizeof(bool));
		amt = read(comm[1][0], (bool *) read_data, sizeof(bool));
		if (amt != sizeof(bool)) {
			swaylock_log(LOG_ERROR, "Error reading boolean from backend");
			return -1;
		}
		break;

	}

	*msg_type = read_type;
	*data = read_data;
	return amt;
}

int get_comm_backend_message_fd(void) {
	return comm[1][0];
}
