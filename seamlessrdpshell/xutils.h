/**
 * Copyright (C) 2009 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com>
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

#ifndef XUTILS_H_
#define XUTILS_H_

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <stdio.h>

/* Xlib constant  */
#define _NET_WM_STATE_REMOVE		0	/* remove/unset property */
#define _NET_WM_STATE_ADD		1	/* add/set property */
#define _NET_WM_STATE_TOGGLE		2	/* toggle property  */

#define STATE_NORMAL			0
#define STATE_ICONIFIED			1
#define STATE_MAXIMIZED_HORIZ		2
#define STATE_MAXIMIZED_VERT		3
#define STATE_MAXIMIZED_BOTH		4

int hex2int(const char *hexa_string);
const char *gravityToStr(int gravity);
Window get_in_window(Display * display, Window w);
int get_window_name(Display * display, Window w, unsigned char **name);
int
get_window_state(Display * display, Window w, Atom ** atoms,
		 unsigned long *nitems);
int set_wm_state(Display* display, Window w, int state);
int is_splash_window(Display * display, Window w);
int get_window_type(Display * display, Window w, Atom * atom);
int get_window_pid(Display * display, Window w, int *pid);
int get_parent_window(Display * display, Window w, Window * parent);
int is_good_window(Display * display, Window w);
int
get_property(Display * display, Window w, const char *property,
	     unsigned long *nitems, unsigned char **data);

#endif				/* XUTILS_H_ */
