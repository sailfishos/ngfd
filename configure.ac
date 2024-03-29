AC_PREREQ(2.61)
AC_INIT(ngfd, 1.4.0)
AM_INIT_AUTOMAKE([subdir-objects])

AC_CONFIG_MACRO_DIR([m4])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([src/ngf/main.c])

# Disable static libraries.
AC_DISABLE_STATIC

# Initialize programs.

AM_PROG_AR

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

# Enable extra GCC flags.
if test "$GCC" = "yes"; then
    CFLAGS="$CFLAGS -Wall -Wextra"
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 localtime_r memmove memset socket strchr strdup strerror strrchr strtoul])

# Checks for glib and gobject.
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.40.0
                        gobject-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# Checks for gmodule.
PKG_CHECK_MODULES(GMODULE, gmodule-2.0)
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

# Checks for PulseAudio.
PKG_CHECK_MODULES(PULSE, libpulse libpulse-mainloop-glib)
AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)

# Checks for check unit test framework.
PKG_CHECK_MODULES(CHECK, check)
AC_SUBST(CHECK_LIBS)
AC_SUBST(CHECK_CFLAGS)

# for sd_notify()
AC_ARG_ENABLE([systemd],
    AS_HELP_STRING([--disable-systemd],[Disable optional systemd integration]))

AS_IF([test "x$enable_systemd" != "xno"],
    [PKG_CHECK_MODULES(SYSTEMD, [libsystemd], HAVE_SYSTEMD=1, HAVE_SYSTEMD=0)],
    HAVE_SYSTEMD=0)

AS_IF([test "x$enable_systemd" = "xyes" && test "x$HAVE_SYSTEMD" = "x0"],
    [AC_MSG_ERROR([*** Needed systemd daemon support not found])])

AC_SUBST(SYSTEMD_LIBS)
AC_SUBST(SYSTEMD_CFLAGS)

AM_CONDITIONAL(HAVE_SYSTEMD, test x$HAVE_SYSTEMD = x1)
AS_IF([test "x$HAVE_SYSTEMD" = "x1"], AC_DEFINE([HAVE_SYSTEMD], 1, [Have SYSTEMD?]))

if test x$HAVE_SYSTEMD = x1; then
    enable_systemd=yes
else
    enable_systemd=no
fi

AC_ARG_ENABLE([coverage],
     	AS_HELP_STRING([--enable-coverage],[Enable coverage @<:@default=false@:>@]),
	[case "${enableval}" in
		yes) coverage=true ; CHECK_CFLAGS="${CHECK_CFLAGS} -fprofile-arcs -ftest-coverage" ; CHECK_LIBS="${CHECK_LIBS} -lgcov" ;;
		no)  coverage=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-coverage]) ;;
	esac],
	[coverage=false])
AM_CONDITIONAL([COVERAGE], [test x$coverage = xtrue])

#AC_SUBST(CHECK_LIBS)
#AC_SUBST(CHECK_CFLAGS)


# NGFD build compatibility variables.
AC_SUBST(NGFD_CFLAGS, "$GLIB_CFLAGS $GMODULE_CFLAGS")
AC_SUBST(NGFD_LIBS, "$GLIB_LIBS $GMODULE_LIBS")
AC_SUBST(NGFD_PLUGIN_CFLAGS, "$NGFD_CFLAGS")
AC_SUBST(NGFD_PLUGIN_LIBS, "$NGFD_LIBS")

AC_SUBST(NGFD_CONF_DIR, "\$(datadir)/ngfd")
AC_SUBST(NGFD_PLUGIN_CONF_DIR, "\$(NGFD_CONF_DIR)/plugins.d")
AC_SUBST(NGFD_PLUGIN_DIR, "\$(libdir)/ngf")

AC_SUBST(NGFD_TESTS_DIR, "/opt/tests/ngfd")

