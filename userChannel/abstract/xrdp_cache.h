/**
 * Copyright (C) 2012 Ulteo SAS
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
 **/

#include "userChannel.h"

struct xrdp_cache* APP_CC
xrdp_cache_create(struct xrdp_wm* owner, struct xrdp_session* session, struct xrdp_client_info* client_info);
void APP_CC
xrdp_cache_delete(struct xrdp_cache* self);
int APP_CC
xrdp_cache_reset(struct xrdp_cache* self, struct xrdp_client_info* client_info);
int APP_CC
xrdp_cache_add_bitmap(struct xrdp_cache* self, struct xrdp_bitmap* bitmap);
int APP_CC
xrdp_cache_add_palette(struct xrdp_cache* self, int* palette);
int APP_CC
xrdp_cache_add_char(struct xrdp_cache* self, struct xrdp_font_char* font_item);
int APP_CC
xrdp_cache_add_pointer(struct xrdp_cache* self, struct xrdp_pointer_item* pointer_item);
int APP_CC
xrdp_cache_add_pointer_static(struct xrdp_cache* self, struct xrdp_pointer_item* pointer_item, int index);
int APP_CC
xrdp_cache_add_brush(struct xrdp_cache* self, char* brush_item_data);

