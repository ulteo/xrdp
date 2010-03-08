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
  sound_channel.h sound_channel.c

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
