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


#include "rdpdr.h"
#include "disk_dev.h"


static int client_id;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
static Action actions[128];
static int action_index=1;

struct log_config	*l_config;
int  disk_sock;
int rdpdr_sock;
int disk_up = 0;
struct device device_list[128];
int device_count = 0;
char username[256];
char mount_point[256];

/*****************************************************************************/
int APP_CC
disk_send(struct stream* s){
  int rv;
  int length = (int)(s->end - s->data);

  rv = vchannel_send(rdpdr_sock, s->data, length);
  if (rv != 0)
  {
    log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_send]: "
    		"Enable to send message");
  }
  log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "rdpdr_disk[disk_send]: "
				"send message: ");
  log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)s->data, length );

  return rv;
}

/*****************************************************************************/
int
disk_get_device_index(int device_id)
{
	int i;
	for (i=0 ; i< device_count ; i++)
	{
		if(device_list[i].device_id == device_id)
		{
			return i;
		}
	}
	return -1;
}

/*****************************************************************************/
int
disk_begin_io_request(char* job,int device)
{
	struct stream* s;
	int index;
	make_stream(s);
	init_stream(s,1024);

	actions[action_index].device = device;
	actions[action_index].file_id = action_index;
	actions[action_index].last_req = IRP_MJ_CREATE;
	actions[action_index].message_id = 0;
	g_strcpy(actions[action_index].path, job);

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[dev_redir_begin_io_request]:"
  		"Process job[%s]",job);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[action_index].device);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, IRP_MJ_CREATE);   	/* major version */
	out_uint32_le(s, 0);								/* minor version */
	index = disk_get_device_index(device);
	switch (device_list[index].device_type) {
		case RDPDR_DTYP_PRINT:
			out_uint32_le(s, 0);								/* desired access(unused) */
			out_uint64_le(s, 0);								/* size (unused) */
			out_uint32_le(s, 0);								/* file attribute (unused) */
			out_uint32_le(s, 0);								/* shared access (unused) */
			out_uint32_le(s, 0);								/* disposition (unused) */
			out_uint32_le(s, 0);								/* create option (unused) */
			out_uint32_le(s, 0);								/* path length (unused) */
			break;
		default:
			log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[dev_redir_begin_io_request]:"
					"the device type %04x is not yet supported",device);
			free_stream(s);
			return 0;
			break;
	}
	s_mark_end(s);
	disk_send(s);
	free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
disk_process_write_io_request(int completion_id, int offset)
{
	struct stream* s;
	int fd;
	char buffer[1024];
	int size;

	make_stream(s);
	init_stream(s,1100);
	actions[completion_id].last_req = IRP_MJ_WRITE;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_write_io_request]:"
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
		disk_send(s);
		actions[completion_id].message_id++;
		free_stream(s);
		return 0;
	}
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_write_io_request]:"
  		"the file %s did not exists",actions[completion_id].path);
	free_stream(s);
	return 1;
}

/*****************************************************************************/
int APP_CC
disk_process_close_io_request(int completion_id)
{
	struct stream* s;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_close_io_request]:"
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
	disk_send(s);
	actions[completion_id].message_id++;
	free_stream(s);
	return 0;
	free_stream(s);
}


/*****************************************************************************/
int APP_CC
disk_process_iocompletion(struct stream* s)
{
	int device;
	int completion_id;
	int io_status;
	int result;
	int offset;
	int size;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
			"device reply");
	in_uint32_le(s, device);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
			"device : %i",device);
	in_uint32_le(s, completion_id);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
			"completion id : %i", completion_id);
	in_uint32_le(s, io_status);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
			"io_statio : %08x", io_status);
	if( io_status != STATUS_SUCCESS)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
  			"the action failed with the status : %08x",io_status);
  	result = -1;
	}

	switch(actions[completion_id].last_req)
	{
	case IRP_MJ_CREATE:
		in_uint32_le(s, actions[completion_id].file_id);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
				"file %s created",actions[completion_id].last_req);
		size = g_file_size(actions[completion_id].path);
		offset = 0;
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_next_io]: "
				"the file size to transfert: %i",size);
		result = disk_process_write_io_request(completion_id, offset);
		break;

	case IRP_MJ_WRITE:
		in_uint32_le(s, size);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
				"%i octect written for the jobs %s",size, actions[completion_id].path);
		offset = 1024* actions[completion_id].message_id;
		size = g_file_size(actions[completion_id].path);
		if(offset > size)
		{
			result = disk_process_close_io_request(completion_id);
			break;
		}
		result = disk_process_write_io_request(completion_id, offset);
		break;

	case IRP_MJ_CLOSE:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
				"file %s closed",actions[completion_id].path);
		//TODO
		//result = disk_dev_delete_job(actions[completion_id].path);
		break;
	default:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_iocompletion]: "
				"last request %08x is invalid",actions[completion_id].last_req);
		result=-1;
		break;

	}
	return result;
}

