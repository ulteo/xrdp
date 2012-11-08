# Copyright (C) 2012 Ulteo SAS
# http://www.ulteo.com
# Author Alexandre CONFIANT-LATOUR <a.confiant@ulteo.com> 2012
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

AM_CPPFLAGS = \
	-DSCIM_XRDP_LOCALEDIR=\"$(datadir)/locale\" \
	-DSCIM_ICONDIR=\"@SCIM_ICONDIR@\" \
	@SCIM_CFLAGS@

xrdp_la_CFLAGS = @SCIM_CFLAGS@
xrdp_la_CXXFLAGS = @SCIM_CFLAGS@
xrdp_la_LDFLAGS = @SCIM_LIBS@ \
	-avoid-version \
	-rpath $(moduledir) \
	-module

xrdp_la_SOURCES = \
	scim_xrdp_imengine_factory.h \
	scim_xrdp_imengine_factory.cpp \
	scim_xrdp_imengine.h \
	scim_xrdp_imengine.cpp

moduledir = @SCIM_MODULEDIR@/IMEngine
module_LTLIBRARIES = \
  xrdp.la

moduleconfdir=$(sysconfdir)/xrdp/scim
moduleconf_DATA = \
  config \
  global

moduledatadir=@SCIM_ICONDIR@
moduledata_DATA = \
  scim-xrdp.png

EXTRA_DIST = \
  $(module_LTLIBRARIES) \
  $(moduleconf_DATA) \
  $(moduledata_DATA)

