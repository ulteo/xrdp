/**
 * Copyright (C) 2013 Ulteo SAS
 * http://www.ulteo.com
 * Author roullier <vincent.roullier@ulteo.com> 2013
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

#ifndef _XRDP_SCREEN_H
#define _XRDP_SCREEN_H

#include "list.h"

struct xrdp_screen {
	struct ip_image* screen;
	int width;
	int height;
	int bpp;
	struct list* update_rects;
};

struct xrdp_screen* xrdp_screen_create(int w, int h, int bpp);
void xrdp_screen_delete(struct xrdp_screen* self);
void xrdp_screen_update_desktop(struct xrdp_screen* self, int x, int y, int cx, int cy, char* data, int w, int h, int srcx, int srcy);

#endif //_XRDP_SCREEN_H
