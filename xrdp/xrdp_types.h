/**
 * Copyright (C) 2011 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2011
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

/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2009

   types

*/

/* lib */


struct xrdp_process;
struct xrdp_wm;
typedef bool (*init_func_pointer)(struct xrdp_process*);
typedef int (*exit_func_pointer)(struct xrdp_process* process);
typedef int (*get_data_descriptor_func_pointer)(struct xrdp_wm* self, tbus* robjs, int* rc, tbus* wobjs, int* wc, int* timeout);
typedef int (*get_data_func_pointer)(struct xrdp_wm* self);
typedef int (*disconnect_func_pointer)(struct xrdp_wm* self);
typedef int (*end_funct_pointer)(struct xrdp_wm* self);
typedef struct xrdp_wm* (*connect_func_pointer)(struct xrdp_process*);
typedef int (*callback_funct_pointer)(long id, int msg, long param1, long param2, long param3, long param4);
typedef bool (*is_term_func_pointer)();


struct xrdp_user_channel
{
  tbus handle;
  init_func_pointer init;
  exit_func_pointer exit;

  // module functions
  get_data_descriptor_func_pointer get_data_descriptor;
  get_data_func_pointer get_data;
  connect_func_pointer connect;
  disconnect_func_pointer disconnect;
  callback_funct_pointer callback;
  end_funct_pointer end;

  // xrdp function
  is_term_func_pointer is_term;
};

/* rdp process */
struct xrdp_process
{
  int status;
  struct trans* server_trans; /* in tcp server mode */
  tbus self_term_event;
  struct xrdp_listen* lis_layer; /* owner */
  struct xrdp_session* session;
  /* create these when up and running */
  struct xrdp_wm* wm;
  struct xrdp_user_channel* mod;
  //int app_sck;
  tbus done_event;
  int session_id;
  int cont;
};

/* rdp listener */
struct xrdp_listen
{
  int status;
  struct trans* listen_trans; /* in tcp listen mode */
  struct list* process_list;
  tbus pro_done_event;
};

