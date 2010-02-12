/**
 * Copyright (C) 2010 Ulteo SAS
 * http://www.ulteo.com
 * Author David Lechevalier <david@ulteo.com>
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

#include "user_channel.h"
#include "chansrv.h"
#include "os_calls.h"


static int g_user_channel_up = 0;
extern char* username;
extern struct log_config log_conf;
extern int g_display_num;
static struct user_channel user_channels[15];
static int channel_count = 0;


/*****************************************************************************/
int APP_CC
user_channel_send( int chan_id, char* mess, int size){
  int rv;

  rv = send_channel_data(chan_id, mess, size);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_send]: "
  		"message sended: %s", mess);
  return rv;
}

/*****************************************************************************/
int APP_CC
user_channel_cleanup(){
	return 0;
}


/*****************************************************************************/
int APP_CC
user_channel_transmit(int socket, int type, char* mess, int size )
{
  struct stream* s;
  int rv;
  make_stream(s);
  init_stream(s, 1024);
  out_uint8(s, type);
  out_uint32_be(s, size);
  out_uint8p(s, mess, size);
  s_mark_end(s);
  size = (int)(s->end - s->data);
  rv = g_tcp_send(socket, s->data, size, 0);
  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_transmit]: "
  		"message sended: %s", mess);
  if (rv != size)
  {
    log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[user_channel_transmit]: "
    		"error while sending the message: %s", mess);
  }
  free_stream(s);
	return rv;
}


/*****************************************************************************/
int APP_CC
user_channel_do_up(char* chan_name)
{
	char socket_filename[256];
	int sock = 0;
	g_sprintf(socket_filename, "/var/spool/xrdp/%i/vchannel_%s", g_display_num, chan_name);
	sock = g_create_unix_socket(socket_filename);
	g_chown(socket_filename, username);
	return sock;
}


/*****************************************************************************/
int APP_CC
user_channel_init(char* channel_name, int channel_id)
{
	int sock;
	int i;
	if (g_user_channel_up == 0)
	{
		g_user_channel_up = 1;
	}

	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_init]: "
			"new client connection for channel '%s' with id= %i ", channel_name, channel_id);
	sock = user_channel_do_up(channel_name);
	if (sock < 0)
	{
		log_message(&log_conf, LOG_LEVEL_ERROR, "chansrv[user_channel_init]: "
				"Unable to open channel %s", channel_name);
		return 0;
	}
	user_channels[channel_count].channel_id = channel_id;
	g_strcpy(user_channels[channel_count].channel_name, channel_name);
	user_channels[channel_count].server_channel_socket = sock;
	channel_count++;
	return 0;
}

/*****************************************************************************/
int APP_CC
user_channel_deinit(void)
{
	char status[1];
	int i;
	int socket;
	status[0] = STATUS_DISCONNECTED;

	for (i=0 ; i<channel_count ; i++ )
	{
		socket = user_channels[i].client_channel_socket;
		user_channel_transmit(socket, SETUP_MESSAGE, status, 1 );
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
user_channel_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length)
{
	int i;
	int size;
  char buf[1024];

	for(i=0; i<channel_count; i++)
	{
		if( user_channels[i].channel_id == chan_id )
		{
			size = s->end - s->data;
			//g_printf("size : %i\n", size);
			g_strncpy(buf, (char*)s->p, size+1);
		  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_data_in]: "
		  		"new client message for channel %s ",user_channels[i].channel_name);
			log_hexdump(&log_conf, LOG_LEVEL_DEBUG, (unsigned char*)buf, size);
			if(user_channels[i].client_channel_socket == 0 )
			{
			  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_data_in]: "
			  		"server side channel is not opened");
			  s->p+=s->size+1;
				return 0;
			}
			user_channel_transmit(user_channels[i].client_channel_socket, DATA_MESSAGE, buf, size );
			return 0;
		}
	}

  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_data_in]: "
  		"the channel id %i is invalid",chan_id);

	return 0;
}

