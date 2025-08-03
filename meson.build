project(
	'swaylock',
	'c',
	version: '1.8.3',
	license: 'MIT',
	meson_version: '>=0.59.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

cc = meson.get_compiler('c')

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

add_project_arguments(cc.get_supported_arguments([
	'-Wno-unused-parameter',
	'-Wno-unused-result',
	'-Wundef',
	'-Wvla',
]), language: 'c')

is_freebsd = host_machine.system().startswith('freebsd')
if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif

wayland_client = dependency('wayland-client', version: '>=1.20.0')
wayland_protos = dependency('wayland-protocols', version: '>=1.25', fallback: 'wayland-protocols')
wayland_scanner = dependency('wayland-scanner', version: '>=1.15.0', native: true)
xkbcommon = dependency('xkbcommon')
cairo = dependency('cairo')
gdk_pixbuf = dependency('gdk-pixbuf-2.0', required: get_option('gdk-pixbuf'))
libpam = cc.find_library('pam', required: get_option('pam'))
crypt = cc.find_library('crypt', required: not libpam.found())
math = cc.find_library('m')
rt = cc.find_library('rt')

git = find_program('git', required: false)
scdoc = find_program('scdoc', required: get_option('man-pages'))
wayland_scanner_prog = find_program(wayland_scanner.get_variable('wayland_scanner'), native: true)

version = meson.project_version()
if git.found()
	git_commit_hash = run_command([git, 'describe', '--always', '--tags'], check: false)
	git_branch = run_command([git, 'rev-parse', '--abbrev-ref', 'HEAD'], check: false)
	if git_commit_hash.returncode() == 0 and git_branch.returncode() == 0
		version = '@0@ (" __DATE__ ", branch \'@1@\')'.format(git_commit_hash.stdout().strip(), git_branch.stdout().strip())
	endif
endif

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_code = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

client_protocols = [
	wl_protocol_dir / 'staging/ext-session-lock/ext-session-lock-v1.xml',
]

protos_src = []
foreach xml : client_protocols
	protos_src += wayland_scanner_code.process(xml)
	protos_src += wayland_scanner_client.process(xml)
endforeach

conf_data = configuration_data()
conf_data.set_quoted('SYSCONFDIR', get_option('prefix') / get_option('sysconfdir'))
conf_data.set_quoted('SWAYLOCK_VERSION', version)
conf_data.set10('HAVE_GDK_PIXBUF', gdk_pixbuf.found())

subdir('include')

dependencies = [
	cairo,
	gdk_pixbuf,
	math,
	rt,
	xkbcommon,
	wayland_client,
]

sources = [
	'background-image.c',
	'cairo.c',
	'comm.c',
	'log.c',
	'loop.c',
	'main.c',
	'password.c',
	'password-buffer.c',
	'pool-buffer.c',
	'render.c',
	'seat.c',
	'unicode.c',
]

if libpam.found()
	sources += ['pam.c']
	dependencies += [libpam]
else
	warning('The swaylock binary must be setuid when compiled without libpam')
	warning('You must do this manually post-install: chmod a+s /path/to/swaylock')
	sources += ['shadow.c']
	dependencies += [crypt]
endif

swaylock_inc = include_directories('include')

executable('swaylock',
	sources + protos_src,
	include_directories: [swaylock_inc],
	dependencies: dependencies,
	install: true
)

if libpam.found()
	install_data(
		'pam/swaylock',
		install_dir: get_option('sysconfdir') / 'pam.d'
	)
endif

if scdoc.found()
	mandir = get_option('mandir')
	man_files = [
		'swaylock.1.scd',
	]
	foreach filename : man_files
		topic = filename.split('.')[-3].split('/')[-1]
		section = filename.split('.')[-2]
		output = '@0@.@1@'.format(topic, section)

		custom_target(
			output,
			input: filename,
			output: output,
			command: scdoc,
			feed: true,
			capture: true,
			install: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif

subdir('completions')
