
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -Wall

INCLUDES = \
  -I$(top_srcdir)/common

pkglib_LTLIBRARIES = \
  lib_uc_proxy.la

lib_uc_proxy_la_SOURCES = \
  proxy.c \
  proxy.h \
  userChannel.c \
  userChannel.h

lib_uc_proxy_la_LIBADD = \
  $(top_builddir)/common/libxrdp_common.la
