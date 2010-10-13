/**
 * Copyright (C) 2008 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2010
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#ifndef SHARES_H_
#define SHARES_H_

#include "log.h"
#include "rdpfs.h"
#include <os_calls.h>

#define DESKTOP_FILE_TEMPLATE "[Desktop Entry]\n" \
		"Version=1.0\n" \
		"Type=Link\n" \
		"Name=%ShareName%\n" \
		"Comment=XRDP share\n" \
		"URL=%SharePath%\n" \
		"Icon=folder-remote\n"

#define BOOKMARK_FILENAME  ".gtk-bookmarks"


int share_add_to_desktop(const char* share_name);
int share_remove_from_desktop(const char* share_name);

#endif /* SHARES_H_ */
