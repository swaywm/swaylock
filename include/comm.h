#ifndef _SWAYLOCK_COMM_H
#define _SWAYLOCK_COMM_H

#include <stdbool.h>

struct swaylock_password;

bool spawn_comm_child(void);

// Read a string from a file descriptor by first reading the size, allocating
// memory to output using alloc(size) and then reading the string data to it.
// Returns the number of bytes in the string, *not* the total no. of bytes read
// - the total number is the return value plus sizeof(size_t).
ssize_t read_string(int fd, char *(*alloc)(size_t), char **output);

ssize_t read_comm_request(char **buf_ptr);
bool write_comm_reply(bool success);
// Requests the provided password to be checked. The password is always cleared
// when the function returns.
bool write_comm_request(struct swaylock_password *pw);
bool read_comm_reply(void);
// FD to poll for password authentication replies.
int get_comm_reply_fd(void);

#endif
