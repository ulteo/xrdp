# Copyright (C) 2010-2013 Ulteo SAS
# http://www.ulteo.com
# Author Samuel BOVEE <samuel@ulteo.com> 2010-2011
# Author David PHAM-VAN <d.pham-van@ulteo.com> 2012-2013
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

Name: xrdp
Version: @VERSION@
Release: @RELEASE@

Summary: RDP server for Linux
License: GPL2
Group: Applications/System
Vendor: Ulteo SAS
URL: http://www.ulteo.com
Packager: David PHAM-VAN <d.pham-van@ulteo.com>

Source: %{name}-%{version}.tar.gz
%if %{defined rhel}
ExclusiveArch: i386 x86_64
BuildRequires: libtool, gcc, libxml2-devel, xorg-x11-server-devel, openssl-devel, pam-devel, pulseaudio-libs-devel, cups-devel, fuse-devel, libXfixes-devel, libtool-ltdl-devel
Requires: python, ulteo-ovd-vnc-server, cups-libs, libcom_err, libgcrypt, gnutls, krb5-libs, pam, openssl, xorg-x11-server-Xorg, xorg-x11-server-utils, libxml2, zlib, lsb
%else
ExclusiveArch: i586 x86_64
BuildRequires: libtool, gcc, libxml2-devel, xorg-x11-libX11-devel, xorg-x11-libXfixes-devel, openssl-devel, pam-devel, pulseaudio-devel, cups-devel, fuse-devel, scim-devel
Requires: python, ulteo-ovd-vnc-server, cups-libs, libcom_err2, libgcrypt11, libgnutls26, krb5, pam, libopenssl1_0_0, xorg-x11-libX11, libxml2, zlib, xkeyboard-config
# xkeyboard-config [Bug 730027] New: xorg-x11-Xvnc: should depend on xkeyboard-config
%endif

%description
Xrdp is a RDP server for Linux. It provides remote display of a desktop and
many other features such as:
 * seamless display
 * printer and local device mapping

%changelog
* Wed Dec 19 2012 David PHAM-VAN <d.pham-van@ulteo.com> 99.99
- Initial release

%prep
%setup -q

%build
ARCH=$(getconf LONG_BIT)
if [ "$ARCH" = "32" ]; then
    LIBDIR=/usr/lib
elif [ "$ARCH" = "64" ]; then
    LIBDIR=/usr/lib64
fi
%if %{defined rhel}
./configure --mandir=/usr/share/man --libdir=$LIBDIR --disable-scim
%else
./configure --mandir=/usr/share/man --libdir=$LIBDIR
%endif
make -j 4
%{__python} setup.py build

