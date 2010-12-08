
SUBDIRS = \
  pam.d \
  Xserver

kminidir=$(sysconfdir)/xrdp

kmini_DATA = \
  km-0401.ini \
  km-0406.ini \
  km-0407.ini \
  km-0409.ini \
  km-040a.ini \
  km-040b.ini \
  km-040c.ini \
  km-0410.ini \
  km-0411.ini \
  km-0414.ini \
  km-0415.ini \
  km-0416.ini \
  km-0419.ini \
  km-041a.ini \
  km-041d.ini \
  km-041f.ini \
  km-0424.ini \
  km-0426.ini \
  km-0427.ini \
  km-042f.ini \
  km-0809.ini \
  km-080c.ini \
  km-0816.ini
  
                
startinitdir=$(sysconfdir)/init.d

startinit_DATA = \
  xrdp.sh

# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/init.d/xrdp.sh
	mv $(DESTDIR)$(sysconfdir)/init.d/xrdp.sh $(DESTDIR)$(sysconfdir)/init.d/xrdp

EXTRA_DIST = \
  $(kmini_DATA) \
  $(startinit_DATA)


