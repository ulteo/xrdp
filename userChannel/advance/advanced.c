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

#include <abstract.h>
#include "userChannel.h"
#include "xrdp_mm.h"
#include "xrdp_wm.h"
#include "fifo.h"
#include "quality_params.h"
#include "progressive_display.h"
#include "video_detection.h"
#include "quality_params.h"
#include "funcs.h"

extern struct userChannel* u;
#define UC_VERSION 0x80

/******************************************************************************/
int DEFAULT_CC
lib_userChannel_mod_start(struct userChannel* u, int w, int h, int bpp)
{
  LIB_DEBUG(u, "lib_userChannel_mod_start");

  u->server_width = w;
  u->server_height = h;
  u->server_bpp = bpp;
  int res = false;
  struct xrdp_wm* wm = (struct xrdp_wm*) u->wm;
  u->desktop = xrdp_screen_create(u->server_width, u->server_height, u->server_bpp, wm->client_info);
  u->q_params = (struct quality_params*) quality_params_create(wm->client_info);
  res = u->mod->mod_start(u->mod, w, h, bpp);

  return res;
}

/******************************************************************************/
int DEFAULT_CC
lib_userChannel_mod_end(struct userChannel* u)
{
  LIB_DEBUG(u, "lib_userChannel_mod_end");
  if (u->mod)
  {
    u->mod->mod_end(u->mod);
  }
  xrdp_screen_delete(u->desktop);
  quality_params_delete(u->q_params);
  return 0;
}

