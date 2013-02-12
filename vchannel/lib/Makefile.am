AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -Wall

INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libxrdp_vchannel.la

libxrdp_vchannel_la_SOURCES = \
  vchannel.h vchannel.c 

libxrdp_vchannel_la_LIBADD = \
  -lpthread \
  -ldl \
  $(top_builddir)/common/libxrdp_common.la 


pkglib_LTLIBRARIES = \
  libxrdp_chansrv.la

libxrdp_chansrv_la_SOURCES = \
  user_channel.c user_channel.h

libxrdp_chansrv_la_LIBADD = \
  $(top_builddir)/common/libxrdp_common.la
