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


int APP_CC
vchannel_try_open(Vchannel* channel)
{
	int sock;
	int len;
	struct sockaddr_un saun;
	char socket_filename[256];
	int display_num;

	display_num = g_get_display_num_from_display(getenv("DISPLAY"));
	if(display_num == 0)
	{
		log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
				" : Display must be different of 0");
		return ERROR;
	}

	/* Create socket */
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
			" : Creating socket");
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		log_message(channel->log_conf, LOG_LEVEL_ERROR ,"vchannel[vchannel_try_open]"
				" : Unable to create socket: %s",strerror(errno));
		return ERROR;
	}

	sprintf(socket_filename, "%s/%i/vchannel_%s", VCHANNEL_SOCKET_DIR, display_num, channel->name);
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
			" : Socket name : %s",socket_filename);
	/* Connect to server */
	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, socket_filename);
	len = sizeof(saun.sun_family) + strlen(saun.sun_path);
	if (connect(sock, (struct sockaddr *) &saun, len) < 0)
	{
		log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
				" : Unable to connect to vchannel server: %s",strerror(errno));
		close(sock);
		return ERROR;
	}
	channel->sock = sock;
	struct stream* s;
	make_stream(s);
	init_stream(s, 256);
	out_uint8p(s, channel->name, strlen(channel->name));
	s_mark_end(s);
	if (_vchannel_send(channel,CHANNEL_OPEN, s, strlen(channel->name)) == ERROR)
	{
		log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_try_open]"
				" : Unable to send data on channel '%s'",channel->name);
		free_stream(s);
		vchannel_close(channel);
		return ERROR;
	}
	free_stream(s);
	return 0;
}

/*****************************************************************************/
int APP_CC
vchannel_open(Vchannel *channel)
{
	int count = 0;
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_open]: "
			" : Try to open channel '%s'", channel->name);
	while( count < VCHANNEL_OPEN_RETRY_ATTEMPT)
	{
		if(vchannel_try_open(channel) == ERROR)
		{
			log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_open]: "
					" : Attempt %i failed on channel '%s'", count, channel->name);
			sleep(1);
			count++;
		}
		else
		{
			log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_open]: "
					" : Channel '%s' openned", channel->name);
			return 0;
		}
	}
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_open]: "
			"Failed to open channel '%s' [timeout reached]", channel->name);
	return ERROR;
}

int APP_CC
vchannel_send(Vchannel* channel, struct stream *s, int length)
{
	return _vchannel_send(channel, DATA_MESSAGE, s, length);
}


/*****************************************************************************/
int APP_CC
_vchannel_send(Vchannel* channel, int type, struct stream *s, int length)
{
	struct stream* header;
	if(channel->sock < 0)
	{
		log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_send]: "
				"The channel %s is not opened ", channel->name);
		return ERROR;
	}
	make_stream(header);
	init_stream(header, 5);
	out_uint8(header, type);
	out_uint32_be(header, length);
	s_mark_end(header);
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_send]: "
			"Message send to channel '%s': ", channel->name);
	log_hexdump(channel->log_conf, LOG_LEVEL_DEBUG, header->data, 4);
	if(write(channel->sock, header->data, 5) < 5)
	{
		log_message(channel->log_conf, LOG_LEVEL_ERROR ,"vchannel[vchannel_send]: "
				"Failed to send message size to channel %s [%s]", channel->name, strerror(errno));
		free_stream(header);
		return ERROR;
	}
	free_stream(header);
	if(write(channel->sock, s->data, length) < length)
	{
		log_message(channel->log_conf, LOG_LEVEL_ERROR ,"vchannel[vchannel_send]: "
				"Failed to send message data to channel %s [%s]", channel->name, strerror(errno));
		return ERROR;
	}
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_send]: "
			"Message send to channel '%s': ", channel->name);
	log_hexdump(channel->log_conf, LOG_LEVEL_DEBUG, s->data, length);

	return 0;
}

/*****************************************************************************/
int APP_CC
vchannel_receive(Vchannel* channel, struct stream *s, int* length, int* total_length)
{
	struct stream* header;
	int nb_read = 0;
	int type;
	int rv;

	if(channel->sock < 0)
	{
		log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_send]: "
				"The channel %s is not opened ", channel->name);
		return ERROR;
	}
	make_stream(header);
	init_stream(header, 9);
	nb_read = g_tcp_recv(channel->sock, header->data, 9, 0);
	if( nb_read != 9)
	{
		log_message(channel->log_conf, LOG_LEVEL_ERROR ,"vchannel[vchannel_receive]"
				" : Error while receiving data lenght: %s",strerror(errno));
		return ERROR;
	}
	in_uint8(header, type);
	in_uint32_be(header, *length);
	in_uint32_be(header, *total_length);
	switch(type)
	{
	case CHANNEL_OPEN:
		log_message(channel->log_conf, LOG_LEVEL_WARNING ,"vchannel[vchannel_receive]"
				" : CHANNEL OPEN message is invalid message");
		rv = ERROR;
		break;
	case SETUP_MESSAGE:
		log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_receive]"
				" : status change message");
		rv = 0;
		break;
	case DATA_MESSAGE:
		log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_receive]"
				" : data message");
		rv = 0;
		break;
	default:
		log_message(channel->log_conf, LOG_LEVEL_ERROR ,"vchannel[vchannel_receive]"
				" : Invalid message '%02x'", type);
		rv = ERROR;
		break;
	}
	if( rv == ERROR)
	{
		return ERROR;
	}
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_receive]"
			"Size of message : %i", *length);
	init_stream(s, *length);
	nb_read = g_tcp_recv(channel->sock, s->data, *length, 0);
	if(nb_read != *length)
	{
		log_message(channel->log_conf, LOG_LEVEL_ERROR ,"vchannel[vchannel_receive]"
				" : Data length is invalid");
		return ERROR;
	}
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_receive]"
			" : message received from channel %s: ", channel->name);
	log_hexdump(channel->log_conf, LOG_LEVEL_DEBUG, s->data, *length);
	if (type == SETUP_MESSAGE)
	{
		return s->data[0];
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
vchannel_close(Vchannel* channel)
{
	log_message(channel->log_conf, LOG_LEVEL_DEBUG ,"vchannel[vchannel_close]"
			" : Channel '%s' closed", channel->name);
	return close(channel->sock);
}

/*****************************************************************************/
int APP_CC
vchannel_read_logging_conf(struct log_config* log_conf, const char* chan_name)
{
  char filename[256];
  char log_filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;
  log_conf->program_name = (char*)g_strdup(chan_name);
  log_conf->log_file = 0;
  log_conf->fd = 0;
  log_conf->log_level = LOG_LEVEL_DEBUG;
  log_conf->enable_syslog = 0;
  log_conf->syslog_level = LOG_LEVEL_DEBUG;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_snprintf(filename, 255, "%s/vchannel.ini", XRDP_CFG_PATH);
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
    if( g_strlen(log_conf->log_file) > 1)
    {

    	sprintf(log_filename, "%s/%i/vchannel_%s.log",
    			log_conf->log_file,
    			g_get_display_num_from_display(getenv("DISPLAY")),
    			log_conf->program_name);
    	log_conf->log_file = (char*)g_strdup(log_filename);
    }
  }
  else
  {
  	g_printf("Invalid channel configuration file : %s\n",filename);
  }
  list_delete(names);
  list_delete(values);
  return 0;
}