/*****************************************************************************/
int APP_CC
disk_device_list_reply(int handle)
{
  struct stream* s;
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_device_list_reply]:"
  		" reply to the device add");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_DEVICE_REPLY);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */
  out_uint32_le(s, client_id);							/* client ID */
  s_mark_end(s);

  g_sleep(10);
  disk_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
disk_devicelist_announce(struct stream* s)
{
  int device_list_count, device_id, device_type, device_data_length;
  int i;
  char dos_name[9] = {0};
  int handle;
  char* p;

  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
  		"	new message: PAKID_CORE_DEVICELIST_ANNOUNCE");
  in_uint32_le(s, device_list_count);	/* DeviceCount */
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
		  "%i device(s) declared", device_list_count);
  /* device list */
  for( i=0 ; i<device_list_count ; i++)
  {
    in_uint32_le(s, device_type);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
    		"device type: %i", device_type);
    in_uint32_le(s, device_id);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
  		  "device id: %i", device_id);

    in_uint8a(s,dos_name,8)
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
  		  "dos name: '%s'", dos_name);
    in_uint32_le(s, device_data_length);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
  		  "data length: %i", device_data_length);

    if (device_type !=  RDPDR_DTYP_FILESYSTEM)
    {
      log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
  					"The device is not a disk");
    	continue;
    }
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
					"Add disk device");
    p = s->p;
    handle = 0;
    disk_dev_add(s, device_data_length, device_id, dos_name);
    s->p = p + device_data_length;
    if (handle != 1)
    {
    	device_list[device_count].device_id = device_id;
    	device_list[device_count].device_type = RDPDR_DTYP_FILESYSTEM;
    	device_count++;
    	disk_device_list_reply(handle);
    	continue;
    }
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_devicelist_announce]: "
    		"Unable to add printer device");
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
disk_confirm_clientID_request()
{
  struct stream* s;
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_send_capability_request]: Send Server Client ID Confirm Request");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */

  s_mark_end(s);
  disk_send(s);
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int APP_CC
disk_process_message(struct stream* s, int length, int total_length)
{
  int component;
  int packetId;
  int result = 0;
  struct stream* packet;
  if(length != total_length)
  {
  	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_message]: "
  			"packet is fragmented");
  	if(is_fragmented_packet == 0)
  	{
  		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_message]: "
  				"packet is fragmented : first part");
  		is_fragmented_packet = 1;
  		fragment_size = length;
  		make_stream(splitted_packet);
  		init_stream(splitted_packet, total_length);
  		g_memcpy(splitted_packet->p, s->p, length );
  		log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)s->p, length);
  		return 0;
  	}
  	else
  	{
  		g_memcpy(splitted_packet->p+fragment_size, s->p, length );
  		fragment_size += length;
  		if (fragment_size == total_length )
  		{
    		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_message]: "
    				"packet is fragmented : last part");
  			packet = splitted_packet;
  		}
  		else
  		{
    		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_message]: "
    				"packet is fragmented : next part");
  			return 0;
  		}
  	}
  }
  else
  {
  	packet = s;
  }
  log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "rdpdr_disk[disk_process_message]: data received:");
  log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)packet->p, total_length);
  in_uint16_le(packet, component);
  in_uint16_le(packet, packetId);
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_process_message]: component=0x%04x packetId=0x%04x", component, packetId);
  if ( component == RDPDR_CTYP_CORE )
  {
    switch (packetId)
    {
    case PAKID_CORE_DEVICELIST_ANNOUNCE:
    	result = disk_devicelist_announce(packet);
      break;
    case PAKID_CORE_DEVICE_IOCOMPLETION:
    	result = disk_process_iocompletion(packet);
    	break;
    default:
      log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[disk_process_message]: "
      		"Unknown message %02x",packetId);
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
int
disk_init()
{
  char filename[256];
  char log_filename[256];
  struct list* names;
  struct list* values;
  char* name;
  char* value;
  int index;
  int display_num;
  int res;

  display_num = g_get_display_num_from_display(g_strdup(g_getenv("DISPLAY")));
	if(display_num == 0)
	{
		g_printf("rdpdr_disk[disk_init]: Display must be different of 0\n");
		return ERROR;
	}
	l_config = g_malloc(sizeof(struct log_config), 1);
	l_config->program_name = "rdpdr_disk";
	l_config->log_file = 0;
	l_config->fd = 0;
	l_config->log_level = LOG_LEVEL_DEBUG;
	l_config->enable_syslog = 0;
	l_config->syslog_level = LOG_LEVEL_DEBUG;

  names = list_create();
  names->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_snprintf(filename, 255, "%s/rdpdr.conf", XRDP_CFG_PATH);
  if (file_by_name_read_section(filename, RDPDR_CFG_GLOBAL, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, RDPDR_CFG_NAME))
      {
        if( g_strlen(value) > 1)
        {
        	l_config->program_name = (char*)g_strdup(value);
        }
      }
    }
  }
  if (file_by_name_read_section(filename, RDPDR_CFG_LOGGING, names, values) == 0)
  {
    for (index = 0; index < names->count; index++)
    {
      name = (char*)list_get_item(names, index);
      value = (char*)list_get_item(values, index);
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_DIR))
      {
      	l_config->log_file = (char*)g_strdup(value);
      }
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_LEVEL))
      {
      	l_config->log_level = log_text2level(value);
      }
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_ENABLE_SYSLOG))
      {
      	l_config->enable_syslog = log_text2bool(value);
      }
      if (0 == g_strcasecmp(name, RDPDR_CFG_LOG_SYSLOG_LEVEL))
      {
      	l_config->syslog_level = log_text2level(value);
      }
    }
  }
  if( g_strlen(l_config->log_file) > 1 && g_strlen(l_config->program_name) > 1)
  {
  	g_sprintf(log_filename, "%s/%i/%s.log",
  			l_config->log_file,	display_num, l_config->program_name);
  	l_config->log_file = (char*)g_strdup(log_filename);
  }
  list_delete(names);
  list_delete(values);
  res = log_start(l_config);
	if( res != LOG_STARTUP_OK)
	{
		g_printf("rdpdr_disk[rdpdr_init]: Unable to start log system [%i]\n", res);
		return res;
	}
  return LOG_STARTUP_OK;
}

