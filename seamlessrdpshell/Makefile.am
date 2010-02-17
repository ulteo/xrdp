
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -Wall -O2 -D_REENTRANT

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel

bin_PROGRAMS = \
  XHook

XHook_SOURCES = \
  main.c seamlessrdpshell.h \
  xutils.c xutils.h 

XHook_LDADD = \
  -lX11 -lpthread \
  $(top_builddir)/vchannel/libvchannel.la \
  $(top_builddir)/common/libcommon.la 

seamlessrdpshelldir=$(bindir)

seamlessrdpshell_DATA = \
  seamlessrdpshell

seamlessrdpshellconfdir=$(sysconfdir)/xrdp

seamlessrdpshellconf_DATA = \
  seamrdp.conf 

install-data-hook:
	chmod 755 $(DESTDIR)$(bindir)/seamlessrdpshell

EXTRA_DIST = \
  $(seamlessrdpshell_DATA) \
  $(seamlessrdpshellconf_DATA)

