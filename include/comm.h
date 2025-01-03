#ifndef _SWAYLOCK_COMM_H
#define _SWAYLOCK_COMM_H

#include <stdbool.h>

struct swaylock_password;

enum backend_message_type {
	//TODO: error messages separately?
	BACKEND_MESSAGE_TYPE_TEXT, // Text info, error message or prompt
	BACKEND_MESSAGE_TYPE_AUTH_RESULT, // Boolean indicating authorization success or failure
};

bool spawn_comm_child(void);

// Write a string to a file descriptor by first sending the size and then the
// string data. len should be the length of the string, *including* null
// termination. Returns the number of bytes in the written string, *not* the
// total no. of bytes written - the total number is the return value plus
// sizeof(size_t).
ssize_t write_string(int fd, const char * const *string, size_t len);

// Read a string from a file descriptor by first reading the size, allocating
// memory to output using alloc(size) and then reading the string data to it.
// Returns the number of bytes in the string, *not* the total no. of bytes read
// - the total number is the return value plus sizeof(size_t).
ssize_t read_string(int fd, char *(*alloc)(size_t), char **output);

// Read a message from the password checking backend (e.g. a prompt or the
// result of authentication) in the main thread. Read first the message type,
// then the associated data itself (size + string data for strings, just the
// boolean value for booleans).
// Returns the no. of bytes in the *data* that was read, the total no. of bytes
// read is the return value plus sizeof(enum backend_message_type).
ssize_t read_comm_message_from_backend(enum backend_message_type *msg_type, void **data);
// Write a string containing a message from the backend
// Returns the no. of bytes in the *message* that was written, the total no. of
// bytes written is the return value plus sizeof(enum backend_message_type).
ssize_t write_comm_text_message_from_backend(const char * const *msg);
// Write a boolean value indicating the result of authentication
// Returns the no. of bytes in the *data* that was written (i.e. sizeof(bool)),
// the total no. of bytes written is the return value plus
// sizeof(enum backend_message_type).
ssize_t write_comm_auth_result_from_backend(bool success);

// Read / write the response typed by the user (password, PIN code, etc) to a
// prompt sent by the backend. The password buffer is always cleared when the
// function returns. //TODO: verify
ssize_t read_comm_prompt_response(char **buf_ptr);
bool write_comm_prompt_response(struct swaylock_password *pw);

// FD to poll for messages from the backend
int get_comm_backend_message_fd(void);

#endif
