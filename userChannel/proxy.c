/*
 * Copyright (C) 2012 Ulteo SAS
 * http://www.userChannel.com
 * Author David LECHEVALIER <david@userChannel.com> 2012
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

#include "proxy.h"


extern struct userChannel* u;



/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_begin_update (struct xrdp_mod* mod)
{
  if (u)
  {
    u->server_begin_update(u);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_end_update(struct xrdp_mod* mod)
{
  if (u)
  {
    u->server_end_update(u);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_reset(struct xrdp_mod* mod, int width, int height, int bpp)
{
  if (u)
  {
    u->server_reset(u, width, height, bpp);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  if (u)
  {
    u->server_fill_rect(u, x, y, cx, cy);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_fgcolor(struct xrdp_mod* mod, int fgcolor)
{
  if (u)
  {
    u->server_set_fgcolor(u, fgcolor);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  char* data, int width, int height, int srcx, int srcy)
{
  if (u)
  {
    u->server_paint_rect(u, x, y, cx, cy, data, width, height, srcx, srcy);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
  if (u)
  {
    u->server_screen_blt(u, x, y, cx, cy, srcx, srcy);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_pointer(struct xrdp_mod* mod, int x, int y, char* data, char* mask)
{
  if (u)
  {
    u->server_set_cursor(u, x, y, data, mask);
  }
  return 0;
}


/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_msg(struct xrdp_mod* mod, char* msg, int code)
{
  if (u)
  {
    u->server_msg(u, msg, code);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_is_term(struct xrdp_mod* mod)
{
  if (u)
  {
    u->server_is_term(u);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_palette(struct xrdp_mod* mod, int* palette)
{
  if (u)
  {
    u->server_palette(u, palette);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  if (u)
  {
    u->server_set_clip(u, x, y, cx, cy);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_reset_clip(struct xrdp_mod* mod)
{
  if (u)
  {
    u->server_reset_clip(u);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_bgcolor(struct xrdp_mod* mod, int bgcolor)
{
  if (u)
  {
    u->server_set_bgcolor(u, bgcolor);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_opcode(struct xrdp_mod* mod, int opcode)
{
  if (u)
  {
    u->server_set_opcode(u, opcode);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_mixmode(struct xrdp_mod* mod, int mixmode)
{
  if (u)
  {
    u->server_set_mixmode(u, mixmode);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin,
                 int style, char* pattern)
{
  if (u)
  {
    u->server_set_brush(u, x_orgin, y_orgin, style, pattern);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_pen(struct xrdp_mod* mod, int style, int width)
{
  if (u)
  {
    u->server_set_pen(u, style, width);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2)
{
  if (u)
  {
    u->server_draw_line(u, x1, y1, x2, y2);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_add_char(struct xrdp_mod* mod, int font, int charactor,
                int offset, int baseline,
                int width, int height, char* data)
{
  if (u)
  {
    u->server_add_char(u, font, charactor, offset, baseline, width, height, data);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_draw_text(struct xrdp_mod* mod, int font,
                 int flags, int mixmode, int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char* data, int data_len)
{
  if (u)
  {
    u->server_draw_text(u, font, flags, mixmode, clip_left, clip_top, clip_right, clip_bottom, box_left, box_top, box_right, box_bottom, x, y, data, data_len);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_query_channel(struct xrdp_mod* mod, int index, char* channel_name,
                     int* channel_flags)
{
  if (u)
  {
    u->server_query_channel(u, index, channel_name, channel_flags);
  }
  return 0;
}

/*****************************************************************************/
/* returns -1 on error */
int DEFAULT_CC
lib_userChannel_proxy_server_get_channel_id(struct xrdp_mod* mod, char* name)
{
  if (u)
  {
    u->server_get_channel_id(u, name);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_send_to_channel(struct xrdp_mod* mod, int channel_id,
                       char* data, int data_len,
                       int total_data_len, int flags)
{
  if (u)
  {
    u->server_send_to_channel(u, channel_id, data, data_len, total_data_len, flags);
  }
  return 0;
}

