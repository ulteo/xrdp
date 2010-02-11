
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libvchannel.la

libvchannel_la_SOURCES = \
  vchannel.h vchannel.c 

libvchannel_la_LIBADD = \
  -lpthread \
  -ldl
