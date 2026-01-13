# swaylock

swaylock is a screen locking utility for Wayland compositors. It is compatible
with any Wayland compositor which implements the ext-session-lock-v1 Wayland
protocol.

This fork adds **animated GIF support** for background images. You can use GIF
files as your lock screen background, and they will animate at the appropriate
frame rate.

See the man page, [swaylock(1)](swaylock.1.scd), for instructions on using swaylock.

## Features

- All standard swaylock features
- **Animated GIF backgrounds**: Use `-i /path/to/animation.gif` to set an animated
  GIF as your lock screen background
- Per-output GIF support: Different animated backgrounds on different monitors

### Example Usage

```
# Use an animated GIF as background
swaylock -i ~/wallpapers/animation.gif

# Use different GIFs for different outputs
swaylock -i eDP-1:~/wallpapers/laptop.gif -i HDMI-A-1:~/wallpapers/monitor.gif
```

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
* gdk-pixbuf2 \*\* (required for GIF animation support)
* pam (optional)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages) \*
* git \*

_\* Compile-time dep_  
_\*\* Optional: required for background images other than PNG, and for animated GIF support_

Run these commands:

    meson build
    ninja -C build
    sudo ninja -C build install

##### PAM Configuration

When building with PAM support (the default when libpam is available), swaylock
requires a PAM configuration file at `/etc/pam.d/swaylock`. This file is
installed automatically when running `sudo ninja -C build install`.

**Arch Linux users:** If authentication fails even with the correct password,
the default PAM configuration (`auth include login`) may not work properly.
Replace `/etc/pam.d/swaylock` with the following:

```
#%PAM-1.0
auth       include      system-auth
account    include      system-auth
password   include      system-auth
session    include      system-auth
```

You can test PAM authentication without locking your screen by creating a small
test program or by running swaylock from a second TTY (switch with `Ctrl+Alt+F2`)
so you can kill it if needed (`pkill swaylock`).

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
