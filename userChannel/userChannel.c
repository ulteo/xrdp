/*
 * Copyright (C) 2012 userChannel SAS
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

#include "userChannel.h"
#include "proxy.h"
#include <stdlib.h>
#include <sys/eventfd.h>
#include <time.h>
#include <list.h>

struct userChannel* u;


/******************************************************************************/
int DEFAULT_CC
lib_userChannel_mod_event(struct userChannel* u, int msg, long param1, long param2,
              long param3, long param4)
{
  LIB_DEBUG(u, "lib_userChannel_mod_event");
  if (u->mod)
  {
    u->mod->mod_event(u->mod, msg, param1, param2, param3, param4);
  }

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
lib_userChannel_mod_signal(struct userChannel* u)
{
  LIB_DEBUG(u, "lib_userChannel_mod_signal\n");
  if (u->mod)
  {
    u->mod->mod_signal(u->mod);
  }

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
lib_userChannel_mod_start(struct userChannel* u, int w, int h, int bpp)
{
  LIB_DEBUG(u, "lib_userChannel_mod_start");
  u->do_start = true;

  u->server_width = w;
  u->server_height = h;
  u->server_bpp = bpp;

  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_userChannel_mod_connect(struct userChannel* u)
{
  LIB_DEBUG(u, "lib_userChannel_mod_connect");
  u->do_connection = true;

  return 0;
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

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
lib_userChannel_mod_set_param(struct userChannel* u, char* name, char* value)
{
  LIB_DEBUG(u, "lib_userChannel_mod_set_param");

  if (u->mod)
  {
    u->mod->mod_set_param(u->mod, name, value);
  }

  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_userChannel_mod_get_wait_objs(struct userChannel* u, tbus* read_objs, int* rcount,
                      tbus* write_objs, int* wcount, int* timeout)
{
  LIB_DEBUG(u, "lib_userChannel_mod_get_wait_objs");

  int i = *rcount;
  if (u != 0)
  {
    if (u->efd != 0)
    {
      read_objs[i++] = u->efd;
    }
  }
  *rcount = i;

  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_userChannel_mod_check_wait_objs(struct userChannel* u)
{
  LIB_DEBUG(u, "lib_userChannel_mod_check_wait_objs");
  char buf[8] = {0};

  if (u->terminate)
  {
    return 1;
  }

  tc_mutex_lock(u->update_list_mutex);
  if (u->last_update_list->count > 0) {
    int i;

    for(i = 0 ; i < u->last_update_list->count ; i++)
    {
      update* up = (update*)list_get_item(u->last_update_list, i);
      switch (up->order_type)
      {
      case begin_update:
        u->server_begin_update(u);
        break;

      case end_update:
        u->server_end_update(u);
        break;

      case reset:
        u->server_reset(u, up->width, up->height, up->bpp);
        break;

      case reset_clip:
        u->server_reset_clip(u);
        break;

      case set_clip:
        u->server_set_clip(u, up->x, up->y, up->cx, up->cy);
        break;

      case fill_rect:
        u->server_fill_rect(u, up->x, up->y, up->width, up->height);
        break;

      case paint_rect:
        u->server_paint_rect(u, up->x, up->y, up->cx, up->cy, up->data, up->width, up->height, up->srcx, up->srcy);
        g_free(up->data);
        break;

      case draw_line:
        u->server_draw_line(u, up->srcx, up->srcy, up->x, up->y);
        g_free(up->data);
        break;

      case screen_blt:
        u->server_screen_blt(u, up->x, up->y, up->cx, up->cy, up->srcx, up->srcy);
        break;

      case set_cursor:
        u->server_set_cursor(u, up->x, up->y, up->data, up->mask);
        g_free(up->data);
        g_free(up->mask);
        break;

      case set_fgcolor:
        u->server_set_fgcolor(u, up->color);
        break;

      case set_bgcolor:
        u->server_set_bgcolor(u, up->color);
        break;

      case set_opcode:
        u->server_set_opcode(u, up->opcode);
        break;

      case set_mixmode:
        u->server_set_mixmode(u, up->mixmode);
        break;

      case send_to_channel:
        u->server_send_to_channel(u, up->channel_id, up->data, up->data_len, up->total_data_len, up->flags);
        g_free(up->data);
        break;

      default:
        printf("order is not known %lu\n", up->order_type);
        break;
      }
    }
    list_clear(u->last_update_list);
  }
  tc_mutex_unlock(u->update_list_mutex);

  u->need_request = true;

  return 0;
}

/*****************************************************************************/
int APP_CC
lib_userChannel_load_library(struct userChannel* self)
{
  void* func;
  char text[256];

  if (self == 0)
  {
    return 1;
  }

  if (self->mod_handle == 0)
  {
    self->mod_handle = g_load_library(LIBRARY_NAME);
    if (self->mod_handle != 0)
    {
      func = g_get_proc_address(self->mod_handle, "mod_init");
      if (func == 0)
      {
        func = g_get_proc_address(self->mod_handle, "_mod_init");
      }
      if (func == 0)
      {
        DEBUG(("problem loading lib in lib_load_library"));
        return 1;
      }
      self->mod_lib_init = (struct xrdp_mod* (*)(void))func;

      func = g_get_proc_address(self->mod_handle, "mod_exit");
      if (func == 0)
      {
        func = g_get_proc_address(self->mod_handle, "_mod_exit");
      }
      if (func == 0)
      {
        DEBUG(("problem loading lib in lib_load_library"));
        return 1;
      }
      self->mod_lib_exit = (int (*)(struct xrdp_mod*))func;
    }

    self->mod = (struct xrdp_mod*)self->mod_lib_init();
    if (self->mod != 0)
    {
      g_writeln("loaded modual '%s' ok, interface size %d, version %d", LIBRARY_NAME, self->size, self->version);
    }
    if (self->mod != 0)
    {
      self->mod->wm = (long)(self->wm);
      self->mod->server_begin_update = lib_userChannel_proxy_server_begin_update;
      self->mod->server_end_update = lib_userChannel_proxy_server_end_update;
      self->mod->server_fill_rect = lib_userChannel_proxy_server_fill_rect;
      self->mod->server_screen_blt = lib_userChannel_proxy_server_screen_blt;
      self->mod->server_paint_rect = lib_userChannel_proxy_server_paint_rect;
      self->mod->server_set_fgcolor = lib_userChannel_proxy_server_set_fgcolor;
      self->mod->server_set_pointer = lib_userChannel_proxy_server_set_pointer;
      self->mod->server_msg = lib_userChannel_proxy_server_msg;
      self->mod->server_is_term = lib_userChannel_proxy_server_is_term;
      self->mod->server_reset = lib_userChannel_proxy_server_reset;
      self->mod->server_palette = lib_userChannel_proxy_server_palette;
      self->mod->server_set_clip = lib_userChannel_proxy_server_set_clip;
      self->mod->server_reset_clip = lib_userChannel_proxy_server_reset_clip;
      self->mod->server_set_bgcolor = lib_userChannel_proxy_server_set_bgcolor;
      self->mod->server_set_opcode = lib_userChannel_proxy_server_set_opcode;
      self->mod->server_set_mixmode = lib_userChannel_proxy_server_set_mixmode;
      self->mod->server_set_brush = lib_userChannel_proxy_server_set_brush;
      self->mod->server_set_pen = lib_userChannel_proxy_server_set_pen;
      self->mod->server_draw_line = lib_userChannel_proxy_server_draw_line;
      self->mod->server_add_char = lib_userChannel_proxy_server_add_char;
      self->mod->server_draw_text = lib_userChannel_proxy_server_draw_text;
      self->mod->server_query_channel = lib_userChannel_proxy_server_query_channel;
      self->mod->server_get_channel_id = lib_userChannel_proxy_server_get_channel_id;
      self->mod->server_send_to_channel = lib_userChannel_proxy_server_send_to_channel;
    }
  }

  /* id self->mod is null, there must be a problem */
  if (self->mod == 0)
  {
    DEBUG(("problem loading lib in xrdp_mm_setup_mod1"));
    return 1;
  }
  return 0;
}

void *lib_ulteo_thread_run(void *arg)
{
  struct timespec tim, tim2;
  char buf[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
  tbus read_objs;
  tbus write_objs;
  int wcount = 0;
  int rcount = 0;
  int timeout = 1000;

  while (! u->terminate)
  {
    if (u->do_start && u->mod != NULL)
    {
      u->do_start = false;
      u->mod->mod_start(u->mod, u->server_width, u->server_height, u->server_bpp);
    }

    if (u->do_connection && u->mod != NULL) {
      u->do_connection = false;
      u->mod->mod_connect(u->mod);
      u->connected = true;
    }

    if (u->server_is_term(u)) {
      break;
    }

    if (!u->need_request || !u->connected) {
      usleep(10000);
      continue;
    }

    rcount = 0;
    wcount = 0;
    u->mod->mod_get_wait_objs(u->mod, &read_objs, &rcount, &write_objs, &wcount, &timeout);
    if (g_obj_wait(&read_objs, rcount, &write_objs, wcount, timeout) != 0)
    {
      /* error, should not get here */
      g_sleep(100);
    }

    if (g_tcp_can_recv(read_objs, 0)) {
      struct list* temp;
      tc_mutex_lock(u->update_list_mutex);
      if (u->mod->mod_check_wait_objs(u->mod) == 1)
      {
        tc_mutex_unlock(u->update_list_mutex);
        u->terminate = 1;
        break;
      }
      u->need_request = false;
      write(u->efd, buf, sizeof(buf));

      // swaping update list
      temp = u->current_update_list;
      u->current_update_list = u->last_update_list;
      u->last_update_list = temp;

      tc_mutex_unlock(u->update_list_mutex);
    }
  }
}

/******************************************************************************/
struct userChannel* EXPORT_CC
mod_init(void)
{
  u = (struct userChannel*)g_malloc(sizeof(struct userChannel), 1);
  u->size = sizeof(struct userChannel);
  u->version = CURRENT_MOD_VER;
  u->handle = (long)u;
  u->mod_connect = lib_userChannel_mod_connect;
  u->mod_start = lib_userChannel_mod_start;
  u->mod_event = lib_userChannel_mod_event;
  u->mod_signal = lib_userChannel_mod_signal;
  u->mod_end = lib_userChannel_mod_end;
  u->mod_set_param = lib_userChannel_mod_set_param;
  u->mod_get_wait_objs = lib_userChannel_mod_get_wait_objs;
  u->mod_check_wait_objs = lib_userChannel_mod_check_wait_objs;

  lib_userChannel_load_library(u);

  u->efd = eventfd(1, 0);
  u->terminate = 0;
  u->need_request = true;

  u->current_update_list = list_create();
  u->current_update_list->auto_free = true;

  u->last_update_list = list_create();
  u->last_update_list->auto_free = true;

  u->connected = false;
  u->do_connection = false;
  u->do_start = false;
  u->update_list_mutex = tc_mutex_create();

  pthread_create(&u->thread, NULL, lib_ulteo_thread_run, 0);

  return u;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(struct userChannel* u)
{
  if (u == 0)
  {
    return 0;
  }

  if (u->mod_lib_exit)
  {
    u->mod_lib_exit(u->mod);
  }

  u->terminate = 1;
  pthread_join(u->thread, NULL);
  close(u->efd);

  g_free(u);
  return 0;
}
