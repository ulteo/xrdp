
SUBDIRS = \
  pam.d \
  Xserver

startscriptdir=$(sysconfdir)/xrdp

startscript_DATA = \
  xrdp.sh \
  km-0407.ini \
  km-0409.ini \
  km-040c.ini \
  km-0410.ini \
  km-0419.ini \
  km-041d.ini

startinitdir=$(sysconfdir)/init.d

startinit_DATA = \
  xrdp.sh

# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/init.d/xrdp.sh

EXTRA_DIST = \
  $(startscript_DATA)