/*****************************************************************************/
void update_add(update* up)
{
  list_add_item(u->current_update_list, (tbus)up);
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_begin_update (struct xrdp_mod* mod)
{
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_end_update(struct xrdp_mod* mod)
{
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_reset(struct xrdp_mod* mod, int width, int height, int bpp)
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
lib_userChannel_server_fill_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy)
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
lib_userChannel_server_set_fgcolor(struct xrdp_mod* mod, int fgcolor)
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
lib_userChannel_server_paint_rect(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  char* data, int width, int height, int srcx, int srcy)
{
  if (u)
  {
    struct xrdp_wm* wm = (struct xrdp_wm*) u->wm;
    struct xrdp_cache* cache = (struct xrdp_cache*) wm->cache;
    struct quality_params* q_params = (struct quality_params*) u->q_params;
    struct xrdp_screen* self = u->desktop;
    struct update_rect* current_urect;
    unsigned int quality = (wm->client_info->use_progressive_display) ? wm->client_info->progressive_display_nb_level - 1 : 0;
    if (q_params->is_video_detection_enable)
    {
      video_detection_update(u->desktop, x, y, cx, cy, 0);
    }
    else
    {
      current_urect = (struct update_rect*) g_malloc(sizeof(struct update_rect), 0);
      current_urect->quality = 0;
      current_urect->quality_already_send = -1;
      current_urect->rect.left = x;
      current_urect->rect.top = y;
      current_urect->rect.right = x + cx;
      current_urect->rect.bottom = y + cy;
      fifo_push(u->desktop->candidate_update_rects, current_urect);
    }

    xrdp_screen_update_screen(self, x, y, cx, cy, data, width, height, srcx, srcy);

    if (wm->client_info->use_progressive_display)
    {
      progressive_display_add_rect(self);
	}
    else
    {
      xrdp_screen_update_desktop(u->desktop);
    }
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_screen_blt(struct xrdp_mod* mod, int x, int y, int cx, int cy,
                  int srcx, int srcy)
{
  if (u)
  {
     int bpp = (u->server_bpp + 7) / 8;
     if (bpp == 3)
     {
       bpp = 4;
     }
     char* data = (char*) g_malloc(cx*cy*bpp, 0);
     ip_image_crop(u->desktop->screen, srcx, srcy, cx, cy, data);
     lib_userChannel_server_paint_rect(u, x, y, cx, cy, data, cx, cy, srcx, srcy);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_set_pointer(struct xrdp_mod* mod, int x, int y, char* data, char* mask)
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
lib_userChannel_server_msg(struct xrdp_mod* mod, char* msg, int code)
{
  if (u)
  {
    server_msg(u, msg, code);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_is_term(struct xrdp_mod* mod)
{
  if (u)
  {
    server_is_term(u);
  }
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_palette(struct xrdp_mod* mod, int* palette)
{
  printf("server_palette is not implemented\n");

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_set_clip(struct xrdp_mod* mod, int x, int y, int cx, int cy)
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
lib_userChannel_server_reset_clip(struct xrdp_mod* mod)
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
lib_userChannel_server_set_bgcolor(struct xrdp_mod* mod, int bgcolor)
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
lib_userChannel_server_set_opcode(struct xrdp_mod* mod, int opcode)
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
lib_userChannel_server_set_mixmode(struct xrdp_mod* mod, int mixmode)
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
lib_userChannel_server_set_brush(struct xrdp_mod* mod, int x_orgin, int y_orgin,
                 int style, char* pattern)
{
  printf("server_set_brush is not implemented\n");

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_set_pen(struct xrdp_mod* mod, int style, int width)
{
  printf("server_set_pen is not implemented\n");

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_draw_line(struct xrdp_mod* mod, int x1, int y1, int x2, int y2)
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
lib_userChannel_server_add_char(struct xrdp_mod* mod, int font, int charactor,
                int offset, int baseline,
                int width, int height, char* data)
{
  printf("server_add_char is not implemented\n");
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_draw_text(struct xrdp_mod* mod, int font,
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
lib_userChannel_server_query_channel(struct xrdp_mod* mod, int index, char* channel_name,
                     int* channel_flags)
{
  if (u)
  {
    server_query_channel(u, index, channel_name, channel_flags);
  }

  return 0;
}

/*****************************************************************************/
/* returns -1 on error */
int DEFAULT_CC
lib_userChannel_server_get_channel_id(struct xrdp_mod* mod, char* name)
{
  if (u)
  {
    server_get_channel_id(u, name);
  }

  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
lib_userChannel_server_send_to_channel(struct xrdp_mod* mod, int channel_id,
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

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_userChannel_update_screen(struct userChannel* u)
{
  struct xrdp_screen* desktop = u->desktop;
  struct quality_params* qp = (struct quality_params*) u->q_params;
  quality_params_prepare_data3(u->q_params, u->desktop, u);
  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_userChannel_update(struct userChannel* u, long * t0)
{
  int i;
  struct quality_params* q_params = (struct quality_params*) u->q_params;
  if (q_params->is_video_detection_enable)
  {
    int t1 = g_time3();
    if ((t1 - *t0) > u->desktop->client_info->video_detection_updatetime)
    {
      /* // ici */
      for (i = u->desktop->candidate_video_regs->count-1 ; i >= 0; i--)
      {
        struct video_reg* tmp = (struct video_reg*) list_get_item(u->desktop->candidate_video_regs, i);
        tmp->nb_update-= 5;
      }
      for (i = u->desktop->candidate_video_regs->count-1 ; i >= 0; i--)
      {
        struct video_reg* tmp = (struct video_reg*) list_get_item(u->desktop->candidate_video_regs, i);
        int Bpp = (u->desktop->bpp + 7) / 8;
        if (Bpp == 3)
        {
          Bpp = 4;
        }
        if (tmp->nb_update <= 0)
        {
          update* up;
          up = g_malloc(sizeof(update), 1);
          up->order_type = paint_rect;
          up->x = tmp->rect.left;
          up->y = tmp->rect.top;
          int w = rect_width(&tmp->rect);
          int h = rect_height(&tmp->rect);
          up->cx = w;
          up->cy = h;
          up->width = w;
          up->height = h;
          up->srcx = 0;
          up->srcy = 0;
          up->data_len = up->cx * up->cy * Bpp;
          up->data = g_malloc(up->data_len, 0);
          ip_image_crop(u->desktop->screen, up->x, up->y, up->cx, up->cy, up->data);
          list_add_item(u->current_update_list, (tbus) up);
          list_remove_item(u->desktop->candidate_video_regs, i);
        }
      }
      *t0 = g_time3();
    }
  }
  return 0;
}

int DEFAULT_CC
lib_userChannel_get_version(struct userChannel* u)
{
  return UC_VERSION;
}
