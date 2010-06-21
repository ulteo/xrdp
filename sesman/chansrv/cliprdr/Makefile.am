
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -Wall -O2 -D_REENTRANT

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel

bin_PROGRAMS = \
  vchannel_cliprdr

vchannel_cliprdr_SOURCES = \
  main.c cliprdr.h \
  xutils.c xutils.h 
  

vchannel_cliprdr_LDADD = \
  -lX11 -lpthread \
  $(top_builddir)/vchannel/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la 

cliprdrsysconfdir=$(sysconfdir)/xrdp

cliprdrsysconf_DATA = \
  cliprdr.conf 

EXTRA_DIST = \
  $(cliprdrsysconf_DATA)

