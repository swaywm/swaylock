swaylock(1)

# NAME

swaylock - Screen locker for Wayland

# SYNOPSIS

_swaylock_ [options...]

Locks your Wayland session.

# OPTIONS

*-C, --config* <path>
	The config file to use. By default, the following paths are checked:
	_$HOME/.swaylock/config_, _$XDG\_CONFIG\_HOME/swaylock/config_, and
	_SYSCONFDIR/swaylock/config_. All flags aside from this one are valid
	options in the configuration file using the format _long-option=value_.
	For options such as _ignore-empty-password_, just supply the _long-option_.
	All leading dashes should be omitted and the equals sign is required for
	flags that take an argument.

*-d, --debug*
	Enable debugging output.

*-e, --ignore-empty-password*
	When an empty password is provided, do not validate it.

*-F, --show-failed-attempts*
	Show current count of failed authentication attempts.

*-f, --daemonize*
	Detach from the controlling terminal after locking.

	Note: this is the default behavior of i3lock.

*-R, --ready-fd* <fd>
	File descriptor to send readiness notifications to.

	When the session has been locked, a single newline is written to the FD.
	At this point, the compositor guarantees that no security sensitive content
	is visible on-screen.

*-h, --help*
	Show help message and quit.

*-v, --version*
	Show the version number and quit.

# APPEARANCE

*-u, --no-unlock-indicator*
	Disable the unlock indicator.

*-i, --image* [[<output>]:]<path>
	Display the given image, optionally only on the given output. Use -c to set
	a background color. If the path potentially contains a ':', prefix it with another
	':' to prevent interpreting part of it as <output>.

*-k, --show-keyboard-layout*
	Display the current xkb layout while typing.

*-K, --hide-keyboard-layout*
	Force hiding the current xkb layout while typing, even if more than one layout
	is configured or the show-keyboard-layout option is set.

*-L, --disable-caps-lock-text*
	Disable the Caps Lock text.

*-l, --indicator-caps-lock*
	Show the current Caps Lock state also on the indicator.

*-s, --scaling*
	Image scaling mode: _stretch_, _fill_, _fit_, _center_, _tile_,
	_solid\_color_. Use _solid\_color_ to display only the background color, even
	if a background image is specified.

*-t, --tiling*
	Same as --scaling=tile.

*-c, --color* <rrggbb[aa]>
	Turn the screen into the given color instead of white. If -i is used, this
	sets the background of the image to the given color. Defaults to white
	(FFFFFF).

*--bs-hl-color* <rrggbb[aa]>
	Sets the color of backspace highlight segments.

*--caps-lock-bs-hl-color* <rrggbb[aa]>
	Sets the color of backspace highlight segments when Caps Lock is active.

*--caps-lock-key-hl-color* <rrggbb[aa]>
	Sets the color of the key press highlight segments when Caps Lock is active.

*--font* <font>
	Sets the font of the text.

*--font-size* <size>
	Sets a fixed font size for the indicator text.

*--indicator-idle-visible*
	Sets the indicator to show even if idle.

*--indicator-radius* <radius>
	Sets the indicator radius. The default value is 50.

*--indicator-thickness* <thickness>
	Sets the indicator thickness. The default value is 10.

*--indicator-x-position* <x>
	Sets the horizontal position of the indicator.

*--indicator-y-position* <y>
	Sets the vertical position of the indicator.

*--inside-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator.

*--inside-clear-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when cleared.

*--inside-caps-lock-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when Caps Lock is active.

*--inside-ver-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when verifying.

*--inside-wrong-color* <rrggbb[aa]>
	Sets the color of the inside of the indicator when invalid.

*--key-hl-color* <rrggbb[aa]>
	Sets the color of the key press highlight segments.

*--layout-bg-color* <rrggbb[aa]>
	Sets the background color of the box containing the layout text.

*--layout-border-color* <rrggbb[aa]>
	Sets the color of the border of the box containing the layout text.

*--layout-text-color* <rrggbb[aa]>
	Sets the color of the layout text.

*--line-color* <rrggbb[aa]>
	Sets the color of the line between the inside and ring.

*--line-clear-color* <rrggbb[aa]>
	Sets the color of the line between the inside and ring when cleared.

*--line-caps-lock-color* <rrggbb[aa]>
	Sets the color of the line between the inside and ring when Caps Lock is
	active.

*--line-ver-color* <rrggbb[aa]>
	Sets the color of the line between the inside and ring when verifying.

*--line-wrong-color* <rrggbb[aa]>
	Sets the color of the line between the inside and ring when invalid.

*-n, --line-uses-inside*
	Use the inside color for the line between the inside and ring.

*-r, --line-uses-ring*
	Use the ring color for the line between the inside and ring.

*--ring-color* <rrggbb[aa]>
	Sets the color of the ring of the indicator when typing or idle.

*--ring-clear-color* <rrggbb[aa]>
	Sets the color of the ring of the indicator when cleared.

*--ring-caps-lock-color* <rrggbb[aa]>
	Sets the color of the ring of the indicator when Caps Lock is active.

*--ring-ver-color* <rrggbb[aa]>
	Sets the color of the ring of the indicator when verifying.

*--ring-wrong-color* <rrggbb[aa]>
	Sets the color of the ring of the indicator when invalid.

*--separator-color* <rrggbb[aa]>
	Sets the color of the lines that separate highlight segments.

*--text-color* <rrggbb[aa]>
	Sets the color of the text.

*--text-clear-color* <rrggbb[aa]>
	Sets the color of the text when cleared.

*--text-caps-lock-color* <rrggbb[aa]>
	Sets the color of the text when Caps Lock is active.

*--text-ver-color* <rrggbb[aa]>
	Sets the color of the text when verifying.

*--text-wrong-color* <rrggbb[aa]>
	Sets the color of the text when invalid.

# SIGNALS

*SIGUSR1*
	Unlock the screen and exit.

# AUTHORS

Maintained by Drew DeVault <sir@cmpwn.com>, who is assisted by other open
source contributors. For more information about swaylock development, see
https://github.com/swaywm/swaylock.
