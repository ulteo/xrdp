/**
 * Copyright (C) 2012 Ulteo SAS
 * http://www.ulteo.com
 * Author Vincent Roullier <vincent.roullier@ulteo.com> 2012
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

#include "os_calls.h"
#include "libxrdp.h"

int APP_CC
xrdp_image_compress_jpeg(int width, int height, int bpp, unsigned char* data, int quality, char* dest) {
    struct stream *s;
    int bufsize;
    make_stream(s);
    init_stream(s, JPEG_BUFFER_SIZE);
    xrdp_bitmap_jpeg_compress(data, width, height, s, bpp, quality);
    bufsize = (int) (s->p - s->data);
    g_memcpy(dest, s->data, bufsize);
    free_stream(s);
    return bufsize;
}

int APP_CC
xrdp_image_compress_rle(int width, int height, int bpp, unsigned char* data, char* dest)
{
   struct stream *s;
   struct stream *tmp_s;
   int e;
   int i;
   int bufsize;
   make_stream(s);
   init_stream(s, IMAGE_TILE_MAX_BUFFER_SIZE);
   make_stream(tmp_s);
   init_stream(tmp_s, IMAGE_TILE_MAX_BUFFER_SIZE);
   e = width % 4;

   if (e != 0)
       e = 4 - e;

   i = height;
   xrdp_bitmap_compress(data, width, height, s, bpp, IMAGE_TILE_MAX_BUFFER_SIZE, i - 1, tmp_s, e);
   bufsize = (int) (s->p - s->data);
   g_memcpy(dest, s->data, bufsize);
   free_stream(tmp_s);
   free_stream(s);
   return bufsize;
}
