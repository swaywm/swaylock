#ifndef _SWAYLOCK_COMM_H
#define _SWAYLOCK_COMM_H

bool spawn_comm_child(void);
ssize_t read_comm_request(char **buf_ptr);
bool write_comm_reply(bool success);

#endif
