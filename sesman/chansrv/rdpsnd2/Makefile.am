AM_CFLAGS = \
  `pkg-config --cflags libpulse-simple`

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel

sbin_PROGRAMS = \
  vchannel_rdpsnd

vchannel_rdpsnd_SOURCES = \
  config.h main.c \
  sound_channel.h sound_channel.c

vchannel_rdpsnd_LDADD = \
  $(top_builddir)/vchannel/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la

vchannel_rdpsnd_LDFLAGS = \
  `pkg-config --libs libpulse-simple`

alsaconfdir=$(sysconfdir)
alsaconf_DATA = \
  asound.conf

sndsysconfdir=$(sysconfdir)/xrdp
sndsysconf_DATA = \
  rdpsnd.conf rdpsnd.pa

EXTRA_DIST = \
  $(sndsysconf_DATA) \
  $(alsaconf_DATA)
