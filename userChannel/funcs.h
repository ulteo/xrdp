/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2009

   simple functions

*/

#include "userChannel.h"

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
