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

#include "xrdp_screen.h"
#include "update_order.h"
#include "fifo.h"
#include "defines.h"
#include "libxrdpinc.h"


struct xrdp_screen* xrdp_screen_create(int w, int h, int bpp) {
	struct xrdp_screen* self;
	self = (struct xrdp_screen*) g_malloc(sizeof(struct xrdp_screen), 1);
	self->width = w;
	self->height = h;
	self->bpp = bpp;
	self->screen = ip_image_create(w, h, bpp);
	self->update_rects = list_create();
	self->update_rects->auto_free = 1;
	return self;
}

void xrdp_screen_delete(struct xrdp_screen* self) {
	if (self == 0)
		return;
	
	if (self->screen) {
		ip_image_delete(self->screen);
	}
	
	if (self->update_rects) {
		list_delete(self->update_rects);
	}
	
	g_free(self);
}

void xrdp_screen_update_desktop(struct xrdp_screen* self, int x, int y, int cx, int cy, char* data, int w, int h, int srcx, int srcy) {
	int i, j;
	struct list* update_rects = self->update_rects;
	struct xrdp_rect* rect;
	struct xrdp_rect* f_rect;
	struct xrdp_rect* r;
	
	struct xrdp_rect intersection;
	struct list* l_tmp =list_create();
	struct fifo* f_tmp = fifo_new();
	ip_image_merge(self->screen, x, y, w, h, data);
	struct xrdp_rect* current_rect = (struct xrdp_rect*) g_malloc(sizeof(struct xrdp_rect), 1);
	current_rect->left = x;
	current_rect->top = y;
	current_rect->right = x + cx;
	current_rect->bottom = y + cy;
	bool no_inter = true;
	
	fifo_push(f_tmp, current_rect);
	while (!fifo_is_empty(f_tmp)) {
		f_rect = (struct xrdp_rect*) fifo_pop(f_tmp);
		if (update_rects->count > 0) {
			no_inter = true;
			for (i = update_rects->count-1; i >= 0 ; i--) {
				rect = (struct xrdp_rect*) list_get_item(update_rects, i);
				if (! rect_equal(rect, f_rect)) {
					if (rect_intersect(rect, f_rect, &intersection)) {
						no_inter = false;
						list_remove_item(update_rects, i);
						rect_union(f_rect, rect, l_tmp);
						for (j = 0 ; j < l_tmp->count; j++) {
							r = (struct xrdp_rect*) list_get_item(l_tmp, j);
							fifo_push(f_tmp, r);
						}
						list_clear(l_tmp);
						break;
					}
				} else {
					no_inter = false;
					break;
				}
			}
			if (no_inter) {
				list_add_item(update_rects, (tbus) f_rect);
			}
		} else {
			list_add_item(update_rects, (tbus) f_rect);
		}
	}
	list_delete(l_tmp);
	fifo_free(f_tmp);
}
