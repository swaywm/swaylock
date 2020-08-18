i18n = import('i18n')

add_project_arguments(
	[
		'-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
		'-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir'))
	],
	language:'c'
)
i18n.gettext(meson.project_name(),
	args: [
		'--directory=' + meson.source_root(),
		'--package-name=' + meson.project_name(),
		'--package-version=' + meson.project_version(),
		'--msgid-bugs-address=https://github.com/swaywm/swaylock/issues',
	]
)
