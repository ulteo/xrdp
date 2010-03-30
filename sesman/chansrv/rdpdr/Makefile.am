AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -Wall -O2 -D_REENTRANT

SUBDIRS = \
  printer \
  disk

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel

sbin_PROGRAMS = \
  vchannel_rdpdr

vchannel_rdpdr_SOURCES = \
  rdpdr.h main.c

vchannel_rdpdr_LDADD = \
  $(top_builddir)/vchannel/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la 

rdpdrsysconfdir=$(sysconfdir)/xrdp

rdpdrsysconf_DATA = \
  rdpdr.conf  

EXTRA_DIST = \
  $(rdpdrsysconf_DATA)
