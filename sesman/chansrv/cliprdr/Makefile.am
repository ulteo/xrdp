
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -Wall -O2 -D_REENTRANT

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel

bin_PROGRAMS = \
  XHook

XHook_SOURCES = \
  main.c \
  xutils.c xutils.h 

XHook_LDADD = \
  -lX11 -lpthread \
  $(top_builddir)/vchannel/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la 

seamlessrdpshelldir=$(bindir)

seamlessrdpshell_DATA = \
  seamlessrdpshell

seamlessrdpshellconfdir=$(sysconfdir)/xrdp

seamlessrdpshellconf_DATA = \
  seamrdp.conf 

EXTRA_DIST = \
  $(seamlessrdpshell_DATA) \
  $(seamlessrdpshellsysconf_DATA)

