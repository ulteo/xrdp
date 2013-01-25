/**
 * Copyright (C) 2013 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2013
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
#include "xrdp.h"
#include "abstract/xrdp_module.h"


struct xrdp_qos* DEFAULT_CC
xrdp_qos_create(struct xrdp_process* process, struct xrdp_user_channel* user_channel)
{
  struct xrdp_qos* result = g_malloc(sizeof(struct xrdp_qos), true);

  if (!process || !user_channel)
  {
    printf("Failed to initialize qos object, [process: %x, user_channel:%x]\n", process, user_channel);
    return;
  }

  if (result == NULL)
  {
    printf("Failed to initialize qos object, unable to allocate object\n");
    return NULL;
  }

  result->process = process;
  result->mod = user_channel;

  return result;
}

void DEFAULT_CC
xrdp_qos_delete(struct xrdp_qos* qos)
{
  g_free(qos);
}

THREAD_RV THREAD_CC
xrdp_qos_loop(void* in_val)
{
  struct xrdp_qos* qos = (struct xrdp_qos*)in_val;
  struct xrdp_process* process;
  int timeout = 1000;
  int robjs_count;
  int wobjs_count;
  tbus robjs[32];
  tbus wobjs[32];
  int current_time = 0;
  int last_update_time = 0;
  int connectivity_check_interval;


  if (qos == NULL)
  {
    printf("Failed to initialize qos thread\n");
    return 1;
  }

  process = qos->process;
  connectivity_check_interval = process->session->client_info->connectivity_check_interval * 1000;
printf("process -> cont %i\n", process->cont);
  while(process->cont)
  {
    /* build the wait obj list */
    timeout = 1000;
    robjs_count = 0;
    wobjs_count = 0;
    process->mod->get_data_descriptor(process->mod, robjs, &robjs_count, wobjs, &wobjs_count, &timeout);
    if (process->session->client_info->connectivity_check)
    {
      current_time =  g_time2();
      if (current_time - process->server_trans->last_time >= connectivity_check_interval)
      {
        libxrdp_send_keepalive(process->session);
        process->server_trans->last_time = current_time;
        if (g_tcp_socket_ok(process->server_trans->sck) == 0)
        {
          printf("Connection end due to keepalive\n");
          break;
        }
      }
    }
    // Waiting for new data
    if (g_obj_wait(robjs, robjs_count, wobjs, wobjs_count, timeout) != 0)
    {
      /* error, should not get here */
      g_sleep(100);
    }
    // TODO really get the data here
    if (process->mod->get_data(process->mod) != 0)
    {
      break;
    }
  }

  if( process->mod != 0)
  {
    process->mod->disconnect(process->mod);
  }

  libxrdp_disconnect(process->session);
  return 0;
}