# Debug flag.

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug],[Enable debug @<:@default=false@:>@]),
    [case "${enableval}" in
        yes) debug=true ; CFLAGS="${CFLAGS} -DENABLE_DEBUG" ;;
        no)  debug=false ;;
        *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
    esac],
    [debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

# DBus plugin

PKG_CHECK_MODULES(DBUS, dbus-1 >= 1.8, [has_dbus=yes], [has_dbus=no])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

if test x$has_dbus = xyes; then
    enable_dbus=yes
else
    enable_dbus=no
fi

AM_CONDITIONAL(BUILD_DBUS, test x$enable_dbus = xyes)

# FF memless plugin
AC_CHECK_HEADERS([linux/input.h sys/ioctl.h], [has_ffm=yes], [has_ffm=no])
AC_SUBST(FFMEMLESS_CFLAGS, "")
AC_SUBST(FFMEMLESS_LIBS, "")

if test x$has_ffm = xyes; then
    enable_ffm=yes
else
    enable_ffm=no
fi
AM_CONDITIONAL(BUILD_FFMEMLESS, test x$enable_ffm = xyes)

# GStreamer plugin

PKG_CHECK_MODULES(GST, gstreamer-1.0 gstreamer-controller-1.0, [has_gst=yes], [has_gst=no])
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

if test x$has_gst = xyes; then
    enable_gst=yes
else
    enable_gst=no
fi

AM_CONDITIONAL(BUILD_GST, test x$enable_gst = xyes)

# libcanberra plugin
PKG_CHECK_MODULES(CANBERRA, libcanberra, [has_canberra=yes], [has_canberra=no])
AC_SUBST(CANBERRA_CFLAGS)
AC_SUBST(CANBERRA_LIBS)

if test x$has_canberra = xyes; then
    enable_canberra=yes
else
    enable_canberra=no
fi

AM_CONDITIONAL(BUILD_CANBERRA, test x$enable_canberra = xyes)

# Immvibe plugin

AC_CHECK_HEADERS([ImmVibe.h ImmVibeCore.h ImmVibeOS.h], [has_immvibe=yes], [has_immvibe=no])
AC_SUBST(IMMVIBE_CFLAGS, "")
AC_SUBST(IMMVIBE_LIBS, "-limmvibe")

if test x$has_immvibe = xyes; then
    enable_immvibe=yes
else
    enable_immvibe=no
fi

AM_CONDITIONAL(BUILD_IMMVIBE, test x$enable_immvibe = xyes)

# Profile plugin

PKG_CHECK_MODULES(PROFILE, profile, [has_profile=yes], [has_profile=no])
AC_SUBST(PROFILE_CFLAGS)
AC_SUBST(PROFILE_LIBS)

if test x$has_profile = xyes; then
    enable_profile=yes
else
    enable_profile=no
fi

AM_CONDITIONAL(BUILD_PROFILE, test x$enable_profile = xyes)

# MCE plugin

PKG_CHECK_MODULES(MCE, mce, [has_mce=yes], [has_mce=no])

if test x$has_mce = xyes; then
    enable_mce=yes
else
    enable_mce=no
fi

AM_CONDITIONAL(BUILD_MCE, test x$enable_mce = xyes)

PKG_CHECK_MODULES(ROUTE, ohm-ext-route >= 2, [has_route=yes], [has_route=no])
AC_SUBST(ROUTE_CFLAGS)

if test x$has_route = xyes; then
    enable_route=yes
else
    enable_route=no
fi

AM_CONDITIONAL(BUILD_ROUTE, test x$enable_route = xyes)

# Stream restore plugin needs DBus and Route

if test x$enable_dbus = xyes ; then
    enable_streamrestore=yes
else
    enable_streamrestore=no
fi

if test x$enable_route = xno ; then
    enable_streamrestore=no
fi


AM_CONDITIONAL(BUILD_STREAMRESTORE, test x$enable_streamrestore = xyes)

# Tonegen plugin needs DBus

if test x$enable_dbus = xyes ; then
    enable_tonegen=yes
else
    enable_tonegen=no
fi

AM_CONDITIONAL(BUILD_TONEGEN, test x$enable_tonegen = xyes)

# Device lock plugin needs DBus

if test x$enable_dbus = xyes ; then
    enable_devicelock=yes
else
    enable_devicelock=no
fi

AM_CONDITIONAL(BUILD_DEVICELOCK, test x$enable_devicelock = xyes)

echo "
 == $PACKAGE_NAME $VERSION ==

    Compiler:               ${CC}
    CFLAGS:                 ${CFLAGS}
    Code coverage:          ${coverage}
    Debug enabled:          ${debug}

    Systemd integration     ${enable_systemd}

    DBus plugin:            ${enable_dbus}
    Device lock plugin:     ${enable_devicelock}
    GStreamer plugin:       ${enable_gst}
    Canberra plugin:        ${enable_canberra}
    Immvibe plugin:         ${enable_immvibe}
    Profile plugin:         ${enable_profile}
    MCE plugin:             ${enable_mce}
    Stream restore plugin:  ${enable_streamrestore}
    Tone generator plugin:  ${enable_tonegen}
    Route plugin:           ${enable_route}
"

AC_CONFIG_FILES([
Makefile
dbus-gmain/Makefile
src/Makefile
src/ngf/Makefile
src/include/Makefile
src/include/ngf/Makefile
src/plugins/Makefile
src/plugins/fake/Makefile
src/plugins/dbus/Makefile
src/plugins/ffmemless/Makefile
src/plugins/transform/Makefile
src/plugins/mce/Makefile
src/plugins/immvibe/Makefile
src/plugins/resource/Makefile
src/plugins/profile/Makefile
src/plugins/streamrestore/Makefile
src/plugins/tonegen/Makefile
src/plugins/gst/Makefile
src/plugins/canberra/Makefile
src/plugins/callstate/Makefile
src/plugins/devicelock/Makefile
src/plugins/route/Makefile
src/plugins/null/Makefile
doc/Makefile
data/Makefile
data/events.d/Makefile
data/plugins.d/Makefile
data/ngf-plugin.pc
tests/Makefile
])

AC_OUTPUT
