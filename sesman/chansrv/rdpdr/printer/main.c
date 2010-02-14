/**
 * Copyright (C) 2008 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2009 2010
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

#include <vchannel.h>
#include <os_calls.h>
#include <xrdp_constants.h>
#include "rdpdr.h"


static pthread_mutex_t mutex;
Vchannel printer_chan;
Vchannel rdpdr_chan;
struct log_config	*l_config;
static char hostname[256];
static int use_unicode;
static int vers_major;
static int vers_minor;
static int client_id;
static int supported_operation[6] = {0};
struct device device_list[128];
int device_count = 0;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
static Action actions[128];
static int action_index=1;

/*****************************************************************************/
int APP_CC
rdpdr_send(struct stream* s){
  int rv;
  int length = (int)(s->end - s->data);

  rv = vchannel_send(&rdpdr_chan, s, length);
  if (rv != 0)
  {
    log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_printer[rdpdr_send]: "
    		"Enable to send message");
  }
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[rdpdr_send]: "
				"send message: ");
  log_hexdump(l_config, LOG_LEVEL_DEBUG, (unsigned char*)s->data, length );

  return rv;
}

/*****************************************************************************/
int APP_CC
rdpdr_in_unistr(struct stream* s, char *string, int str_size, int in_len)
{
#ifdef HAVE_ICONV
  size_t ibl = in_len, obl = str_size - 1;
  char *pin = (char *) s->p, *pout = string;
  static iconv_t iconv_h = (iconv_t) - 1;

  if (g_iconv_works)
  {
    if (iconv_h == (iconv_t) - 1)
    {
      if ((iconv_h = iconv_open(g_codepage, WINDOWS_CODEPAGE)) == (iconv_t) - 1)
      {
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[rdpdr_in_unistr]: iconv_open[%s -> %s] fail %p",
					WINDOWS_CODEPAGE, g_codepage, iconv_h);
        g_iconv_works = False;
        return rdp_in_unistr(s, string, str_size, in_len);
      }
    }

    if (iconv(iconv_h, (ICONV_CONST char **) &pin, &ibl, &pout, &obl) == (size_t) - 1)
    {
      if (errno == E2BIG)
      {
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[rdpdr_in_unistr]: "
							"server sent an unexpectedly long string, truncating");
      }
      else
      {
        iconv_close(iconv_h);
        iconv_h = (iconv_t) - 1;
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[rdpdr_in_unistr]: "
							"iconv fail, errno %d\n", errno);
        g_iconv_works = False;
        return rdpdr_in_unistr(s, string, str_size, in_len);
      }
    }

    /* we must update the location of the current STREAM for future reads of s->p */
    s->p += in_len;
    *pout = 0;
    return pout - string;
  }
  else
#endif
  {
    int i = 0;
    int len = in_len / 2;
    int rem = 0;

    if (len > str_size - 1)
    {
      log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[rdpdr_in_unistr]: "
						"server sent an unexpectedly long string, truncating");
      len = str_size - 1;
      rem = in_len - 2 * len;
    }
    while (i < len)
    {
      in_uint8a(s, &string[i++], 1);
      in_uint8s(s, 1);
    }
    in_uint8s(s, rem);
    string[len] = 0;
    return len;
  }
}

/*****************************************************************************/
int APP_CC
rdpdr_process_write_io_request(int completion_id, int offset)
{
	struct stream* s;
	int fd;
	char buffer[1024];
	int size;

	make_stream(s);
	init_stream(s,1100);
	actions[completion_id].last_req = IRP_MJ_WRITE;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[rdpdr_process_write_io_request]:"
  		"process next io request[%s]",actions[completion_id].path);
	out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_WRITE);   	/* major version */
	out_uint32_le(s, 0);								/* minor version */
	if(g_file_exist(actions[completion_id].path)){
		fd = g_file_open(actions[completion_id].path);
		g_file_seek(fd, offset);
		size = g_file_read(fd, buffer, 1024);
		out_uint32_le(s,size);
		out_uint64_le(s,offset);
		out_uint8s(s,20);
		out_uint8p(s,buffer,size);
		s_mark_end(s);
		rdpdr_send(s);
		actions[completion_id].message_id++;
		free_stream(s);
		return 0;
	}
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[rdpdr_process_write_io_request]:"
  		"the file %s did not exists",actions[completion_id].path);
	free_stream(s);
	return 1;
}

