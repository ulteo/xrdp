###############################################
Name: xrdp
###############################################

Summary: RDP server for Linux
Version: 1.0
Release: 1
License: GPL2
Vendor: Ulteo SAS
URL: http://www.ulteo.com
Packager: Samuel Bovée <samuel@ulteo.com>
Group: Applications/System
ExclusiveArch: x86_64
Buildroot: %{_tmppath}/%{name}-%{version}-root
Patch0: /ici/il/faut/le mettre
BuildRequires: cups-devel, libxml2-devel, libX11-devel, libXfixes-devel, openssl-devel, pam-devel, pulseaudio-lib-devel, libtool-ltdl-devel
Requires: python

%description
Xrdp is a RDP server for Linux. It provides remote display of a desktop and
many other features such as:
 * seamless display
 * printer and local device mapping

%changelog
* Mon Mar 26 2010 Samuel Bovée <samuel@ulteo.com> 1.0~svn00210
- Initial release
%prep
cd ..
svn up SOURCES/xrdp
svn export --force SOURCES/xrdp BUILD

%build
./autogen.sh
make

%install
rm -fr $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
sed '/^SESSION/c\SESSION="x-session-manager"' $RPM_BUILD_ROOT/etc/xrdp/startwm.sh -i
chmod 755 $RPM_BUILD_ROOT/etc/xrdp/startwm.sh

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/sbin/*
/usr/share/xrdp/*
/usr/lib/xrdp/libmc.so*
/usr/lib/xrdp/librdp.so*
/usr/lib/xrdp/libscp.so*
/usr/lib/xrdp/libvnc.so*
/usr/lib/xrdp/libxrdp.so*
/usr/lib/xrdp/libxup.so*
/usr/lib/*.so*
/usr/bin/logoff
/usr/bin/startapp
/usr/bin/xrdp-genkeymap
/usr/bin/xrdp-keygen
/usr/bin/xrdp-sesadmin
/usr/bin/xrdp-sesrun
/usr/bin/xrdp-sestest
%config /etc/xrdp/rdpdr.conf
%config /etc/xrdp/startwm.sh
%config /etc/xrdp/*.ini
%config /etc/xrdp/Xserver/*
%config /etc/pam.d/*
%config /etc/init.d/*
%doc /usr/share/man/man1/logoff.1.gz
%doc /usr/share/man/man1/rdpdr_printer.1.gz
%doc /usr/share/man/man1/startapp.1.gz
%doc /usr/share/man/man1/vchannel_rdpdr.1.gz
%doc /usr/share/man/man1/xrdp-*.1.gz
%doc /usr/share/man/man5/*
%doc /usr/share/man/man8/*
%define _unpackaged_files_terminate_build 0

%post
groupadd tsusers
LOG=/var/log/xrdp
if [ ! -d $LOG ]
then
    mkdir -p $LOG
fi
chgrp tsusers $LOG
SPOOL=/var/spool/xrdp
if [ ! -d $SPOOL ]
then
    mkdir -p $SPOOL
fi
chgrp tsusers $SPOOL

%postun
groupdel tsusers
rm -rf /var/log/xrdp /var/spool/xrdp

###########################################
%package seamrdp
###########################################

Summary: Seamless XRDP Shell
Group: Applications/System
Requires: xrdp

%description seamrdp
Seamlessrdpshell is a rdp addon offering the possibility to have an
application without a desktop

%files seamrdp
%defattr(-,root,root)
/usr/bin/seamlessrdpshell
/usr/bin/XHook
%config /etc/xrdp/seamrdp.conf
%doc /usr/share/man/man1/seamlessrdpshell.1.gz
%doc /usr/share/man/man1/XHook.1.gz

###########################################
%package printer
###########################################

Summary: cups file converter to ps format
Group: Applications/System
Requires: xrdp, ghostscript, cups

%description printer
Xrdpi-Printer convert a ps file from cups in ps

%files printer
%defattr(-,root,root)
%config /etc/cups/xrdp_printer.conf
/usr/lib/cups/backend/xrdpprinter
/usr/share/cups/model/PostscriptColor.ppd.gz

%post printer
mkdir -p /var/spool/xrdp_printer/SPOOL
chown -R lp /var/spool/xrdp_printer

###########################################
%package sound
###########################################

Summary: XRDP plugin for PulseAudio
Group: Applications/System
Requires: xrdp, pulseaudio, alsa-utils, alsa-lib

%description sound
This package contains the XRDP plugin for PulseAudio, a sound server for POSIX
and WIN32 systems

%files sound
%defattr(-,root,root)
#/etc/xrdp/rdpsnd.*
#/usr/lib/xrdp/librdpsnd.so*
