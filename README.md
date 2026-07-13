# swaylock

swaylock is a screen locking utility for Wayland compositors. It is compatible
with any Wayland compositor which implements the ext-session-lock-v1 Wayland
protocol.

See the man page, [swaylock(1)](swaylock.1.scd), for instructions on using swaylock.

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
* fprintd
* dbus \*
* glib \*
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages) \*
* git \*

_\* Compile-time dep_
_\*\* Optional: required for background images other than PNG_

Run these commands:

    meson build
    ninja -C build
    sudo ninja -C build install

##### Without PAM

On systems without PAM, swaylock uses `shadow.h`.

Systems which rely on a tcb-like setup (either via musl's native support or via
glibc+[tcb]), require no further action.

[tcb]: https://www.openwall.com/tcb/

For most other systems, where passwords for all users are stored in `/etc/shadow`,
swaylock needs to be installed suid:

    sudo chmod a+s /usr/local/bin/swaylock

Optionally, on systems where the file `/etc/shadow` is owned by the `shadow`
group, the binary can be made sgid instead:

    sudo chgrp shadow /usr/local/bin/swaylock
    sudo chmod g+s /usr/local/bin/swaylock

Swaylock will drop root permissions shortly after startup.
