#include "swaylock.h"
#include "log.h"
#include "config.h"
#include <stdlib.h>

#if !HAVE_PAM
bool load_pam_library(void) {
	return false;
}

bool is_pam_loaded(void) {
	return false;
}

void initialize_pam_backend(int argc, char **argv) {
	// Stub function - should never be called
}

void run_pam_backend_child(void) {
	// Stub function - should never be called
}
#endif

#if !HAVE_SHADOW
void initialize_shadow_backend(int argc, char **argv) {
	// Stub function - should never be called
}

void run_shadow_backend_child(void) {
	// Stub function - should never be called
}
#endif

void initialize_pw_backend(int argc, char **argv) {
#if HAVE_PAM
	if (load_pam_library()) {
		swaylock_log(LOG_ERROR, "Initialising pam");
		initialize_pam_backend(argc, argv);
		return;
	}
#endif
#if HAVE_SHADOW
	swaylock_log(LOG_ERROR, "Initialising shadow");
	initialize_shadow_backend(argc, argv);
#else
	swaylock_log(LOG_ERROR, "No authentication backend available");
	exit(EXIT_FAILURE);
#endif
}

void run_pw_backend_child(void) {
#if HAVE_PAM
	if (is_pam_loaded()) {
		run_pam_backend_child();
		return;
	}
#endif
#if HAVE_SHADOW
	run_shadow_backend_child();
#else
	swaylock_log(LOG_ERROR, "No authentication backend available");
	exit(EXIT_FAILURE);
#endif
}
