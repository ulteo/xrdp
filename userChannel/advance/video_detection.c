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

#include "video_detection.h"
#include "defines.h"
#include "os_calls.h"
#include "list.h"
#include "fifo.h"
#include "funcs.h"
#include "update_order.h"
#include "userChannel.h"
#include "quality_params.h"

void video_detection_update(struct xrdp_screen* self, int x, int y, int cx, int cy, int quality)
{
  struct list* c_video_regs = self->candidate_video_regs;
  struct list* video_regs = self->video_regs;
  struct fifo* f_tmp = fifo_new();
  struct list* l_tmp = list_create();
  struct fifo* f_tmp2 = self->candidate_update_rects;
  struct xrdp_rect* f_rect;
  struct xrdp_rect* l2_prect;
  struct xrdp_rect intersection;
  bool no_inter = true;
  struct xrdp_rect* r;
  int i, j;
  struct xrdp_rect current_rect;
  current_rect.left = x;
  current_rect.top = y;
  current_rect.right = x + cx;
  current_rect.bottom = y + cy;
  update_candidate_video_regions(c_video_regs, current_rect, self->client_info->video_detection_maxfps);
  update_video_regions(video_regs, c_video_regs, self->client_info->video_detection_fps);
  fifo_push(f_tmp, &current_rect);
  while (!fifo_is_empty(f_tmp))
  {
    f_rect = (struct xrdp_rect*) fifo_pop(f_tmp);
    if (video_regs->count > 0)
    {
      no_inter = true;
      for (i = video_regs->count - 1; i >= 0; i--)
      {
        struct video_reg* reg = (struct video_reg*) list_get_item(video_regs, i);
        if (!rect_equal(&reg->rect, f_rect))
        {
          if (rect_intersect(&reg->rect, f_rect, &intersection))
          {
            no_inter = false;
            rect_substract(f_rect, &reg->rect, l_tmp);
            for (j = 0; j < l_tmp->count; j++)
            {
              r = (struct xrdp_rect*) list_get_item(l_tmp, j);
              l2_prect = (struct xrdp_rect *) g_malloc(sizeof(struct xrdp_rect), 0);
              g_memcpy(l2_prect, r, sizeof(struct xrdp_rect));
              fifo_push(f_tmp, l2_prect);
            }
            list_clear(l_tmp);
            break;
          }
        }
        else
        {
          no_inter = false;
          break;
        }
      }
      if (no_inter)
      {
        struct update_rect* urect = (struct update_rect*) g_malloc(sizeof(struct update_rect), 0);
        urect->quality = quality;
        urect->quality_already_send = -1;
        urect->rect.top = f_rect->top;
        urect->rect.left = f_rect->left;
        urect->rect.right = f_rect->right;
        urect->rect.bottom = f_rect->bottom;
        fifo_push(f_tmp2, urect);
      }
    }
    else
    {
      struct update_rect* urect = (struct update_rect*) g_malloc(sizeof(struct update_rect), 0);
      urect->quality = quality;
      urect->quality_already_send = -1;
      urect->rect.top = f_rect->top;
      urect->rect.left = f_rect->left;
      urect->rect.right = f_rect->right;
      urect->rect.bottom = f_rect->bottom;
      fifo_push(f_tmp2, urect);
    }
  }
  fifo_free(f_tmp);
  list_delete(l_tmp);
}

