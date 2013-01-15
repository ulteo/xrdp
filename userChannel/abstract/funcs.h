/*
 * Copyright (C) 2012 userChannel SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2012
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
 */
#ifndef FUNCS_H
#define FUNCS_H

#include "userChannel.h"
#include "xrdp_bitmap.h"

int APP_CC
rect_contains_pt(struct xrdp_rect* in, int x, int y);
int APP_CC
rect_intersect(struct xrdp_rect* in1, struct xrdp_rect* in2, struct xrdp_rect* out);
int APP_CC
rect_contained_by(struct xrdp_rect* in1, int left, int top, int right, int bottom);
int APP_CC
check_bounds(struct xrdp_bitmap* b, int* x, int* y, int* cx, int* cy);
int APP_CC
add_char_at(char* text, int text_size, twchar ch, int index);
int APP_CC
remove_char_at(char* text, int text_size, int index);
int APP_CC
set_string(char** in_str, const char* in);
int APP_CC
wchar_repeat(twchar* dest, int dest_size_in_wchars, twchar ch, int repeat);

#endif // FUNCS_H
