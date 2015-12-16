# Copyright (C) 2010, 2014 Ulteo SAS
# http://www.ulteo.com
# Author Samuel BOVEE <samuel@ulteo.com> 2010
# Author David LECHEVALIER <david@ulteo.com> 2010
# Author David PHAM-VAN <d.pham-van@ulteo.com> 2012
# Author Alexandre CONFIANT-DELATOUR <a.confiant@ulteo.com> 2014
# Author Vincent ROULLIER <v.roullier@ulteo.com> 2014
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

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -Wall -O2 -D_REENTRANT -DIMAGE_MAGICK_SUPPORT \
  `pkg-config MagickWand --cflags`

AM_LDFLAGS = \
  $(filter-out -lglib-2.0, $(shell pkg-config MagickWand --libs))

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel/lib

bin_PROGRAMS = \
  XHook

XHook_SOURCES = \
  main.c seamlessrdpshell.h \
  xutils.c xutils.h

XHook_LDADD = \
  -lX11 \
  $(top_builddir)/vchannel/lib/libxrdp_vchannel.la \
  $(top_builddir)/common/libxrdp_common.la

seamlessrdpshelldir=$(bindir)
seamlessrdpshell_DATA = \
  seamlessrdpshell

seamlessrdpshellconfdir=$(sysconfdir)/xrdp
seamlessrdpshellconf_DATA = \
  seamrdp.conf

install-data-hook:
	chmod 755 $(DESTDIR)$(bindir)/seamlessrdpshell

EXTRA_DIST = \
  $(seamlessrdpshell_DATA) \
  $(seamlessrdpshellconf_DATA)

