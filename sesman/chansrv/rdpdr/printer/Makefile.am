AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -Wall -O2 -D_REENTRANT \
  `cups-config --cflags`

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel \
  -I$(top_srcdir)/sesman/chansrv/rdpdr

AM_LDFLAGS = \
  `cups-config --libs`

sbin_PROGRAMS = \
  rdpdr_printer

rdpdr_printer_SOURCES = \
  main.c \
  printer_dev.h printer_dev.c 

rdpdr_printer_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/vchannel/libvchannel.la \
  $(top_builddir)/common/libcommon.la 


