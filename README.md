# swaylock

swaylock is a screen locking utility for Wayland compositors. It is compatible
with any Wayland compositor which implements the following Wayland protocols:

- wlr-layer-shell
- wlr-input-inhibitor
- xdg-output
- xdg-shell

See the man page, `swaylock(1)`, for instructions on using swaylock.

## Release Signatures

Releases are signed with [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
and published [on GitHub](https://github.com/swaywm/sway/releases). swaylock
releases are managed independently of sway releases.

## Installation

### From Packages

Sway is available in many distributions. Try installing the "swaylock" package
for yours. If it's not available, check out [this wiki
page](https://github.com/swaywm/sway/wiki/Unsupported-packages) for information
on installation for your distributions.

If you're interested in packaging sway for your distribution, stop by the IRC
channel or shoot an email to sir@cmpwn.com for advice.

### Compiling from Source

Install dependencies:

* meson \*
* wayland
* wayland-protocols \*
* pango
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