void update_video_regions(struct list* video, struct list* candidate, int fps)
{
  int i;
  int k;
  struct xrdp_rect intersection;

  for (i = 0; i < candidate->count; i++)
  {
    struct video_reg* cur = (struct video_reg*) list_get_item(candidate, i);
    if (cur->nb_update >= fps)
    {
      if (video->count == 0)
      {
        list_add_video_reg(video, cur->rect.left, cur->rect.top, cur->rect.right, cur->rect.bottom, cur->nb_update);
      }
      else
      {
        int vc = video->count;
        for (k = 0 ; k < vc ; k++)
        {
          struct video_reg* bb = (struct video_reg*) list_get_item(video, k);
          if (rect_intersect(&bb->rect, &cur->rect, &intersection))
          {
            bb->rect.top = MIN(bb->rect.top, cur->rect.top);
            bb->rect.left = MIN(bb->rect.left, cur->rect.left);
            bb->rect.bottom= MAX(bb->rect.bottom, cur->rect.bottom);
            bb->rect.right= MAX(bb->rect.right, cur->rect.right);
            bb->nb_update = MAX(bb->nb_update, cur->nb_update);
            break;
          }
          else
          {
            if ( ( ((cur->rect.bottom - bb->rect.top) < DELTA_VIDEO || (bb->rect.bottom - cur->rect.top) < DELTA_VIDEO) &&
                ( abs(cur->rect.left - bb->rect.left) < DELTA_VIDEO || abs(cur->rect.right - bb->rect.right) < DELTA_VIDEO) ) ||
                ( ((cur->rect.right - bb->rect.left) < DELTA_VIDEO || (bb->rect.right - cur->rect.left) < DELTA_VIDEO) &&
                    ( abs(cur->rect.top - bb->rect.top) < DELTA_VIDEO || abs(cur->rect.bottom - bb->rect.bottom) < DELTA_VIDEO)))
            {
              bb->rect.top = MIN(bb->rect.top, cur->rect.top);
              bb->rect.left = MIN(bb->rect.left, cur->rect.left);
              bb->rect.bottom= MAX(bb->rect.bottom, cur->rect.bottom);
              bb->rect.right= MAX(bb->rect.right, cur->rect.right);
              bb->nb_update = MAX(bb->nb_update, cur->nb_update);
              break;
            }
            else
            {
              list_add_video_reg(video, cur->rect.left, cur->rect.top, cur->rect.right, cur->rect.bottom, cur->nb_update);
              break;
            }
          }
        }
      }
    }
  }
  video_regions_merge(video);
}

void video_regions_merge(struct list* video)
{
  int i, j;
  bool merged = true;
  while (merged)
  {
    merged = false;
    for (j = 0; j < video->count; j++)
    {
      struct video_reg* first = (struct video_reg*) list_get_item(video, j);
      for (i = video->count - 1 ; i > j; i--)
      {
        struct video_reg* second = (struct video_reg*) list_get_item(video, i);
        if (rect_equal(&first->rect, &second->rect))
        {
          list_remove_item(video, i);
          merged = true;
          break;
        }
        if (abs(first->rect.left - second->rect.left) < DELTA_VIDEO && abs(first->rect.right - second->rect.right) < DELTA_VIDEO)
        {
          if (abs(first->rect.bottom - second->rect.top) < DELTA_VIDEO)
          {
            first->rect.bottom = MAX(first->rect.bottom, second->rect.bottom);
            list_remove_item(video, i);
            merged = true;
            break;
          }
          if (abs(first->rect.top - second->rect.bottom) < DELTA_VIDEO)
          {
            first->rect.top = MIN(first->rect.top, second->rect.top);
            list_remove_item(video, i);
            merged = true;
            break;
          }
        }
        if (abs(first->rect.top - second->rect.top) < DELTA_VIDEO && abs(first->rect.bottom - second->rect.bottom) < DELTA_VIDEO)
        {
          if (abs(first->rect.left - second->rect.right) < DELTA_VIDEO)
          {
            first->rect.left = MIN(first->rect.left, second->rect.left);
            list_remove_item(video, i);
            merged = true;
            break;
          }
          if (abs(first->rect.right - second->rect.left) < DELTA_VIDEO)
          {
            first->rect.right = MAX(first->rect.right, second->rect.right);
            list_remove_item(video, i);
            merged = true;
            break;
          }
        }
      }
      if (merged == true) break;
    }
  }
}

