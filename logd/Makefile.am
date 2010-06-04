AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -Wall -O2 -D_REENTRANT \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common 

sbin_PROGRAMS = \
  xrdp-logd

xrdp_logd_SOURCES = \
  main.c  

xrdp_logd_LDADD = \
  -lpthread \
  $(top_builddir)/common/libxrdp_common.la 

xrdp_logddir=$(bindir)

xrdp_logd_DATA = \
  xrdp-logd

xrdp_logd_confdir=$(sysconfdir)/xrdp

xrdp_logd_conf_DATA = \
  xrdp-log.conf 

install-data-hook:
	chmod 755 $(DESTDIR)$(sbindir)/xrdp-logd

EXTRA_DIST = \
  $(xrdp_logd_DATA) \
  $(xrdp_logdconf_DATA)

