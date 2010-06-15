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
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file scp_v0.c
 * @brief scp version 0 implementation
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "sesman.h"

extern struct config_sesman* g_cfg; /* in sesman.c */
static tbus session_creation_lock;

void DEFAULT_CC
scp_init_mutex()
{
	session_creation_lock = tc_mutex_create();
}

void DEFAULT_CC
scp_remove_mutex()
{
	tc_mutex_delete(session_creation_lock);
}

/******************************************************************************/
void DEFAULT_CC
scp_v0_process(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  int display = 0;
  tbus data;
  struct session_item* s_item;

  tc_mutex_lock(session_creation_lock);
  data = auth_userpass(s->username, s->password);

  if (data)
  {
    s_item = session_get_bydata(s->username);
    if (s_item != 0)
    {
      display = s_item->display;
      auth_end(data);
      if (s_item->status == SESMAN_SESSION_STATUS_TO_DESTROY)
      {
      	log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "Session for user %s is in destroy, unable to initialize a new session");
      	scp_v0s_deny_connection(c, "Your last session is currently \nended, retry later");
      }
      else
      {
      	session_update_status_by_user(s->username, SESMAN_SESSION_STATUS_ACTIVE);
      	log_message(&(g_cfg->log), LOG_LEVEL_INFO, "switch from status DISCONNECTED to ACTIVE");
      }
    }
    else
    {
      LOG_DBG(&(g_cfg->log), "pre auth");
      if (1 == access_login_allowed(s->username))
      {
        log_message(&(g_cfg->log), LOG_LEVEL_INFO, "granted TS access to user %s", s->username);
        if (SCP_SESSION_TYPE_XVNC == s->type)
        {
          log_message(&(g_cfg->log), LOG_LEVEL_INFO, "starting Xvnc session...");
          display = session_start(s->width, s->height, s->bpp, s->username,
                                  s->password, data, SESMAN_SESSION_TYPE_XVNC,
                                  s->domain, s->program, s->directory, s->keylayout);
        }
        else
        {
          log_message(&(g_cfg->log), LOG_LEVEL_INFO, "starting X11rdp session...");
          display = session_start(s->width, s->height, s->bpp, s->username,
                                  s->password, data, SESMAN_SESSION_TYPE_XRDP,
                                  s->domain, s->program, s->directory, s->keylayout);
        }
      }
      else
      {
        display = 0;
      }
    }
    if (display == 0)
    {
      auth_end(data);
      data = 0;
      log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "User %s are not allow to start session", s->username);
      scp_v0s_deny_connection(c, "You are not allow to start \nsession");
    }
    else
    {
      scp_v0s_allow_connection(c, display);
    }
  }
  else
  {
    log_message(&(g_cfg->log), LOG_LEVEL_WARNING, "User %s failed to authenticate", s->username);
  	scp_v0s_deny_connection(c, "Your username or \nyour password is invalid");
  }
  tc_mutex_unlock(session_creation_lock);
}