/*****************************************************************************/
int APP_CC
rdpdr_process_close_io_request(int completion_id)
{
	struct stream* s;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[rdpdr_process_close_io_request]:"
	  		"close file : %s",actions[completion_id].path);
	make_stream(s);
	init_stream(s,1100);
	actions[completion_id].last_req = IRP_MJ_CLOSE;
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_CLOSE);   	/* major version */
	out_uint32_le(s, 0);								/* minor version */
	out_uint8s(s,32);
	s_mark_end(s);
	rdpdr_send(s);
	actions[completion_id].message_id++;
	free_stream(s);
	return 0;
	free_stream(s);
}


/*****************************************************************************/
int APP_CC
rdpdr_process_iocompletion(struct stream* s)
{
	int device;
	int completion_id;
	int io_status;
	int result;
	int offset;
	int size;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
			"device reply");
	in_uint32_le(s, device);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
			"device : %i",device);
	in_uint32_le(s, completion_id);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
			"completion id : %i", completion_id);
	in_uint32_le(s, io_status);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
			"io_statio : %08x", io_status);
	if( io_status != STATUS_SUCCESS)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
  			"the action failed with the status : %08x",io_status);
  	result = -1;
	}

	switch(actions[completion_id].last_req)
	{
	case IRP_MJ_CREATE:
		in_uint32_le(s, actions[completion_id].file_id);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
				"file %s created",actions[completion_id].last_req);
		size = g_file_size(actions[completion_id].path);
		offset = 0;
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[rdpdr_next_io]: "
				"the file size to transfert: %i",size);
		result = rdpdr_process_write_io_request(completion_id, offset);
		break;

	case IRP_MJ_WRITE:
		in_uint32_le(s, size);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
				"%i octect written for the jobs %s",size, actions[completion_id].path);
		offset = 1024* actions[completion_id].message_id;
		size = g_file_size(actions[completion_id].path);
		if(offset > size)
		{
			result = rdpdr_process_close_io_request(completion_id);
			break;
		}
		result = rdpdr_process_write_io_request(completion_id, offset);
		break;

	case IRP_MJ_CLOSE:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
				"file %s closed",actions[completion_id].path);
		//TODO
		//result = printer_dev_delete_job(actions[completion_id].path);
		break;
	default:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_iocompletion]: "
				"last request %08x is invalid",actions[completion_id].last_req);
		result=-1;
		break;

	}
	return result;
}

/*****************************************************************************/
int APP_CC
rdpdr_devicelist_announce(struct stream* s)
{
  int device_list_count, device_id, device_type, device_data_length;
  int i;
  char dos_name[9] = {0};
  int handle;

  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
  		"	new message: PAKID_CORE_DEVICELIST_ANNOUNCE");
  in_uint32_le(s, device_list_count);	/* DeviceCount */
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
		  "%i device(s) declared", device_list_count);
  /* device list */
  for( i=0 ; i<device_list_count ; i++)
  {
    in_uint32_le(s, device_type);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
    		"device type: %i", device_type);
    in_uint32_le(s, device_id);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
  		  "device id: %i", device_id);

    in_uint8a(s,dos_name,8)
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
  		  "dos name: '%s'", dos_name);
    in_uint32_le(s, device_data_length);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
  		  "data length: %i", device_data_length);

    switch(device_type)
    {
    case RDPDR_DTYP_PRINT :
      log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
      		  "Add printer device");
      //TODO creation socket
      //handle = printer_dev_add(s, device_data_length, device_id, dos_name);
      if (handle != 1)
      {
      	device_list[device_count].device_id = device_id;
      	device_list[device_count].device_type = RDPDR_DTYP_PRINT;
      	device_count++;
      	rdpdr_device_list_reply(handle);
      	break;
      }
      log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
      		  "Enable to add printer device");
      break;

    case RDPDR_DTYP_FILESYSTEM :
      log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_devicelist_announce]: "
        	  "Add filesystem device");
      break;
    }
  }
  return 0;
}


