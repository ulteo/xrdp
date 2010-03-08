xrdplibdir=$(libdir)/xrdp

AM_CFLAGS = \
  -DHAVE_CONFIG_H

INCLUDES = \
  -Ipulse_headers \
  -I$(top_builddir)/common \
  -I$(top_builddir)/vchannel

pkglib_LTLIBRARIES = \
  librdpsnd.la

librdpsnd_la_SOURCES = \
  module-rdp-sink-symdef.h module-rdp-sink.c \
  sound_channel.h sound_channel.c \
  pulse_headers/pulsecore/asyncmsgq.h pulse_headers/pulsecore/gccmacro.h \
  pulse_headers/pulsecore/memblockq.h pulse_headers/pulsecore/object.h \ 
  pulse_headers/pulsecore/sink-input.h pulse_headers/pulsecore/memblock.h \  
  pulse_headers/pulsecore/asyncq.h pulse_headers/pulsecore/hashmap.h \
  pulse_headers/pulsecore/memchunk.h pulse_headers/pulsecore/once.h \
  pulse_headers/pulsecore/source.h pulse_headers/pulsecore/native-common.h \
  pulse_headers/pulsecore/atomic.h pulse_headers/pulsecore/hook-list.h \ 
  pulse_headers/pulsecore/modargs.h pulse_headers/pulsecore/queue.h \
  pulse_headers/pulsecore/thread.h pulse_headers/pulsecore/sink.h \
  pulse_headers/pulsecore/client.h pulse_headers/pulsecore/idxset.h \
  pulse_headers/pulsecore/modinfo.h pulse_headers/pulsecore/refcnt.h \
  pulse_headers/pulsecore/thread-mq.h pulse_headers/pulsecore/core-def.h \
  pulse_headers/pulsecore/llist.h pulse_headers/pulsecore/module.h \
  pulse_headers/pulsecore/resampler.h pulse_headers/pulsecore/core.h \
  pulse_headers/pulsecore/log.h pulse_headers/pulsecore/msgobject.h \
  pulse_headers/pulsecore/rtclock.h pulse_headers/pulsecore/core-subscribe.h \
  pulse_headers/pulsecore/macro.h pulse_headers/pulsecore/mutex.h \
  pulse_headers/pulsecore/rtpoll.h pulse_headers/pulsecore/fdsem.h



librdpsnd_la_LIBADD = \
  $(top_builddir)/vchannel/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la 

sndsysconfdir=$(sysconfdir)/xrdp

sndsysconf_DATA = \
  rdpsnd.conf \
  rdpsnd.pa

# must be tab below
install-data-hook:
	pulsemoddir=$(DESTDIR)/usr/lib/pulse-0.9/modules; \
	mkdir -p $$pulsemoddir; \
	ln -sf $(DESTDIR)$(xrdplibdir)/librdpsnd.so.0.0.0  $$pulsemoddir/module-rdp-sink.so
