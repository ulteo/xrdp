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

#include "ip_image.h"
#include "defines.h"
#include "os_calls.h"

struct ip_image* ip_image_create(int width, int height, int bpp) {
	struct ip_image* ip;
	ip = (struct ip_image*) g_malloc(sizeof(struct ip_image), 1);
	ip->width = width;
	ip->height = height;
	ip->bpp = bpp;
	ip->data = (char*) g_malloc(width*height*bpp, 1);
	return ip;
}

void ip_image_delete(struct ip_image* self){
	if (self->data)
		g_free(self->data);
	
	g_free(self);
}

void ip_image_merge(struct ip_image* self, int x, int y, int w, int h, char* data) {
	int i, j, ii, jj, pixel;
	int width = ((x + w) > self->width) ? self->width : x + w;
	int height = ((y + h) > self->height) ? self->height : y + h;
	
	for (j = y, jj = 0; j < height ; j++,  jj++) {
		for (i = x, ii = 0 ; i < width ; i++, ii++) {
			if(self->bpp == 8) {
				pixel = GETPIXEL8(data, ii, jj, w);
				SETPIXEL8(self->data, i, j, self->width, pixel);
			} else if (self->bpp == 15) {
				pixel = GETPIXEL16(data, ii, jj, w);
				SETPIXEL16(self->data, i, j, self->width, pixel);
			} else if (self->bpp == 16) {
				pixel = GETPIXEL16(data, ii, jj, w);
				SETPIXEL16(self->data, i, j, self->width, pixel);
			} else if (self->bpp == 24) {
				pixel = GETPIXEL32(data, ii, jj, w);
				SETPIXEL32(self->data, i, j, self->width, pixel);
			}
		}
	}
}

void ip_image_crop(struct ip_image* self, int x, int y, int cx, int cy, char* out) {
	int i, j, r, g, b;
	for (j = 0 ; j < cy ; j++) {
		for (i = 0; i < cx ; i++) {
			if (self->bpp == 8) {
				SETPIXEL8(out, i, j, cx, GETPIXEL8(self->data, x+i, y+j, self->width));
			} else if (self->bpp == 15) {
				SETPIXEL16(out, i, j, cx, GETPIXEL16(self->data, x+i, y+j, self->width));
			} else if (self->bpp == 16) {
				SETPIXEL16(out, i, j, cx, GETPIXEL16(self->data, x+i, y+j, self->width));
			} else if (self->bpp==24) {
				SETPIXEL32(out, i, j, cx, GETPIXEL32(self->data, x+i, y+j, self->width));
			}
		}
	}
}
