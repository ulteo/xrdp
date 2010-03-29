printerconfdir=$(sysconfdir)/cups
printerconf_DATA=xrdp_printer.conf

printerlibdir=$(libdir)/cups/backend
printerlib_DATA=xrdpprinter

printermodeldir=$(datarootdir)/cups/model/
printermodel_DATA=PostscriptColor.ppd.gz

install-data-hook:
	mkdir -p $(DESTDIR)$(localstatedir)/spool/xrdp_printer/SPOOL

EXTRA_DIST = \
  $(printerconf_DATA) \
  $(printerlib_DATA) \
  $(printermodel_DATA)
