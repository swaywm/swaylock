#ifndef _SWAY_UNICODE_H
#define _SWAY_UNICODE_H
#include <stddef.h>
#include <stdint.h>

// Technically UTF-8 supports up to 6 byte codepoints, but Unicode itself
// doesn't really bother with more than 4.
#define UTF8_MAX_SIZE 4

#define UTF8_INVALID 0x80

/**
 * Gets the size in bytes of the last utf8 character in a NULL terminated string
 * This function does not validate that the buffer contains correct utf8 data;
 * it merely looks for the first byte that correctly denotes the beginning of a
 * utf8 character.
 */
int utf8_last_size(const char *str);

/**
 * Grabs the next UTF-8 character and advances the string pointer
 */
uint32_t utf8_decode(const char **str);

/**
 * Encodes a character as UTF-8 and returns the length of that character.
 */
size_t utf8_encode(char *str, uint32_t ch);

/**
 * Returns the size of the next UTF-8 character
 */
int utf8_size(const char *str);

/**
 * Returns the size of a UTF-8 character
 */
size_t utf8_chsize(uint32_t ch);

#endif

