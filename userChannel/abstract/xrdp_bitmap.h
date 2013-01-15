/**
 * Copyright (C) 2011-2012 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2011, 2012
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

#ifndef XRDP_BITMAP_H
#define XRDP_BITMAP_H

#include "userChannel.h"


struct xrdp_bitmap* APP_CC
xrdp_bitmap_create(int width, int height, int bpp, int type, struct xrdp_wm* wm);
struct xrdp_bitmap* APP_CC
xrdp_bitmap_create_with_data(int width, int height, int bpp, char* data, struct xrdp_wm* wm);
void APP_CC
xrdp_bitmap_delete(struct xrdp_bitmap* self);
struct xrdp_bitmap* APP_CC
xrdp_bitmap_get_child_by_id(struct xrdp_bitmap* self, int id);
int APP_CC
xrdp_bitmap_set_focus(struct xrdp_bitmap* self, int focused);
int APP_CC
xrdp_bitmap_resize(struct xrdp_bitmap* self, int width, int height);
int APP_CC
xrdp_bitmap_load(struct xrdp_bitmap* self, const char* filename, int* palette);
int APP_CC
xrdp_bitmap_get_pixel(struct xrdp_bitmap* self, int x, int y);
int APP_CC
xrdp_bitmap_set_pixel(struct xrdp_bitmap* self, int x, int y, int pixel);
int APP_CC
xrdp_bitmap_copy_box(struct xrdp_bitmap* self, struct xrdp_bitmap* dest, int x, int y, int cx, int cy);
int APP_CC
xrdp_bitmap_copy_box_with_crc(struct xrdp_bitmap* self, struct xrdp_bitmap* dest, int x, int y, int cx, int cy);
int APP_CC
xrdp_bitmap_compare(struct xrdp_bitmap* self, struct xrdp_bitmap* b);
int APP_CC
xrdp_bitmap_compare_with_crc(struct xrdp_bitmap* self, struct xrdp_bitmap* b);
int APP_CC
xrdp_bitmap_invalidate(struct xrdp_bitmap* self, struct xrdp_rect* rect);
int APP_CC
xrdp_bitmap_def_proc(struct xrdp_bitmap* self, int msg, int param1, int param2);
int APP_CC
xrdp_bitmap_to_screenx(struct xrdp_bitmap* self, int x);
int APP_CC
xrdp_bitmap_to_screeny(struct xrdp_bitmap* self, int y);
int APP_CC
xrdp_bitmap_from_screenx(struct xrdp_bitmap* self, int x);
int APP_CC
xrdp_bitmap_from_screeny(struct xrdp_bitmap* self, int y);
int APP_CC
xrdp_bitmap_get_screen_clip(struct xrdp_bitmap* self,
                            struct xrdp_painter* painter,
                            struct xrdp_rect* rect,
                            int* dx, int* dy);

#endif // XRDP_BITMAP_H
