plugindir = @NGFD_PLUGIN_DIR@
plugin_LTLIBRARIES = libngfd_tonegen.la
libngfd_tonegen_la_SOURCES = plugin.c dbusif.c ausrv.c stream.c tone.c envelop.c indicator.c \
                             dtmf.c rfc4733.c ngfif.c
libngfd_tonegen_la_LIBADD = @NGFD_PLUGIN_LIBS@ @DBUS_LIBS@ @PULSE_LIBS@
libngfd_tonegen_la_LDFLAGS = -module -avoid-version
libngfd_tonegen_la_CFLAGS = @NGFD_PLUGIN_CFLAGS@ @DBUS_CFLAGS@ @PULSE_CFLAGS@ -I$(top_srcdir)/src/include
