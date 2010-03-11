
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" 


INCLUDES = \
  -I$(top_srcdir)/common

sbin_PROGRAMS = \
  xrdp-chansrv 

xrdp_chansrv_SOURCES = \
  chansrv.h chansrv.c \
  user_channel.h user_channel.c 

xrdp_chansrv_LDADD = \
  $(top_builddir)/common/libxrdp_common.la 

chansrvsysconfdir=$(sysconfdir)/xrdp

chansrvsysconf_DATA = \
  vchannel.ini 

channel_launcherdir = $(sbindir)

channel_launcher_DATA = \
  xrdp-channel-launcher

SUBDIRS = \
  rdpdr \
  rdpsnd

install-data-hook:
	chmod 755 $(DESTDIR)$(sbindir)/xrdp-channel-launcher

EXTRA_DIST = \
  $(chansrvsysconf_DATA) \
  $(channel_launcher_DATA)