/*****************************************************************************/
int APP_CC
rdpdr_device_list_reply(int handle)
{
  struct stream* s;
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_device_list_reply]:"
  		" reply to the device add");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_DEVICE_REPLY);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */
  out_uint32_le(s, client_id);							/* client ID */
  s_mark_end(s);
  rdpdr_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_confirm_clientID_request()
{
  struct stream* s;
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_send_capability_request]: Send Server Client ID Confirm Request");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */

  s_mark_end(s);
  rdpdr_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpdr_process_message(struct stream* s, int length, int total_length)
{
  int component;
  int packetId;
  int result;
  struct stream* packet;

  if(length != total_length)
  {
  	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_message]: "
  			"packet is fragmented");
  	if(is_fragmented_packet == 0)
  	{
  		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_message]: "
  				"packet is fragmented : first part");
  		is_fragmented_packet = 1;
  		fragment_size = length;
  		make_stream(splitted_packet);
  		init_stream(splitted_packet, total_length);
  		g_memcpy(splitted_packet->p,s->p, length );
  		log_hexdump(l_config, LOG_LEVEL_DEBUG, (unsigned char*)s->p, length);
  		return 0;
  	}
  	else
  	{
  		g_memcpy(splitted_packet->p+fragment_size, s->p, length );
  		fragment_size += length;
  		if (fragment_size == total_length )
  		{
    		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_message]: "
    				"packet is fragmented : last part");
  			packet = splitted_packet;
  		}
  		else
  		{
    		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_message]: "
    				"packet is fragmented : next part");
  			return 0;
  		}
  	}
  }
  else
  {
  	packet = s;
  }
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_message]: data received:");
  log_hexdump(l_config, LOG_LEVEL_DEBUG, (unsigned char*)packet->p, total_length);
  in_uint16_le(packet, component);
  in_uint16_le(packet, packetId);
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer_channel[rdpdr_process_message]: component=0x%04x packetId=0x%04x", component, packetId);
  if ( component == RDPDR_CTYP_CORE )
  {
    switch (packetId)
    {
    case PAKID_CORE_DEVICELIST_ANNOUNCE:
    	result = rdpdr_devicelist_announce(packet);
      break;
    case PAKID_CORE_DEVICE_IOCOMPLETION:
    	result = rdpdr_process_iocompletion(packet);
    	break;
    default:
      log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer_channel[rdpdr_process_message]: "
      		"unknown message %02x",packetId);
      result = 1;
    }
    if(is_fragmented_packet == 1)
    {
    	is_fragmented_packet = 0;
    	fragment_size = 0;
    	free_stream(packet);
    }

  }
  return result;
}

/*****************************************************************************/
void *thread_spool_process (void * arg)
{
	while (1) {
		pthread_mutex_lock(&mutex);
		pthread_mutex_unlock(&mutex);
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[thread_spool_process]: "
			"Finished spool process");
	pthread_exit (0);
}

/*****************************************************************************/
void *thread_vchannel_process (void * arg)
{
	char* buffer = malloc(1024);
	struct stream* s = NULL;
	int rv;
	int length;
	int total_length;
	while(1){
		make_stream(s);
		rv = vchannel_receive(&printer_chan, s, &length, &total_length);
		if( rv == ERROR )
		{
			free_stream(s);
			continue;
		}
		switch(rv)
		{
		case ERROR:
			log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_printer[thread_vchannel_process]: "
					"Invalid message");
			break;
		case STATUS_CONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[thread_vchannel_process]: "
					"Status connected");
			break;
		case STATUS_DISCONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[thread_vchannel_process]: "
					"Status disconnected");
			break;
		default:
			rdpdr_process_message(s, length, total_length);
			break;
		}
		free_stream(s);
	}
	pthread_exit (0);
}

/*****************************************************************************/
int main(int argc, char** argv, char** environ)
{
	pthread_t spool_thread;
	pthread_t vchannel_thread;
	void *ret;
	l_config = g_malloc(sizeof(struct log_config), 1);
	vchannel_read_logging_conf(l_config, "rdpdr_printer");
	if (log_start(l_config) != LOG_STARTUP_OK)
	{
		g_printf("Enable to init log system\n");
		return 1;
	}
	printer_chan.log_conf = l_config;
	rdpdr_chan.log_conf = l_config;
	pthread_mutex_init(&mutex, NULL);
	g_strncpy(printer_chan.name, "printer", 9);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[main]: "
				"Open channel to rdpdr main apps");
	if(vchannel_open(&printer_chan) == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_printer[main]: "
				"Error while connecting to vchannel provider");
		return 1;
	}
	g_strncpy(rdpdr_chan.name, "rdpdr", 9);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[main]: "
				"Open channel to channel server");
	if(vchannel_open(&rdpdr_chan) == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_printer[main]: "
				"Error while connecting to rdpdr provider");
		return 1;
	}
	/* init */
	if (pthread_create (&spool_thread, NULL, thread_spool_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_printer[main]: "
				"Pthread_create error for thread : spool_thread");
		return 1;
	}
	if (pthread_create (&vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_printer[main]: "
				"Pthread_create error for thread : vchannel_thread");
		return 1;
	}

	//(void)pthread_join (spool_thread, &ret);
	(void)pthread_join (vchannel_thread, &ret);
	pthread_mutex_destroy(&mutex);

	return 0;
}
