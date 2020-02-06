# swaylock-effects

Swaylock-effects is a fork of [swaylock](https://github.com/swaywm/swaylock)
which adds built-in screenshots and image manipulation effects like blurring.
It's inspired by [i3lock-color](https://github.com/PandorasFox/i3lock-color),
although the feature sets aren't perfectly overlapping.

Versioning is based on upstream versioning; `v1.6-0` is the first release based
on upstream's `v1.6`, `v1.6-1` would be the next release based on
upstream's `v1.6`, etc.

![Screenshot](https://raw.githubusercontent.com/mortie/swaylock-effects/master/screenshot.png)

## Example Command

	swaylock \
		--screenshots \
		--clock \
		--indicator \
		--indicator-radius 100 \
		--indicator-thickness 7 \
		--effect-blur 7x5 \
		--ring-color bb00cc \
		--key-hl-color 880033 \
		--line-color 00000000 \
		--inside-color 00000088 \
		--separator-color 00000000

## New Features

The main new features compared to upstream swaylock are:

* `--screenshots` to use screenshots instead of an image on disk or a color
* `--clock` to show date/time in the indicator
	* Use `--indicator` to make the indicator always active
	* Use `--timestr` and `--datestr` to set the date/time formats
	  (using strftime-style formatting)
* Various effects which can be applied to the background image
	* `--effect-blur <radius>x<times>` blurs the image (thanks to yvbbrjdr's
	  fast box blur algorithm in
	  [i3lock-fancy-rapid](https://github.com/yvbbrjdr/i3lock-fancy-rapid))
	* `--effect-scale <scale>` scales the image by a factor. This can be used
	  to pixelate the image, or make other effects faster if you don't need
	  the full resolution.
	* `--effect-greyscale` makes the image greyscale.
	* `--effect-vignette <base>:<factor>` applies a vignette effect.
	* `--effect-custom <path>` loads a custom effect from a shared object.

New feature ideas are welcome as issues (though I may never get around to
implement them), new feature implementations are welcome as pull requests :)

## Installation

### From Packages

* Arch Linux (AUR): [swaylock-effects-git](https://aur.archlinux.org/packages/swaylock-effects-git/)
* Fedora (Copr): [swaylock-effects](https://copr.fedorainfracloud.org/coprs/eddsalkield/swaylock-effects/)
  (thanks to Edd Salkield)

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
* openmp (if using a compiler other than GCC)

_\*Compile-time dep_

_\*\*Optional: required for background images other than PNG_

Run these commands:

	meson build
	ninja -C build
	sudo ninja -C build install

On systems without PAM, you need to suid the swaylock binary:

	sudo chmod a+s /usr/local/bin/swaylock

Swaylock will drop root permissions shortly after startup.
