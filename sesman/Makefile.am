
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  `xml2-config --libs`

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman \
  -I$(top_srcdir)/sesman/libscp \
  `xml2-config --cflags`

if SESMAN_NOPAM
AUTH_C = verify_user.c
AUTH_LIB = -lcrypt
else
if SESMAN_PAMUSERPASS
AUTH_C = verify_user_pam_userpass.c
AUTH_LIB = -lpam -lpam_userpass
else
if SESMAN_KERBEROS
AUTH_C = verify_user_kerberos.c
AUTH_LIB = -lkrb5
else
AUTH_C = verify_user_pam.c
AUTH_LIB = -lpam
endif
endif
endif

sbin_PROGRAMS = \
  xrdp-sesman

xrdp_sesman_SOURCES = \
  scp.c scp.h \
  scp_v0.c scp_v0.h \
  scp_v1.c scp_v1.h \
  scp_v1_mng.c scp_v1_mng.h \
  sesman.c sesman.h \
  session.c session.h \
  sig.c sig.h \
  thread.c thread.h \
  lock.c lock.h \
  access.c access.h \
  config.c config.h \
  env.c env.h \
  $(AUTH_C) \
  auth.h

xrdp_sesman_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/sesman/libscp/libscp.la \
  $(AUTH_LIB)

sesmansysconfdir=$(sysconfdir)/xrdp

sesmansysconf_DATA = \
  sesman.ini \
  startwm.sh

SUBDIRS = \
  libscp \
  tools \
  sessvc \
  chansrv

# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/xrdp/startwm.sh

EXTRA_DIST = \
  $(sesmansysconf_DATA)
