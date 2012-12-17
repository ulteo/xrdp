
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -Wall -O2 -D_REENTRANT -DIMAGE_MAGICK_SUPPORT \
  `Wand-config --cflags`

AM_LDFLAGS = \
  $(filter-out -lglib-2.0, $(shell Wand-config --ldflags --libs))

INCLUDES = \
  -I$(top_srcdir)/common

pkglib_LTLIBRARIES = \
  libulteo.la

libulteo_la_SOURCES = \
  ulteo.c \
  ulteo.h

libvnc_la_LIBADD = \
  $(top_builddir)/common/libxrdp_common.la