void update_candidate_video_regions(struct list* self, struct xrdp_rect rect, int max_fps)
{
  struct fifo* f_tmp = fifo_new();
  struct video_reg* f_vr;
  struct video_reg* tmp_vr;
  struct video_reg* vr;
  struct video_reg* tmp_vr2;
  struct xrdp_rect intersection;
  struct list* l_tmp = list_create();
  int i, j;
  bool no_inter = true;
  struct video_reg* cur_vr = (struct video_reg*) g_malloc(sizeof(struct video_reg), 1);
  cur_vr->rect = rect;
  cur_vr->nb_update = 1;
  fifo_push(f_tmp, cur_vr);
  while (!fifo_is_empty(f_tmp))
  {
    f_vr = (struct video_reg*) fifo_pop(f_tmp);
    if (self->count > 0)
    {
      no_inter = true;
      for (i = self->count - 1; i >= 0; i--)
      {
        tmp_vr = (struct video_reg*) list_get_item(self, i);
        if (!rect_equal(&tmp_vr->rect, &f_vr->rect))
        {
          if (rect_intersect(&tmp_vr->rect, &f_vr->rect, &intersection))
          {
            no_inter = false;
            video_regions_union(tmp_vr, f_vr, l_tmp, max_fps);
            for (j = 0; j < l_tmp->count; j++)
            {
              vr = (struct video_reg*) list_get_item(l_tmp, j);
              tmp_vr2 = g_malloc(sizeof(struct video_reg), 0);
              memcpy(tmp_vr2, vr, sizeof(struct video_reg));
              fifo_push(f_tmp, tmp_vr2);
            }
            list_clear(l_tmp);
            list_remove_item(self, i);
            break;
          }
        }
        else
        {
          tmp_vr->nb_update =
              (tmp_vr->nb_update + 1 > max_fps) ? max_fps : tmp_vr->nb_update + 1;
          no_inter = false;
          break;
        }
      }
      if (no_inter)
      {
        list_add_item(self, (tbus) f_vr);
      }
    }
    else
    {
      list_add_item(self, (tbus) f_vr);
    }
  }
  list_delete(l_tmp);
  fifo_free(f_tmp);
}