/*****************************************************************************/
int APP_CC
user_channel_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  int lcount;
  int i;

  if ((!g_user_channel_up) || (objs == 0) || (count == 0))
  {
    return 0;
  }
  lcount = *count;
  for (i=0 ; i<channel_count ; i++)
  {
  	if( user_channels[i].server_channel_socket != 0)
  	{
  		objs[lcount] = user_channels[i].server_channel_socket;
  		lcount++;
  	}
  	if( user_channels[i].client_channel_socket != 0)
  	{
  	  objs[lcount] = user_channels[i].client_channel_socket;
  	  lcount++;
  	}
  }
  *count = lcount;
  return 0;
}

/************************************************************************/
int DEFAULT_CC
user_channel_process_channel_opening(int channel_index, int client)
{
	struct stream* s;
  int data_length;
  int size;
  int type;

	make_stream(s);
	init_stream(s, 1024);

	size = g_tcp_recv(client, s->data, sizeof(int)+1, 0);

	if ( size < 1)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"enable to get information on the opening channel");
		g_tcp_close(client);
		free_stream(s);
		return 0;
	}
	in_uint8(s, type);
	if(type != CHANNEL_OPEN)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"Invalid operation type");
		free_stream(s);
		return 0;
	}
	in_uint32_be(s, data_length);
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
			"Data_length : %i\n", data_length);
	size = g_tcp_recv(client, s->data, data_length, 0);
	s->data[data_length] = 0;
	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
			"Channel name : %s\n",s->data);

	if(g_strcmp(user_channels[channel_index].channel_name, s->data) == 0)
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"New server connection for channel %s ", user_channels[channel_index].channel_name);
		user_channels[channel_index].client_channel_socket = client;
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"Socket : %i",user_channels[channel_index].channel_id);
	}
	else
	{
		log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
				"Unable to open a channel without client");
	}
	free_stream(s);
	return 0;
}



/*****************************************************************************/
int APP_CC
user_channel_check_wait_objs(void)
{
  struct stream* s;
  int data_length;
	int size;
	int i;
	int new_client;
	int test;
	int type;

	for( i=0 ; i<channel_count; i++)
	{
		test = g_is_wait_obj_set(user_channels[i].server_channel_socket);
		if (test)
	  {
			new_client = g_wait_connection(user_channels[i].server_channel_socket);
			user_channel_process_channel_opening(i, new_client);
			return 0;
		}
	}
	for( i=0 ; i<channel_count ; i++)
  {
		test = g_is_wait_obj_set(user_channels[i].client_channel_socket);
		if (test)
		{
		  make_stream(s);
			init_stream(s, 1024);
			size = g_tcp_recv(user_channels[i].client_channel_socket, s->data, sizeof(int)+1, 0);
  	  if ( size < 1)
			{
	  	  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
	  	  		"channel %s closed : \n",user_channels[i].channel_name);
				g_tcp_close(user_channels[i].client_channel_socket);
				user_channels[i].client_channel_socket = 0;
				free_stream(s);
				continue;
			}
  		in_uint8(s, type);
  		if(type != DATA_MESSAGE)
  		{
  			log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_process_channel_opening]: "
  					"Invalid operation type");
  			free_stream(s);
  			return 0;
  		}
  	  in_uint32_be(s,data_length);
  	  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
  	  		"data_length : %i\n", data_length);
  	  size = g_tcp_recv(user_channels[i].client_channel_socket, s->data, data_length, 0);
  	  s->data[data_length] = 0;
  	  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
  	  		"data : %s\n",s->data);
  	  log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
  	  		"data received : %s",s->data);
  	  if( user_channels[i].channel_id == -1)
  	  {
  	  	log_message(&log_conf, LOG_LEVEL_DEBUG, "chansrv[user_channel_check_wait_objs]: "
  	  			"client channel is not opened");
  	  	free_stream(s);
  	  	return 0 ;
  	  }
  	  user_channel_send(user_channels[i].channel_id, s->data, size);
		}
		free_stream(s);
  }
	return 0;
}

