
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
  sound.c sound.h \
  clipboard.c clipboard.h \
  devredir.c devredir.h \
  seamrdp.c seamrdp.h \
  printer_dev.c printer_dev.h \
  user_channel.h user_channel.c \
  xutils.c

xrdp_chansrv_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/common/libcommon.la \
  -lX11 -lXfixes 

chansrvsysconfdir=$(sysconfdir)/xrdp

chansrvsysconf_DATA = \
  vchannel.ini 

EXTRA_DIST = \
  $(vchannelsysconf_DATA)
