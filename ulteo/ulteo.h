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

#ifndef ULTEO_MODULE_H
#define ULTEO_MODULE_H

#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "defines.h"
#include "proxy.h"


#define CURRENT_MOD_VER 1
#define LIBRARY_NAME "libvnc.so"

struct ulteo
{
  int size; /* size of this struct */
  int version; /* internal version */
  /* client functions */
  int (*mod_start)(struct ulteo* v, int w, int h, int bpp);
  int (*mod_connect)(struct ulteo* v);
  int (*mod_event)(struct ulteo* v, int msg, long param1, long param2,
                   long param3, long param4);
  int (*mod_signal)(struct ulteo* v);
  int (*mod_end)(struct ulteo* v);
  int (*mod_set_param)(struct ulteo* v, char* name, char* value);
  int (*mod_session_change)(struct ulteo* v, int, int);
  int (*mod_get_wait_objs)(struct ulteo* v, tbus* read_objs, int* rcount,
                           tbus* write_objs, int* wcount, int* timeout);
  int (*mod_check_wait_objs)(struct ulteo* v);
  long mod_dumby[100 - 9]; /* align, 100 minus the number of mod
                              functions above */
  /* server functions */
  int (*server_begin_update)(struct ulteo* v);
  int (*server_end_update)(struct ulteo* v);
  int (*server_fill_rect)(struct ulteo* v, int x, int y, int cx, int cy);
  int (*server_screen_blt)(struct ulteo* v, int x, int y, int cx, int cy,
                           int srcx, int srcy);
  int (*server_paint_rect)(struct ulteo* v, int x, int y, int cx, int cy,
                           char* data, int width, int height, int srcx, int srcy);
  int (*server_set_cursor)(struct ulteo* v, int x, int y, char* data, char* mask);
  int (*server_palette)(struct ulteo* v, int* palette);
  int (*server_msg)(struct ulteo* v, char* msg, int code);
  int (*server_is_term)(struct ulteo* v);
  int (*server_set_clip)(struct ulteo* v, int x, int y, int cx, int cy);
  int (*server_reset_clip)(struct ulteo* v);
  int (*server_set_fgcolor)(struct ulteo* v, int fgcolor);
  int (*server_set_bgcolor)(struct ulteo* v, int bgcolor);
  int (*server_set_opcode)(struct ulteo* v, int opcode);
  int (*server_set_mixmode)(struct ulteo* v, int mixmode);
  int (*server_set_brush)(struct ulteo* v, int x_orgin, int y_orgin,
                          int style, char* pattern);
  int (*server_set_pen)(struct ulteo* v, int style,
                        int width);
  int (*server_draw_line)(struct ulteo* v, int x1, int y1, int x2, int y2);
  int (*server_add_char)(struct ulteo* v, int font, int charactor,
                         int offset, int baseline,
                         int width, int height, char* data);
  int (*server_draw_text)(struct ulteo* v, int font,
                          int flags, int mixmode, int clip_left, int clip_top,
                          int clip_right, int clip_bottom,
                          int box_left, int box_top,
                          int box_right, int box_bottom,
                          int x, int y, char* data, int data_len);
  int (*server_reset)(struct ulteo* v, int width, int height, int bpp);
  int (*server_query_channel)(struct ulteo* v, int index,
                              char* channel_name,
                              int* channel_flags);
  int (*server_get_channel_id)(struct ulteo* v, char* name);
  int (*server_send_to_channel)(struct ulteo* v, int channel_id,
                                char* data, int data_len,
                                int total_data_len, int flags);
  long server_dumby[100 - 24]; /* align, 100 minus the number of server
                                  functions above */
  /* common */
  long handle; /* pointer to self as long */
  long wm;
  long painter;
  int sck;
  /* mod data */
  int server_width;
  int server_height;
  int server_bpp;
  int mod_width;
  int mod_height;
  int mod_bpp;
  char mod_name[256];
  int mod_mouse_state;
  int palette[256];
  int ulteo_desktop;
  char username[256];
  char password[256];
  char ip[256];
  char port[256];
  int sck_closed;
  int shift_state; /* 0 up, 1 down */
  int keylayout;
  int clip_chanid;
  char* clip_data;
  int clip_data_size;
  tbus sck_obj;
  /* mod ulteo */
  long mod_handle; /* returned from g_load_library */
  struct xrdp_mod* (*mod_lib_init)(void);
  int (*mod_lib_exit)(struct xrdp_mod*);

  struct xrdp_mod* mod;
};


#endif
