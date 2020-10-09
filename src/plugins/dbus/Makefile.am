plugindir = @NGFD_PLUGIN_DIR@
plugin_LTLIBRARIES = libngfd_dbus.la
libngfd_dbus_la_SOURCES = plugin.c
libngfd_dbus_la_LIBADD = @NGFD_PLUGIN_LIBS@ @DBUS_LIBS@
libngfd_dbus_la_LDFLAGS = -module -avoid-version $(top_srcdir)/dbus-gmain/libdbus-gmain.la
libngfd_dbus_la_CFLAGS = @NGFD_PLUGIN_CFLAGS@ @DBUS_CFLAGS@ -I$(top_srcdir)/src/include
