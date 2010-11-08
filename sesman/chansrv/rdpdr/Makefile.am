AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -Wall -O2 -D_REENTRANT \
  `pkg-config fuse --cflags` \
  `xml2-config --cflags`

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel \
  -I$(top_srcdir)/sesman/chansrv/rdpdr

AM_LDFLAGS = \
  `pkg-config fuse --libs` \
  `xml2-config --libs`

sbin_PROGRAMS = \
  vchannel_rdpdr

vchannel_rdpdr_SOURCES = \
  main.c  rdpdr.h\
  shares.c shares.h \
  printers.c printers.h \
  fuse_dev.c fuse_dev.h \
  hashmap.c hashmap.h \
  rdpfs_cache.c rdpfs_cache.h \
  rdpfs.h rdpfs.c 

vchannel_rdpdr_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/vchannel/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la 

rdpdrsysconfdir=$(sysconfdir)/xrdp

rdpdrsysconf_DATA = \
  rdpdr.conf  

EXTRA_DIST = \
  $(rdpdrsysconf_DATA)
