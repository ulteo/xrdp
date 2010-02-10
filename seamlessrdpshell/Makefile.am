
AM_CFLAGS = \
  -Wall -O2 -D_REENTRANT

INCLUDES = \
  -I$(top_srcdir)/common 

bin_PROGRAMS = \
  XHook

XHook_SOURCES = \
  main.c seamlessrdpshell.h \
  xutils.c xutils.h \
  user_channel.c userchannel.h

XHook_LDADD = \
  -lX11 -lpthread \
  $(top_builddir)/common/libcommon.la 

seamlessrdpshelldir=$(bindir)

seamlessrdpshell_DATA = \
  seamlessrdpshell

install-data-hook:
	chmod 755 $(DESTDIR)$(bindir)/seamlessrdpshell

