#ifndef _SWAY_PASSWORD_BUFFER_H
#define _SWAY_PASSWORD_BUFFER_H

#include <stddef.h>

char *password_buffer_create(size_t size);
void password_buffer_destroy(char *buffer, size_t size);

#endif
