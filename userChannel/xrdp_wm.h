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

#include "userChannel.h"


struct xrdp_wm* APP_CC
xrdp_wm_create(struct xrdp_process* owner, struct xrdp_client_info* client_info);
void APP_CC
xrdp_wm_delete(struct xrdp_wm* self);
int APP_CC
xrdp_wm_send_palette(struct xrdp_wm* self);
int APP_CC
xrdp_wm_send_bitmap(struct xrdp_wm* self, struct xrdp_bitmap* bitmap, int x, int y, int cx, int cy);
int APP_CC
xrdp_wm_set_focused(struct xrdp_wm* self, struct xrdp_bitmap* wnd);
int APP_CC
xrdp_wm_pointer(struct xrdp_wm* self, char* data, char* mask, int x, int y);
int APP_CC
xrdp_wm_load_pointer(struct xrdp_wm* self, char* file_name, char* data, char* mask, int* x, int* y);
int APP_CC
xrdp_wm_send_pointer(struct xrdp_wm* self, int cache_idx, char* data, char* mask, int x, int y);
int APP_CC
xrdp_wm_set_pointer(struct xrdp_wm* self, int cache_idx);
int APP_CC
xrdp_wm_load_static_colors(struct xrdp_wm* self);
int APP_CC
xrdp_wm_load_static_pointers(struct xrdp_wm* self);
int APP_CC
xrdp_wm_init(struct xrdp_wm* self);
int APP_CC
xrdp_wm_get_vis_region(struct xrdp_wm* self, struct xrdp_bitmap* bitmap, int x, int y, int cx, int cy, struct xrdp_region* region, int clip_children);
int APP_CC
xrdp_wm_mouse_move(struct xrdp_wm* self, int x, int y);
int APP_CC
xrdp_wm_mouse_click(struct xrdp_wm* self, int x, int y, int but, int down);
int APP_CC
xrdp_wm_key(struct xrdp_wm* self, int device_flags, int scan_code);
int APP_CC
xrdp_wm_key_sync(struct xrdp_wm* self, int device_flags, int key_flags);
int APP_CC
xrdp_wm_unicode_key(struct xrdp_wm* self, int unicode_key);
int APP_CC
xrdp_wm_pu(struct xrdp_wm* self, struct xrdp_bitmap* control);
int DEFAULT_CC
callback(long id, int msg, long param1, long param2, long param3, long param4);
int APP_CC
xrdp_wm_log_msg(struct xrdp_wm* self, char* msg);
int APP_CC
xrdp_wm_log_error(struct xrdp_wm* self, char* msg);
int APP_CC
xrdp_wm_get_wait_objs(struct xrdp_wm* self, tbus* robjs, int* rc, tbus* wobjs, int* wc, int* timeout);
int APP_CC
xrdp_wm_check_wait_objs(struct xrdp_wm* self);
int APP_CC
xrdp_wm_set_login_mode(struct xrdp_wm* self, int login_mode);

