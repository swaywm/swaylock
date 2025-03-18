#include "password-buffer.h"
#include "log.h"
#include "swaylock.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>

static bool mlock_supported = true;
static long int page_size = 0;

static long int get_page_size() {
	if (!page_size) {
		page_size = sysconf(_SC_PAGESIZE);
	}
	return page_size;
}

// password_buffer_lock expects addr to be page alligned
static bool password_buffer_lock(char *addr, size_t size) {
	int retries = 5;
	while (mlock(addr, size) != 0 && retries > 0) {
		switch (errno) {
		case EAGAIN:
			retries--;
			if (retries == 0) {
				swaylock_log(LOG_ERROR, "mlock() supported but failed too often.");
				return false;
			}
			break;
		case EPERM:
			swaylock_log_errno(LOG_ERROR, "Unable to mlock() password memory: Unsupported!");
			mlock_supported = false;
			return true;
		default:
			swaylock_log_errno(LOG_ERROR, "Unable to mlock() password memory.");
			return false;
		}
	}

	return true;
}

// password_buffer_unlock expects addr to be page alligned
static bool password_buffer_unlock(char *addr, size_t size) {
	if (mlock_supported) {
		if (munlock(addr, size) != 0) {
			swaylock_log_errno(LOG_ERROR, "Unable to munlock() password memory.");
			return false;
		}
	}

	return true;
}

char *password_buffer_create(size_t size) {
	void *buffer;
	int result = posix_memalign(&buffer, get_page_size(), size);
	if (result) {
		//posix_memalign doesn't set errno according to the man page
		errno = result;
		swaylock_log_errno(LOG_ERROR, "failed to alloc password buffer");
		return NULL;
	}

	if (!password_buffer_lock(buffer, size)) {
		free(buffer);
		return NULL;
	}

	return buffer;
}

void password_buffer_destroy(char *buffer, size_t size) {
	clear_buffer(buffer, size);
	password_buffer_unlock(buffer, size);
	free(buffer);
}
