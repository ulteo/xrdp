/**
 * Copyright (C) 2009 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com>
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

#include "vchannel.h"


extern struct log_config l_config;


int APP_CC
vchannel_try_open(Vchannel* channel)
{
	int sock;
	int len;
	struct sockaddr_un saun;
	char socket_filename[256];

	/* Create socket */
	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
			" : Creating socket");
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		log_message(&l_config, LOG_LEVEL_ERROR ,"vchannel[vchannel_try_open]"
				" : Unable to create socket: %s",strerror(errno));
		return ERROR;
	}
	sprintf(socket_filename, "%s%s", VCHANNEL_SOCKET_PATH, (char*)getenv("DISPLAY"));
	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
			" : Socket name : %s",socket_filename);

	/* Connect to server */
	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, socket_filename);
	len = sizeof(saun.sun_family) + strlen(saun.sun_path);
	if (connect(sock, (struct sockaddr *) &saun, len) < 0)
	{
		log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
				" : Unable to connect to vchannel server: %s",strerror(errno));
		close(sock);
		return ERROR;
	}
	struct stream* s;
	make_stream(s);
	init_stream(s, 256);
	out_uint8p(s, channel->name, strlen(channel->name));
	vchannel_send(channel, s);
	free_stream(s);
	return sock;
}

/*****************************************************************************/
int APP_CC
vchannel_open(Vchannel *channel)
{
	int count = 0;
	int sock;
	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_open]"
			" : Try to open channel %s", count, channel->name);
	while( count < VCHANNEL_OPEN_RETRY_ATTEMPT)
	{
		sock = vchannel_open(channel);
		if(sock == ERROR)
		{
			log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_open]"
					" : Attempt %i failed on channel %s", count, channel->name);
			sleep(1);
			count++;
		}
		else
		{
			break;
		}
	}
	channel->sock = sock;
	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_open]"
			" : Channel '%s' openned", channel->name);
	return 0;
}

/*****************************************************************************/
int APP_CC
vchannel_send(Vchannel* channel, struct stream *s)
{
	char size_msg[4] = {0};
	int size = s->end - s->data;
	*((unsigned int*)size_msg) = size;

	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_send]"
			" : message send to channel %s: ", channel->name);
	log_hexdump(&l_config, LOG_LEVEL_DEBUG, s->data, size);

	if(write(channel->sock, size_msg, 4) < 4)
	{
		log_message(&l_config, LOG_LEVEL_ERROR ,"vchannel[vchannel_send]"
				" : Failed to send message size to channel %s", channel->name);
		return ERROR;
	}
	if(write(channel->sock, s->data, size) < size)
	{
		log_message(&l_config, LOG_LEVEL_ERROR ,"vchannel[vchannel_send]"
				" : Failed to send message data to channel %s", channel->name);
		return ERROR;
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
vchannel_receive(Vchannel* channel, struct stream *s)
{
	char size_msg[4] = {0};
	int nb_read = 0;
	int size;

	nb_read = read(channel->sock, size_msg, 4);
	if( nb_read != 4)
	{
		log_message(&l_config, LOG_LEVEL_ERROR ,"vchannel[vchannel_receive]"
				" : Error while receiving data lenght: %s",strerror(errno));
		size = *((unsigned int*) size_msg);
		return ERROR;
	}
	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_receive]"
			"Size of message : %i", size);
	nb_read = read( channel->sock, size_msg, size);
	if(nb_read != size){
		log_message(&l_config, LOG_LEVEL_ERROR ,"vchannel[vchannel_receive]"
				" : Data length is invalid");
		return ERROR;
	}
	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_receive]"
			" : message received from channel %s: ", channel->name);
	log_hexdump(&l_config, LOG_LEVEL_DEBUG, s->data, size);
	return 0;
}

/*****************************************************************************/
int APP_CC
vchannel_close(Vchannel* channel)
{
	log_message(&l_config, LOG_LEVEL_DEBUG ,"vchannel[vchannel_close]"
			" : Channel '%s' closed", channel->name);
	return close(channel->sock);
}

/*****************************************************************************/
int APP_CC
vchannel_read_logging_conf(struct log_config* log_conf)
{
  char filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;

  log_conf->program_name = (char*)g_strdup("channel_program");
  log_conf->log_file = 0;
  log_conf->fd = 0;
  log_conf->log_level = LOG_LEVEL_DEBUG;
  log_conf->enable_syslog = 0;
  log_conf->syslog_level = LOG_LEVEL_DEBUG;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_snprintf(filename, 255, "%s/channel.ini", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, VCHAN_CFG_LOGGING, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, VCHAN_CFG_LOG_FILE))
      {
        log_conf->log_file = (char*)g_strdup(value);
      }
      if (0 == g_strcasecmp(name, VCHAN_CFG_LOG_LEVEL))
      {
        log_conf->log_level = log_text2level(value);
      }
      if (0 == g_strcasecmp(name, VCHAN_CFG_LOG_ENABLE_SYSLOG))
      {
        log_conf->enable_syslog = log_text2bool(value);
      }
      if (0 == g_strcasecmp(name, VCHAN_CFG_LOG_SYSLOG_LEVEL))
      {
        log_conf->syslog_level = log_text2level(value);
      }
    }
  }
  list_delete(names);
  list_delete(values);
  return 0;
}
