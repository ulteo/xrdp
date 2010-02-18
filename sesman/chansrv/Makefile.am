
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  `cups-config --cflags` 

AM_LDFLAGS = \
  `cups-config --libs`

INCLUDES = \
  -I$(top_srcdir)/common

sbin_PROGRAMS = \
  xrdp-chansrv

xrdp_chansrv_SOURCES = \
  chansrv.c chansrv.h \
  clipboard.c clipboard.h \
  user_channel.h user_channel.c 

xrdp_chansrv_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/common/libcommon.la \
  -lX11 -lXfixes 

chansrvsysconfdir=$(sysconfdir)/xrdp

chansrvsysconf_DATA = \
  vchannel.ini 

SUBDIRS = \
  rdpdr 

EXTRA_DIST = \
  $(chansrvsysconf_DATA)
