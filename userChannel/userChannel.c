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
  LIB_DEBUG(u, "lib_userChannel_mod_signal");
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
  u->server_width = w;
  u->server_height = h;
  u->server_bpp = bpp;

  if (u->mod)
  {
    u->mod->mod_start(u->mod, w, h, bpp);
  }

  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_userChannel_mod_connect(struct userChannel* u)
{
  LIB_DEBUG(u, "lib_userChannel_mod_connect");
  if (u->mod)
  {
    u->mod->mod_connect(u->mod);
  }

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

  return u->mod->mod_get_wait_objs(u->mod, read_objs, rcount, write_objs, wcount, timeout);
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_userChannel_mod_check_wait_objs(struct userChannel* u)
{
  LIB_DEBUG(u, "lib_userChannel_mod_check_wait_objs");

  return u->mod->mod_check_wait_objs(u->mod);
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

  return u;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(struct userChannel* u)
{
  LIB_DEBUG(v, "mod_exit");

  if (u == 0)
  {
    return 0;
  }

  if (u->mod_lib_exit)
  {
    u->mod_lib_exit(u->mod);
  }

  g_free(u);
  return 0;
}
