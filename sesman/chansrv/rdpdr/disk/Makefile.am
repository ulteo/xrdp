AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  `pkg-config fuse --cflags`

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel \
  -I$(top_srcdir)/sesman/chansrv/rdpdr

AM_LDFLAGS = \
  `pkg-config fuse --libs`

sbin_PROGRAMS = \
  rdpdr_disk

rdpdr_disk_SOURCES = \
  main.c \
  disk_dev.c disk_dev.h \
  rdpfs.c

rdpdr_disk_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/vchannel/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la 


