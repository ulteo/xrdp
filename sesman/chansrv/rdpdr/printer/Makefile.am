
INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel \
  -I../

sbin_PROGRAMS = \
  rdpdr_printer

rdpdr_printer_SOURCES = \
  main.c

rdpdr_printer_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/vchannel/libvchannel.la \
  $(top_builddir)/common/libcommon.la 