%install
rm -fr %{buildroot}
make install DESTDIR=%{buildroot}
%{__python} setup.py install --prefix=%{_prefix} --root=%{buildroot} --record=INSTALLED_FILES
mkdir -p %{buildroot}/var/log/xrdp %{buildroot}/var/spool/xrdp
%if %{defined rhel}
install -D instfiles/init/redhat/xrdp %{buildroot}/etc/init.d/xrdp
mv %{buildroot}/etc/asound.conf %{buildroot}/etc/xrdp/
%else
install -D instfiles/init/suse/xrdp %{buildroot}/etc/init.d/xrdp
%endif

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%config /etc/xrdp/xrdp-log.conf
%config /etc/xrdp/startwm.sh
%config /etc/xrdp/*.ini
%config /etc/xrdp/Xserver/*
%config /etc/pam.d/*
%config /etc/init.d/*
/usr/lib*/xrdp/libmc.so*
/usr/lib*/xrdp/librdp.so*
/usr/lib*/xrdp/libscp.so*
/usr/lib*/xrdp/libvnc.so*
/usr/lib*/xrdp/libxrdp.so*
/usr/lib*/xrdp/libxup.so*
/usr/lib*/xrdp/lib_uc_proxy.so*
/usr/lib*/xrdp/lib_uc_advance.so*
/usr/lib*/*.so*
/usr/sbin/xrdp*
/usr/share/xrdp/*
/usr/bin/xrdp-disconnect
/usr/bin/xrdp-genkeymap
/usr/bin/xrdp-keygen
/usr/bin/xrdp-logoff
/usr/bin/xrdp-sesadmin
/usr/bin/xrdp-sesrun
/usr/bin/xrdp-sestest
%doc /usr/share/man/man1/xrdp-*.1.gz
%doc /usr/share/man/man5/*
%doc /usr/share/man/man8/*
%dir /var/log/xrdp
%dir /var/spool/xrdp


%post
getent group tsusers >/dev/null || groupadd tsusers
chgrp tsusers /var/spool/xrdp

ldconfig
chkconfig --add xrdp > /dev/null
service xrdp start

%if %{undefined rhel}
# Hack because opensuse is unable to use the Xrdp rpath attribute correctly on OpenSUSE...
%{__ln_s} %{_libdir}/xrdp/libvnc.so %{_libdir}/libvnc.so
%endif


%preun
service xrdp stop
chkconfig --del xrdp > /dev/null


%postun
rm -rf /var/log/xrdp /var/spool/xrdp
if [ "$1" = "0" ]; then
    getent group tsusers >/dev/null && groupdel tsusers
fi

ldconfig


###########################################
%package seamrdp
###########################################

Summary: Seamless XRDP Shell
Group: Applications/System
%if %{defined rhel}
Requires: xrdp, xorg-x11-server-Xorg, xfwm4
%else
Requires: xrdp, xorg-x11-libX11
%endif


%description seamrdp
Seamlessrdpshell is a rdp addon offering the possibility to have an
application without a desktop


%files seamrdp
%defattr(-,root,root)
%config /etc/xrdp/seamrdp.conf
/usr/bin/seamlessrdpshell
/usr/bin/startapp
/usr/bin/XHook
%doc /usr/share/man/man1/seamlessrdpshell.1.gz
%doc /usr/share/man/man1/startapp.1.gz
%doc /usr/share/man/man1/XHook.1.gz


###########################################
%package rdpdr
###########################################

Summary: XRDP disks redirection
Group: Applications/System
Requires: xrdp, fuse, libxml2


%description rdpdr
XRDP channel that handle disks redirection.


%files rdpdr
%defattr(-,root,root)
%config /etc/xrdp/rdpdr.conf
/usr/sbin/vchannel_rdpdr
%doc /usr/share/man/man1/rdpdr_disk.1.gz
%doc /usr/share/man/man1/vchannel_rdpdr.1.gz


%post rdpdr
grep -q -E "^ *[^#] *user_allow_other *" /etc/fuse.conf
if [ $? -ne 0 ]; then
	echo "user_allow_other" >> /etc/fuse.conf
fi


###########################################
%package clipboard
###########################################

Summary: XRDP clipboard
Group: Applications/System
%if %{defined rhel}
Requires: xrdp, xorg-x11-server-Xorg
%else
Requires: xrdp, xorg-x11-libX11
%endif


%description clipboard
XRDP channel providing copy/past text functionnality.


%files clipboard
%defattr(-,root,root)
%config /etc/xrdp/cliprdr.conf
/usr/sbin/vchannel_cliprdr


###########################################
%package sound
###########################################

Summary: XRDP plugin for PulseAudio
Group: Applications/System
%if %{defined rhel}
Requires: xrdp, pulseaudio, alsa-utils, alsa-plugins-pulseaudio
%else
Requires: xrdp, pulseaudio, alsa-utils, libasound2
%endif


%description sound
This package contains the XRDP plugin for PulseAudio, a sound server for POSIX
and WIN32 systems

%files sound
%defattr(-,root,root)
%if %{defined rhel}
%config /etc/xrdp/asound.conf
%else
%config /etc/asound.conf
%endif
%config /etc/xrdp/rdpsnd.*
/usr/sbin/vchannel_rdpsnd

%post sound
[ -f /etc/asound.conf ] && cp /etc/asound.conf /etc/asound.conf.BACK
cp /etc/xrdp/asound.conf /etc/asound.conf

###########################################
%package printer
###########################################

Summary: cups file converter to ps format
Group: Applications/System
Requires: xrdp-rdpdr, python, ghostscript, cups


%description printer
Xrdpi-Printer convert a ps file from cups in ps


%files printer
%defattr(-,root,root)
%config /etc/cups/xrdp_printer.conf
/usr/lib*/cups/backend/xrdpprinter
/usr/share/cups/model/PostscriptColor.ppd.gz
%doc /usr/share/man/man1/rdpdr_printer.1.gz
%defattr(-,lp,root)
%dir /var/spool/xrdp_printer/SPOOL


###########################################
%package python
###########################################

Summary: Python API for XRDP
Group: Applications/System
Requires: xrdp, python


%description python
XRDP-Python is a Python wrapper for XRDP


%files -f INSTALLED_FILES python
%defattr(-,root,root)


###########################################
%package devel
###########################################

Summary: Developpement files for XRDP
Group: Development
Requires: xrdp

# TODO: headers missing


%description devel
Developpement files for XRDP


%files devel
%defattr(-,root,root)
/usr/lib*/*.a
/usr/lib*/*.la
/usr/lib*/*.so
/usr/lib*/xrdp/*.a
/usr/lib*/xrdp/*.la
/usr/lib*/xrdp/*.so


%if %{undefined rhel}
###########################################
%package scim
###########################################

Summary: XRDP Unicode input method
Group: Applications/System
Requires: scim


%description scim
XRDP-Scim provides unicode input support for XRDP using Scim


%files scim
%defattr(-,root,root)
/usr/share/scim/*
/usr/lib*/scim-1.0/*
%config /etc/xrdp/scim/*
/usr/bin/xrdp-scim-panel
%endif
