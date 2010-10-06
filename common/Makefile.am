
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DHAVE_ICONV \
  -DHAVE_ICONV_H

lib_LTLIBRARIES = \
  libxrdp_common.la

libxrdp_common_la_SOURCES = \
  xrdp_constants.h \
  arch.h defines.h parse.h \
  d3des.c d3des.h \
  file.c file.h file_loc.h \
  list.c list.h \
  log.c log.h \
  os_calls.c os_calls.h \
  ssl_calls.c ssl_calls.h \
  thread_calls.c thread_calls.h \
  trans.c trans.h \
  uni_rdp.c uni_rdp.h

libxrdp_common_la_LIBADD = \
  -lcrypto \
  -lpthread \
  -ldl
