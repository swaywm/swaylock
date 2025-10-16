gnome = import('gnome')

dbus = dependency('dbus-1')
glib = dependency('glib-2.0', version: '>=2.64.0')
gio_dep = dependency('gio-2.0')

fprintd_dbus_interfaces = files(
    '/usr/share/dbus-1/interfaces/net.reactivated.Fprint.Manager.xml',
    '/usr/share/dbus-1/interfaces/net.reactivated.Fprint.Device.xml',
)

fprintd_dbus_sources = gnome.gdbus_codegen('fprintd-dbus',
    sources: fprintd_dbus_interfaces,
    autocleanup: 'all',
    interface_prefix: 'net.reactivated.Fprint.',
    namespace: 'FprintDBus',
    object_manager: true,
)

fingerprint = declare_dependency(
    include_directories: [
        include_directories('..'),
    ],
    sources: [
        fprintd_dbus_sources,
        'fingerprint.c' ,
    ],
    dependencies: [
        gio_dep,
        glib,
        dbus
    ],
)