int video_regions_union(struct video_reg* v1, struct video_reg* v2, struct list* out, int maxfps)
{
  struct xrdp_rect in1 = v1->rect;
  struct xrdp_rect in2 = v2->rect;

  int nb_update = (MAX(v1->nb_update, v2->nb_update) + 1 > maxfps) ? maxfps : MAX(v1->nb_update, v2->nb_update) + 1;

  if (in1.left <= in2.left && in1.top <= in2.top && in1.right >= in2.right && in1.bottom >= in2.bottom)
  {
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.top, in2.left, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in2.right, in2.bottom, nb_update);
    list_add_video_reg(out, in2.right, in2.top, in1.right, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);

  }
  else if (in1.left <= in2.left && in1.right >= in2.right && in1.bottom < in2.bottom && in1.top <= in2.top)
  { /* partially covered(whole top) */
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.top, in2.left, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.bottom, nb_update);
    list_add_video_reg(out, in2.right, in2.top, in1.right, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  else if (in1.top <= in2.top && in1.bottom >= in2.bottom && in1.right < in2.right && in1.left <= in2.left)
  { /* partially covered(left) */
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.top, in2.left, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in1.right, in2.bottom, nb_update);
    list_add_video_reg(out, in1.right, in2.top, in2.right, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);
  }
  else if (in1.left <= in2.left && in1.right >= in2.right && in1.top > in2.top && in1.bottom >= in2.bottom)
  { /* partially covered(bottom) */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in2.left, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in2.right, in2.bottom, nb_update);
    list_add_video_reg(out, in2.right, in1.top, in1.right, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);
  }
  else if (in1.top <= in2.top && in1.bottom >= in2.bottom && in1.left > in2.left && in1.right >= in2.right)
  { /* partially covered(right) */
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in1.left, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in2.top, in2.right, in2.bottom, nb_update);
    list_add_video_reg(out, in2.right, in2.top, in1.right, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);
  }
  else if (in1.left <= in2.left && in1.top <= in2.top && in1.right < in2.right && in1.bottom < in2.bottom)
  { /* partially covered(top left) */
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.top, in2.left, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in1.right, in1.bottom, nb_update);
    list_add_video_reg(out, in1.right, in2.top, in2.right, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  else if (in1.left <= in2.left && in1.bottom >= in2.bottom && in1.right < in2.right && in1.top > in2.top)
  { /* partially covered(bottom left) */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in2.left, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in1.right, in2.bottom, nb_update);
    list_add_video_reg(out, in1.right, in1.top, in2.right, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);
  }
  else if (in1.left > in2.left && in1.right >= in2.right && in1.top <= in2.top && in1.bottom < in2.bottom)
  { /* partially covered(top right) */
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in1.left, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in2.top, in2.right, in1.bottom, nb_update);
    list_add_video_reg(out, in2.right, in2.top, in1.right, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  else if (in1.left > in2.left && in1.right >= in2.right && in1.top > in2.top && in1.bottom >= in2.bottom)
  { /* partially covered(bottom right) */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in1.left, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in2.right, in2.bottom, nb_update);
    list_add_video_reg(out, in2.right, in1.top, in1.right, in2.bottom, v1->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);
  }
  else if (in1.left > in2.left && in1.top <= in2.top && in1.right < in2.right && in1.bottom >= in2.bottom)
  { /* 2 rects, one on each end */
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in1.left, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in2.top, in1.right, in2.bottom, nb_update);
    list_add_video_reg(out, in1.right, in2.top, in2.right, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);
  }
  else if (in1.left <= in2.left && in1.top > in2.top && in1.right >= in2.right && in1.bottom < in2.bottom)
  { /* 2 rects, one on each end */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in2.left, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in2.right, in1.bottom, nb_update);
    list_add_video_reg(out, in2.right, in1.top, in1.right, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  else if (in1.left > in2.left && in1.right < in2.right && in1.top <= in2.top && in1.bottom < in2.bottom)
  { /* partially covered(top) */
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.top, v1->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in1.left, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in2.left, in2.top, in1.right, in1.bottom, nb_update);
    list_add_video_reg(out, in1.right, in2.top, in2.right, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  else if (in1.top > in2.top && in1.bottom < in2.bottom && in1.left <= in2.left && in1.right < in2.right)
  { /* partially covered(left) */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in2.left, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in1.right, in1.bottom, nb_update);
    list_add_video_reg(out, in1.right, in1.top, in2.right, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  else if (in1.left > in2.left && in1.right < in2.right && in1.bottom >= in2.bottom && in1.top > in2.top)
  { /* partially covered(bottom) */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in1.left, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in1.right, in2.bottom, nb_update);
    list_add_video_reg(out, in1.right, in1.top, in2.right, in2.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in2.bottom, in1.right, in1.bottom, v1->nb_update);
  }
  else if (in1.top > in2.top && in1.bottom < in2.bottom && in1.right >= in2.right && in1.left > in2.left)
  { /* partially covered(right) */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in1.left, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in2.right, in1.bottom, nb_update);
    list_add_video_reg(out, in2.right, in1.top, in1.right, in1.bottom, v1->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  else if (in1.left > in2.left && in1.top > in2.top && in1.right < in2.right && in1.bottom < in2.bottom)
  { /* totally contained, 4 rects */
    list_add_video_reg(out, in2.left, in2.top, in2.right, in1.top, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.top, in1.left, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in1.left, in1.top, in1.right, in1.bottom, nb_update);
    list_add_video_reg(out, in1.right, in1.top, in2.right, in1.bottom, v2->nb_update);
    list_add_video_reg(out, in2.left, in1.bottom, in2.right, in2.bottom, v2->nb_update);
  }
  return 1;
}

void list_add_video_reg(struct list* self, int left, int top, int right, int bottom, int nb_update)
{
  if (right != left && top != bottom)
  {
    struct video_reg* tmp = (struct video_reg*) g_malloc(sizeof(struct video_reg), 0);
    tmp->nb_update = nb_update;
    tmp->rect.top = top;
    tmp->rect.left = left;
    tmp->rect.right = right;
    tmp->rect.bottom = bottom;
    list_add_item(self, (tbus) tmp);
  }
}

void video_detection_add_update_order(struct xrdp_screen* self, struct list* update_list, struct userChannel* u)
{
  update* up;
  int i;
  int bpp = (self->bpp + 7) / 8;
  if (bpp == 3)
  {
    bpp = 4;
  }

  struct quality_params* q_params = (struct quality_params*) u->q_params;
  video_regions_merge(self->video_regs);
  if (self->video_regs->count > 0)
  {
    int w, h;
    int color;
    if (q_params->video_display_fps != 0)
    {
      for (i = 0; i < self->video_regs->count; i++)
      {
        color = 255;
        struct video_reg* vr = (struct video_reg*) list_get_item(self->video_regs, i);

        w = vr->rect.right - vr->rect.left;
        h = vr->rect.bottom - vr->rect.top;
        up = g_malloc(sizeof(update), 1);
        up->order_type = paint_update;
        up->x = vr->rect.left;
        up->y = vr->rect.top;
        up->cx = w;
        up->cy = h;
        up->data_len = up->cx * up->cy * bpp;
        up->data = g_malloc(up->data_len, 0);
        ip_image_crop(self->screen, up->x, up->y, up->cx, up->cy, up->data);
        list_add_item(update_list, (tbus) up);

        if (!vr->already_send)
        {
          vr->already_send = true;
          if (self->client_info->video_display_borders)
          {
            if (w < 6 || h < 6)
            {
              color = 33023;
              up = g_malloc(sizeof(update), 1);
              up->order_type = set_fgcolor;
              up->color = color;
              list_add_item(update_list, (tbus) up);
              up = g_malloc(sizeof(update), 1);
              up->order_type = fill_rect;
              up->x = vr->rect.left;
              up->y = vr->rect.top;
              up->width = w;
              up->height = h;
              list_add_item(update_list, (tbus) up);
            }
            else
            {
              up = g_malloc(sizeof(update), 1);
              up->order_type = set_fgcolor;
              up->color = color;
              list_add_item(update_list, (tbus) up);
              up = g_malloc(sizeof(update), 1);
              up->order_type = fill_rect;
              up->x = vr->rect.left + 1;
              up->y = vr->rect.top + 1;
              up->width = vr->rect.right - vr->rect.left - 2;
              up->height = 2;
              list_add_item(update_list, (tbus) up);
              up = g_malloc(sizeof(update), 1);
              up->order_type = fill_rect;
              up->x = vr->rect.left + 1;
              up->y = vr->rect.top + 1;
              up->width = 2;
              up->height = vr->rect.bottom - vr->rect.top - 2;
              list_add_item(update_list, (tbus) up);
              up = g_malloc(sizeof(update), 1);
              up->order_type = fill_rect;
              up->x = vr->rect.left + 1;
              up->y = vr->rect.bottom - 3;
              up->width = vr->rect.right - vr->rect.left - 2;
              up->height = 2;
              list_add_item(update_list, (tbus) up);
              up = g_malloc(sizeof(update), 1);
              up->order_type = fill_rect;
              up->x = vr->rect.right - 3;
              up->y = vr->rect.top + 1;
              up->width = 2;
              up->height = vr->rect.bottom - vr->rect.top - 2;
              list_add_item(update_list, (tbus) up);
            }
          }
        }
      }
    }
    list_clear(self->video_regs);
  }
}
