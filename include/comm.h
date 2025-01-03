#ifndef _SWAYLOCK_COMM_H
#define _SWAYLOCK_COMM_H

#include <stdbool.h>

struct swaylock_password;

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

bool write_comm_reply(bool success);
bool read_comm_reply(void);

// Read / write the response typed by the user (password, PIN code, etc) to a
// prompt sent by the backend. The password buffer is always cleared when the
// function returns. //TODO: verify
ssize_t read_comm_prompt_response(char **buf_ptr);
bool write_comm_prompt_response(struct swaylock_password *pw);

// FD to poll for password authentication replies.
int get_comm_reply_fd(void);

#endif
