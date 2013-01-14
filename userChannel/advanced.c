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

#include "proxy.h"
#include "userChannel.h"


extern struct userChannel* u;


/*****************************************************************************/
void update_add(update* up)
{
  list_add_item(u->current_update_list, (tbus)up);
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_begin_update (struct xrdp_mod* mod)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = begin_update;
    update_add(up);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_end_update(struct xrdp_mod* mod)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = end_update;
    update_add(up);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_reset(struct xrdp_mod* mod, int width, int height, int bpp)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = reset;
    up->width = width;
    up->height = height;
    up->bpp = bpp;
    update_add(up);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = fill_rect;
    up->x = x;
    up->y = y;
    up->width = cx;
    up->height = cy;
    update_add(up);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_fgcolor(struct xrdp_mod* mod, int fgcolor)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = set_fgcolor;
    up->color = fgcolor;
    update_add(up);
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
    int bpp = (u->server_bpp + 7) / 8;
    if (bpp == 3)
    {
      bpp = 4;
    }

    update* up = g_malloc(sizeof(update), 1);
    up->order_type = paint_rect;
    up->x = x;
    up->y = y;
    up->cx = cx;
    up->cy = cy;
    up->width = width;
    up->height = height;
    up->srcx = srcx;
    up->srcy = srcy;
    up->data_len = cx*cy*bpp;
    up->data = g_malloc(up->data_len, 0);
    g_memcpy(up->data, data, up->data_len);
    update_add(up);
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
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = screen_blt;
    up->x = x;
    up->y = y;
    up->cx = cx;
    up->cy = cy;
    up->srcx = srcx;
    up->srcy = srcy;
    update_add(up);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_pointer(struct xrdp_mod* mod, int x, int y, char* data, char* mask)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = set_cursor;
    up->x = x;
    up->y = y;
    up->data_len = 32 * 32 * 3;
    up->mask_len = 32 * 32 / 8;
    up->data = g_malloc(up->data_len, 0);
    up->mask = g_malloc(up->mask_len, 0);
    g_memcpy(up->data, data, up->data_len);
    g_memcpy(up->mask, mask, up->mask_len);
    update_add(up);
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
  printf("server_palette is not implemented\n");

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = set_clip;
    up->x = x;
    up->y = y;
    up->width = cx;
    up->height = cy;
    update_add(up);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_reset_clip(struct xrdp_mod* mod)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = reset_clip;
    update_add(up);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_bgcolor(struct xrdp_mod* mod, int bgcolor)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = set_bgcolor;
    up->color = bgcolor;
    update_add(up);
  }

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_opcode(struct xrdp_mod* mod, int opcode)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = set_opcode;
    up->opcode = opcode;
    update_add(up);
  }

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_mixmode(struct xrdp_mod* mod, int mixmode)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = set_mixmode;
    up->mixmode = mixmode;
    update_add(up);
  }

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin,
                 int style, char* pattern)
{
  printf("server_set_brush is not implemented\n");

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_set_pen(struct xrdp_mod* mod, int style, int width)
{
  printf("server_set_pen is not implemented\n");

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2)
{
  if (u)
  {
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = draw_line;
    up->srcx = x1;
    up->srcy = y1;
    up->x = x2;
    up->y = y2;

    update_add(up);
  }

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_proxy_server_add_char(struct xrdp_mod* mod, int font, int charactor,
                int offset, int baseline,
                int width, int height, char* data)
{
  printf("server_add_char is not implemented\n");
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
  printf("server_draw_text is not implemented\n");

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
    update* up = g_malloc(sizeof(update), 1);
    up->order_type = send_to_channel;
    up->channel_id = channel_id;
    up->data = g_malloc(data_len, 0);
    g_memcpy(up->data, data, data_len);
    up->data_len = data_len;
    up->total_data_len = total_data_len;
    up->flags = flags;

    update_add(up);
  }

  return 0;
}

