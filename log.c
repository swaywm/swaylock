#define _POSIX_C_SOURCE 199506L
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "log.h"

static enum log_importance log_importance = LOG_ERROR;

static const char *verbosity_colors[] = {
	[LOG_SILENT] = "",
	[LOG_ERROR ] = "\x1B[1;31m",
	[LOG_INFO  ] = "\x1B[1;34m",
	[LOG_DEBUG ] = "\x1B[1;30m",
	[LOG_TRACE ] = "\x1B[1;32m",
};

void swaylock_log_init(enum log_importance verbosity) {
	if (verbosity < LOG_IMPORTANCE_LAST) {
		log_importance = verbosity;
	}
}

void _swaylock_log(enum log_importance verbosity, const char *fmt, ...) {
	if (verbosity > log_importance) {
		return;
	}

	va_list args;
	va_start(args, fmt);

	// prefix the time to the log message
	struct tm result;
	time_t t = time(NULL);
	struct tm *tm_info = localtime_r(&t, &result);
	char buffer[26];

	// generate time prefix
	strftime(buffer, sizeof(buffer), "%F %T - ", tm_info);
	fprintf(stderr, "%s", buffer);

	unsigned c = (verbosity < LOG_IMPORTANCE_LAST)
		? verbosity : LOG_IMPORTANCE_LAST - 1;

	if (isatty(STDERR_FILENO)) {
		fprintf(stderr, "%s", verbosity_colors[c]);
	}

	vfprintf(stderr, fmt, args);

	if (isatty(STDERR_FILENO)) {
		fprintf(stderr, "\x1B[0m");
	}
	fprintf(stderr, "\n");

	va_end(args);
}

// This is mainly here for performance.
// Don't want to do _swaylock_strip_path every event if we're not tracing.
void _swaylock_trace(const char *file, int line, const char *func) {
	if (LOG_TRACE > log_importance) {
		return;
	}

	_swaylock_log(LOG_TRACE, "[%s:%d]: trace: %s",
			_swaylock_strip_path(file), line, func);
}

const char *_swaylock_strip_path(const char *filepath) {
	if (*filepath == '.') {
		while (*filepath == '.' || *filepath == '/') {
			++filepath;
		}
	}
	return filepath;
 }
