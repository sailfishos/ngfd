bin_PROGRAMS = ngfd

ngfd_CFLAGS = $(NGFD_CFLAGS) $(DBUS_CFLAGS) -I$(top_srcdir)/src/include -DDEFAULT_PLUGIN_PATH=@NGFD_PLUGIN_DIR@
ngfd_LDFLAGS = $(NGFD_LIBS) $(DBUS_LIBS) -lrt \
	$(top_srcdir)/dbus-gmain/libdbus-gmain.la

if HAVE_SYSTEMD
ngfd_CFLAGS += $(SYSTEMD_CFLAGS)
ngfd_LDFLAGS += $(SYSTEMD_LIBS)
endif

ngfd_SOURCES =                \
    main.c                    \
    plugin-internal.h         \
    plugin.h                  \
    plugin.c                  \
    core-internal.h           \
    core.h                    \
    core.c                    \
    core-hooks.h              \
    core-hooks.c              \
    haptic-internal.h         \
    haptic.c                  \
    hook.h                    \
    hook.c                    \
    core-player.h             \
    core-player.c             \
    context-internal.h        \
    context.h                 \
    context.c                 \
    inputinterface-internal.h \
    inputinterface.h          \
    inputinterface.c          \
    sinkinterface-internal.h  \
    sinkinterface.h           \
    sinkinterface.c           \
    value.h                   \
    value.c                   \
    proplist.h                \
    proplist.c                \
    eventlist-internal.h      \
    eventlist.c               \
    event-internal.h          \
    event.h                   \
    event.c                   \
    eventrule-internal.h      \
    eventrule.c               \
    request-internal.h        \
    request.h                 \
    request.c                 \
    core-dbus-internal.h      \
    core-dbus.c               \
    log.h                     \
    log.c
