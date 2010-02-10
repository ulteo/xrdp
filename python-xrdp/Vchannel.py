# -*- coding: utf-8 -*-

# Copyright (C) 2010 Ulteo SAS
# http://www.ulteo.com
# Author Julien LANGLOIS <julien@ulteo.com> 2010
# Author David LECHEVALIER <david@ulteo.com> 2010
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

import os
import socket
import struct
from VchannelException import VchannelException

def _VchannelGetSocket():
	if not os.environ.has_key("DISPLAY"):
		raise VchannelException("Unable to get environment variable $DISPLAY")
	
	path = "/var/spool/xrdp/xrdp_user_channel_socket%s"%(os.environ["DISPLAY"])
	if not os.path.exists(path):
		raise VchannelException("The socket did not exist")
	
	# todo if not read/write access ...
	
	return path


def VchannelOpen(name):
	path = _VchannelGetSocket()
	if path is None:
		raise VchannelException("Internal error")

	try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(path)
	except socket.error:
		raise VchannelException("Unable to open Vchannel : %s"%(name))	

	VchannelWrite(s, name)
	return s


def VchannelClose(s):
	return s.close()


def VchannelRead(s):
	try:
		data = s.recv(4)
	except socket.error:
		raise VchannelException("Unable read data from channel")

	if len(data) == 0:
		raise VchannelException("Data size is invalid")
	
	size = struct.unpack('>I',data)[0]
	if size == 0:
		return None
	try:
		data = s.recv(size)
	except socket.error:
		raise VchannelException("Unable read data from channel")

	return data


def VchannelWrite(s, data):
	data_len = struct.pack('>I', len(data))
	try:
		s.sendall(data_len)
		s.sendall(data)
	except socket.error:
		raise VchannelException("Unable write data on the channel")

	return True