/*****************************************************************************/
int disk_deinit()
{
	char status[1];
	int i;

	status[0] = STATUS_DISCONNECTED;
	vchannel_close(rdpdr_sock);
	vchannel_close(disk_sock);
	fuse_unmount(mount_point);
	g_exit(0);
}

/*****************************************************************************/
void *thread_disk_process (void * arg)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[thread_disk_process]:"
				"initialise main mount point");
	fuse_process();

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[thread_disk_process]: "
			"Finished spool process");
	pthread_exit (0);
}

/*****************************************************************************/
void *thread_vchannel_process (void * arg)
{
	struct stream* s = NULL;
	int rv;
	int length;
	int total_length;

	while(1){
		make_stream(s);
		init_stream(s, 1605);
		rv = vchannel_receive(disk_sock, s->data, &length, &total_length);

		switch(rv)
		{
		case ERROR:
			log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[thread_vchannel_process]: "
					"Invalid message");
			free_stream(s);
			pthread_exit ((void*) 1);
			break;
		case STATUS_CONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[thread_vchannel_process]: "
					"Status connected");
			break;
		case STATUS_DISCONNECTED:
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[thread_vchannel_process]: "
					"Status disconnected");
			disk_deinit();
			break;
		default:
			disk_process_message(s, length, total_length);
			break;
		}
		free_stream(s);
	}
	pthread_exit (0);
}




/*****************************************************************************/
int main(int argc, char** argv, char** environ)
{
	pthread_t disk_thread;
	pthread_t vchannel_thread;
	void *ret;

	l_config = g_malloc(sizeof(struct log_config), 1);
	if (argc != 2)
	{
		g_printf("Usage : rdpdr_disk USERNAME\n");
		return 1;
	}
	if ( g_getuser_info(argv[1], 0, 0, 0, 0, 0) == 1)
	{
		g_printf("The username '%s' did not exist\n", argv[1]);
	}
	g_strncpy(username, argv[1], sizeof(username));

	//decrease_right

	if (disk_init() != LOG_STARTUP_OK)
	{
		g_printf("rdpdr_disk[main]: Enable to init log system\n");
		g_free(l_config);
		return 1;
	}
	if (vchannel_init() == ERROR)
	{
		g_printf("rdpdr_disk[main]: Enable to init channel system\n");
		g_free(l_config);
		return 1;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[main]: "
				"Open channel to rdpdr main apps");
	disk_sock = vchannel_open("disk");
	if(disk_sock == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[main]: "
				"Error while connecting to vchannel provider");
		return 1;
	}
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[main]: "
				"Open channel to channel server");
	rdpdr_sock = vchannel_open("rdpdr");
	if(rdpdr_sock == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[main]: "
				"Error while connecting to rdpdr provider");
		return 1;
	}
	g_sprintf(mount_point, "/home/%s/rdp_drive", username);
	g_mkdir(mount_point);
	if( g_directory_exist(mount_point) == 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[main]: "
				"Unable to initialize the mount point");
	}
	/* init */
	if (pthread_create (&disk_thread, NULL, thread_disk_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[main]: "
				"Pthread_create error for thread : spool_thread");
		return 1;
	}
	if (pthread_create (&vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[main]: "
				"Pthread_create error for thread : vchannel_thread");
		return 1;
	}
	disk_up = 1;
	(void)pthread_join (vchannel_thread, &ret);
	(void)pthread_join (disk_thread, &ret);


	return fuse_process();
}
