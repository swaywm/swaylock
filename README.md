# swaylock-effects

Swaylock-effects is a fork of [swaylock](https://github.com/swaywm/swaylock)
which adds built-in screenshots and image manipulation effects like blurring.

![Screenshot](https://raw.githubusercontent.com/mortie/swaylock-effects/master/screenshot.jpg)

## Installation

### Compiling from Source

Install dependencies:

* meson \*
* wayland
* wayland-protocols \*
* libxkbcommon
* cairo
* gdk-pixbuf2 \*\*
* pam (optional)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages) \*
* git \*

_\*Compile-time dep_

_\*\*optional: required for background images other than PNG_

Run these commands:

	meson build
	ninja -C build
	sudo ninja -C build install

On systems without PAM, you need to suid the swaylock binary:

	sudo chmod a+s /usr/local/bin/swaylock

Swaylock will drop root permissions shortly after startup.
