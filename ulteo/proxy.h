/*
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
 */

#ifndef PROXY_H_
#define PROXY_H_

#include "arch.h"
#include "ulteo.h"



struct xrdp_mod
{
  int size; /* size of this struct */
  int version; /* internal version */
  /* client functions */
  int (*mod_start)(struct xrdp_mod* v, int w, int h, int bpp);
  int (*mod_connect)(struct xrdp_mod* v);
  int (*mod_event)(struct xrdp_mod* v, int msg, long param1, long param2,
                   long param3, long param4);
  int (*mod_signal)(struct xrdp_mod* v);
  int (*mod_end)(struct xrdp_mod* v);
  int (*mod_set_param)(struct xrdp_mod* v, char* name, char* value);
  int (*mod_session_change)(struct xrdp_mod* v, int, int);
  int (*mod_get_wait_objs)(struct xrdp_mod* v, tbus* read_objs, int* rcount,
                           tbus* write_objs, int* wcount, int* timeout);
  int (*mod_check_wait_objs)(struct xrdp_mod* v);
  long mod_dumby[100 - 9]; /* align, 100 minus the number of mod
                              functions above */
  /* server functions */
  int (*server_begin_update)(struct xrdp_mod* v);
  int (*server_end_update)(struct xrdp_mod* v);
  int (*server_fill_rect)(struct xrdp_mod* v, int x, int y, int cx, int cy);
  int (*server_screen_blt)(struct xrdp_mod* v, int x, int y, int cx, int cy,
                           int srcx, int srcy);
  int (*server_paint_rect)(struct xrdp_mod* v, int x, int y, int cx, int cy,
                           char* data, int width, int height, int srcx, int srcy);
  int (*server_set_pointer)(struct xrdp_mod* v, int x, int y, char* data, char* mask);
  int (*server_palette)(struct xrdp_mod* v, int* palette);
  int (*server_msg)(struct xrdp_mod* v, char* msg, int code);
  int (*server_is_term)(struct xrdp_mod* v);
  int (*server_set_clip)(struct xrdp_mod* v, int x, int y, int cx, int cy);
  int (*server_reset_clip)(struct xrdp_mod* v);
  int (*server_set_fgcolor)(struct xrdp_mod* v, int fgcolor);
  int (*server_set_bgcolor)(struct xrdp_mod* v, int bgcolor);
  int (*server_set_opcode)(struct xrdp_mod* v, int opcode);
  int (*server_set_mixmode)(struct xrdp_mod* v, int mixmode);
  int (*server_set_brush)(struct xrdp_mod* v, int x_orgin, int y_orgin,
                          int style, char* pattern);
  int (*server_set_pen)(struct xrdp_mod* v, int style,
                        int width);
  int (*server_draw_line)(struct xrdp_mod* v, int x1, int y1, int x2, int y2);
  int (*server_add_char)(struct xrdp_mod* v, int font, int charactor,
                         int offset, int baseline,
                         int width, int height, char* data);
  int (*server_draw_text)(struct xrdp_mod* v, int font,
                          int flags, int mixmode, int clip_left, int clip_top,
                          int clip_right, int clip_bottom,
                          int box_left, int box_top,
                          int box_right, int box_bottom,
                          int x, int y, char* data, int data_len);
  int (*server_reset)(struct xrdp_mod* v, int width, int height, int bpp);
  int (*server_query_channel)(struct xrdp_mod* v, int index,
                              char* channel_name,
                              int* channel_flags);
  int (*server_get_channel_id)(struct xrdp_mod* v, char* name);
  int (*server_send_to_channel)(struct xrdp_mod* v, int channel_id,
                                char* data, int data_len,
                                int total_data_len, int flags);
  long server_dumby[100 - 24]; /* align, 100 minus the number of server
                                  functions above */
  /* common */
  long handle; /* pointer to self as int */
  long wm; /* struct xrdp_wm* */
  long painter;
  int sck;
};


int DEFAULT_CC
lib_ulteo_proxy_server_begin_update (struct xrdp_mod* mod);
int DEFAULT_CC
lib_ulteo_proxy_server_end_update(struct xrdp_mod* mod);
int DEFAULT_CC
lib_ulteo_proxy_server_reset(struct xrdp_mod* mod, int width, int height, int bpp);
int DEFAULT_CC
lib_ulteo_proxy_server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy);
int DEFAULT_CC
lib_ulteo_proxy_server_set_fgcolor(struct xrdp_mod* mod, int fgcolor);
int DEFAULT_CC
lib_ulteo_proxy_server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy, char* data, int width, int height, int srcx, int srcy);
int DEFAULT_CC
lib_ulteo_proxy_server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy, int srcx, int srcy);
int DEFAULT_CC
lib_ulteo_proxy_server_set_pointer(struct xrdp_mod* mod, int x, int y, char* data, char* mask);
int DEFAULT_CC
lib_ulteo_proxy_server_msg(struct xrdp_mod* mod, char* msg, int code);
int DEFAULT_CC
lib_ulteo_proxy_server_is_term(struct xrdp_mod* mod);
int DEFAULT_CC
lib_ulteo_proxy_server_palette(struct xrdp_mod* mod, int* palette);
int DEFAULT_CC
lib_ulteo_proxy_server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy);
int DEFAULT_CC
lib_ulteo_proxy_server_reset_clip(struct xrdp_mod* mod);
int DEFAULT_CC
lib_ulteo_proxy_server_set_bgcolor(struct xrdp_mod* mod, int bgcolor);
int DEFAULT_CC
lib_ulteo_proxy_server_set_opcode(struct xrdp_mod* mod, int opcode);
int DEFAULT_CC
lib_ulteo_proxy_server_set_mixmode(struct xrdp_mod* mod, int mixmode);
int DEFAULT_CC
lib_ulteo_proxy_server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin, int style, char* pattern);
int DEFAULT_CC
lib_ulteo_proxy_server_set_pen(struct xrdp_mod* mod, int style, int width);
int DEFAULT_CC
lib_ulteo_proxy_server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2);
int DEFAULT_CC
lib_ulteo_proxy_server_add_char(struct xrdp_mod* mod, int font, int charactor, int offset, int baseline, int width, int height, char* data);
int DEFAULT_CC
lib_ulteo_proxy_server_draw_text(struct xrdp_mod* mod, int font, int flags, int mixmode, int clip_left, int clip_top, int clip_right, int clip_bottom, int box_left, int box_top, int box_right, int box_bottom, int x, int y, char* data, int data_len);
int DEFAULT_CC
lib_ulteo_proxy_server_query_channel(struct xrdp_mod* mod, int index, char* channel_name, int* channel_flags);
int DEFAULT_CC
lib_ulteo_proxy_server_get_channel_id(struct xrdp_mod* mod, char* name);
int DEFAULT_CC
lib_ulteo_proxy_server_send_to_channel(struct xrdp_mod* mod, int channel_id, char* data, int data_len, int total_data_len, int flags);

#endif /* PROXY_H_ */
