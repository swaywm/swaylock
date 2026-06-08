#ifndef _SWAYLOCK_KEYPAD_LAYOUT_H
#define _SWAYLOCK_KEYPAD_LAYOUT_H

#define KEYPAD_ROWS 5
#define KEYPAD_COLS 10

static const char *keypad_layout_base[KEYPAD_ROWS][KEYPAD_COLS] = {
	{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
	{"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
	{"a", "s", "d", "f", "g", "h", "j", "k", "l", ";"},
	{"z", "x", "c", "v", "b", "n", "m", ",", ".", "/"},
	{"Go", "Del", "Alt", "", "", "", "", "", "", ""},
};

static const char *keypad_layout_upper[KEYPAD_ROWS][KEYPAD_COLS] = {
	{"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"},
	{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
	{"A", "S", "D", "F", "G", "H", "J", "K", "L", ":"},
	{"Z", "X", "C", "V", "B", "N", "M", "<", ">", "?"},
	{"Go", "Del", "Alt", "", "", "", "", "", "", ""},
};

#endif
