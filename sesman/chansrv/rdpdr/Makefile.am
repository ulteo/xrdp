
SUBDIRS = \
  printer

INCLUDES = \
  -I$(top_srcdir)/common

sbin_PROGRAMS = \
  vchannel_rdpdr

vchannel_rdpdr_SOURCES = \
  main.c

vchannel_rdpdr_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/common/libcommon.la 

rdpdrsysconfdir=$(sysconfdir)/xrdp

rdpdrsysconf_DATA = \
  rdpdr.conf  

EXTRA_DIST = \
  $(rdpdrsysconf_DATA)

