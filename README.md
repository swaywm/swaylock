# swaylock

swaylock is a screen locking utility for Wayland compositors. It is compatible
with any Wayland compositor which implements the following Wayland protocols:

- wlr-layer-shell + wlr-input-inhibitor, or ext-session-lock-v1
- xdg-output

See the man page, `swaylock(1)`, for instructions on using swaylock.

## Release Signatures

Releases are signed with [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48)
and published [on GitHub](https://github.com/swaywm/swaylock/releases). swaylock
releases are managed independently of sway releases.

## Installation

### From Packages

Swaylock is available in many distributions. Try installing the "swaylock"
package for yours.

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

_\* Compile-time dep_  
_\*\* Optional: required for background images other than PNG_

Run these commands:

    meson build
    ninja -C build
    sudo ninja -C build install

On systems without PAM, you need to suid the swaylock binary:

    sudo chmod a+s /usr/local/bin/swaylock

Swaylock will drop root permissions shortly after startup.
