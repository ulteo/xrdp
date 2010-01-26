# -*- coding: utf-8 -*-

# Copyright (C) 2009 Ulteo SAS
# http://www.ulteo.com
# Author Julien LANGLOIS <julien@ulteo.com> 2009
# Author David LECHEVALIER <david@ulteo.com> 2009
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
from xml.dom import minidom

USER_PREF_UNKNOW		= "UNKNOW"


def _UserGetSocket():
	filename = "/tmp/management"
	
	if not os.path.exists(filename):
		return None
	
	# ToDo: check is rw access on the file
	
	return filename


def _UserProcessRequest(request):
	path = _UserGetSocket()
	if path is None:
		return None
	
	s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
	try:
		s.connect(path)
	except socket.error,err:
		print "Unable to connect: ",err
		return None	
	
	data = request.toxml()
		
	s.send(struct.pack('>I', len(data)))
	s.send(data)
	
	buffer = s.recv(4)
	try:
		packet_len = struct.unpack('>I', buffer)[0]
	except Exception, e:
		print "packet recv syntax error"
		s.close()
		return None
	
	data = s.recv(packet_len)
	s.close()
	
	try:
		document = minidom.parseString(data)
	except:
		print "server didn't return XML"
		return None
	
	if document.documentElement.nodeName == "error":
		print "server return error"
		return None
	

	return document


def _UserFormatRequest(username_, type_, action_, key_, value_ = None ):
	doc = minidom.Document()
	rootNode = doc.createElement("request")
	rootNode.setAttribute("type", type_)
	rootNode.setAttribute("action", action_)
	rootNode.setAttribute("username", username_)
	rootNode.setAttribute("key", key_)
	if value_ is not None:
		rootNode.setAttribute("value", value_)
		
	doc.appendChild(rootNode)
	return doc




def UserGetUserPref(username, key):
	doc = _UserProcessRequest(_UserFormatRequest(username, "user_conf", "get", key ))
	
	if doc is None:
		return USER_PREF_UNKNOW

	pref = doc.getElementsByTagName("user_conf")
	if len(pref) == 0:
		return USER_PREF_UNKNOW
	
	prefNode = pref[0]
	value = prefNode.getAttribute("value")
	return value


def UserSetUserPref(username, key, value):
	doc = _UserProcessRequest(_UserFormatRequest(username, "user_conf", "set", key, value ))
	
	if doc is None:
		return False

	pref = doc.getElementsByTagName("response")
	if len(pref) == 0:
		return False
	return True


def UserSetShell(username, shell):
	return UserSetUserPref(username, "DefaultShell", shell)


def UserGetShell(username):
	return UserGetUserPref(username, "DefaultShell")


def UserAllowUserShellOverride(username, auth):
	return UserSetUserPref(username, "AuthorizeAlternateShell", str(auth))

def UserIsAllowUserShellOverride(username):
	rep = UserGetUserPref(username, "AuthorizeAlternateShell")
	return rep == "True"


