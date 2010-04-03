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


#include "rdpfs.h"

extern struct log_config *l_config;
int  disk_sock;
int rdpdr_sock;
static int client_id;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
static Action actions[128];
static int action_index=1;
struct device device_list[128];
int device_count = 0;



/* Output a string in Unicode */
void
rdp_out_unistr(struct stream* s, char *string, int len)
{
#ifdef HAVE_ICONV
	size_t ibl = strlen(string), obl = len + 2;
	static iconv_t iconv_h = (iconv_t) - 1;
	char *pin = string, *pout = (char *) s->p;

	memset(pout, 0, len + 4);

	if (g_iconv_works)
	{
		if (iconv_h == (iconv_t) - 1)
		{
			size_t i = 1, o = 4;
			if ((iconv_h = iconv_open(WINDOWS_CODEPAGE, g_codepage)) == (iconv_t) - 1)
			{
				warning("rdp_out_unistr: iconv_open[%s -> %s] fail %p\n",
					g_codepage, WINDOWS_CODEPAGE, iconv_h);

				g_iconv_works = False;
				rdp_out_unistr(s, string, len);
				return;
			}
			if (iconv(iconv_h, (ICONV_CONST char **) &pin, &i, &pout, &o) ==
			    (size_t) - 1)
			{
				iconv_close(iconv_h);
				iconv_h = (iconv_t) - 1;
				warning("rdp_out_unistr: iconv(1) fail, errno %d\n", errno);

				g_iconv_works = False;
				rdp_out_unistr(s, string, len);
				return;
			}
			pin = string;
			pout = (char *) s->p;
		}

		if (iconv(iconv_h, (ICONV_CONST char **) &pin, &ibl, &pout, &obl) == (size_t) - 1)
		{
			iconv_close(iconv_h);
			iconv_h = (iconv_t) - 1;
			warning("rdp_out_unistr: iconv(2) fail, errno %d\n", errno);

			g_iconv_works = False;
			rdp_out_unistr(s, string, len);
			return;
		}

		s->p += len + 2;

	}
	else
#endif
	{
		int i = 0, j = 0;

		len += 2;

		while (i < len)
		{
			s->p[i++] = string[j++];
			s->p[i++] = 0;
		}

		s->p += len;
	}
}



/*****************************************************************************/
int APP_CC
rdpfs_send(struct stream* s){
  int rv;
  int length = (int)(s->end - s->data);

  rv = vchannel_send(rdpdr_sock, s->data, length);
  if (rv != 0)
  {
    log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_send]: "
    		"Enable to send message");
  }
  log_message(l_config, LOG_LEVEL_DEBUG_PLUS, "rdpdr_disk[rdpfs_send]: "
				"send message: ");
  log_hexdump(l_config, LOG_LEVEL_DEBUG_PLUS, (unsigned char*)s->data, length );

  return rv;
}

/*****************************************************************************/
int APP_CC
rdpfs_receive(const char* data, int* length, int* total_length)
{
	return vchannel_receive(disk_sock, data, length, total_length);

}



/*****************************************************************************/
int APP_CC
rdpfs_open()
{
	disk_sock = vchannel_open("disk");
	if(disk_sock == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_open]: "
				"Error while connecting to vchannel provider");
		return 1;
	}
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_open]: "
				"Open channel to channel server");
	rdpdr_sock = vchannel_open("rdpdr");
	if(rdpdr_sock == ERROR)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_open]: "
				"Error while connecting to rdpdr provider");
		return 1;
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
rdpfs_close()
{
	vchannel_close(rdpdr_sock);
	vchannel_close(disk_sock);
}

/*****************************************************************************/
void rdpfs_drive_init()
{
	int i;
	int device_id;
	for (i=0 ; i<device_count ; i++)
	{
		device_id = device_list[i].device_id;
		rdpfs_create(device_id, GENERIC_READ|FILE_EXECUTE_ATTRIBUTES,	FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,	FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, "");
	}

}


/*****************************************************************************/
void rdpfs_readdir(int device_id, const char* path)
{
	struct stream* s;
	int index;
	int path_len;
	make_stream(s);
	init_stream(s,1024);

	actions[action_index].device = device_id;
	actions[action_index].file_id = action_index;
	actions[action_index].last_req = IRP_MJ_DIRECTORY_CONTROL;
	actions[action_index].message_id = 0;
	g_strcpy(actions[action_index].path, path);

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_readdir]:"
  		"Process job[%s]",path);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[action_index].device);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, IRP_MJ_DIRECTORY_CONTROL);   	/* major version */
	out_uint32_le(s, IRP_MN_QUERY_DIRECTORY);				/* minor version */

	out_uint32_le(s, FileBothDirectoryInformation);	/* FsInformationClass */
	out_uint8(s, 0);																/* InitialQuery */
	path_len = (g_strlen(path)+1)*2;
	out_uint32_le(s, path_len);											/* PathLength */
	out_uint8s(s, 23);															/* Padding */
	rdp_out_unistr(s, (char*)path, path_len);				/* Path */

	s_mark_end(s);
	rdpfs_send(s);
	free_stream(s);
  return ;
}


/*****************************************************************************/
void APP_CC
rdpfs_create(int device_id, int desired_access, int shared_access,
		int creation_disposition, int flags, const char* path)
{
	struct stream* s;
	int index;
	make_stream(s);
	init_stream(s,1024);

	actions[action_index].device = device_id;
	actions[action_index].file_id = action_index;
	actions[action_index].last_req = IRP_MJ_CREATE;
	actions[action_index].message_id = 0;
	g_strcpy(actions[action_index].path, path);

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_readdir]:"
  		"Process job[%s]",path);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[action_index].device);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, IRP_MJ_CREATE);   											/* major version */
	out_uint32_le(s, 0);																		/* minor version */

	out_uint32_le(s, desired_access);												/* FsInformationClass */
	out_uint8s(s, 8);																				/* allocationSizee */
	out_uint32_le(s, 0x80);																	/* FileAttributes */
	out_uint32_le(s, shared_access);												/* SharedMode */
	out_uint32_le(s, creation_disposition);									/* Disposition */
	out_uint32_le(s, flags);																/* CreateOptions */
	out_uint32_le(s, (strlen(path)+1)*2);										/* PathLength */
	rdp_out_unistr(s, (char*)path, (strlen(path)+1)*2);			/* Path */

	s_mark_end(s);
	rdpfs_send(s);
	free_stream(s);
  return ;
}

/*****************************************************************************/
void rdpfs_query_information(int device_id, int information, const char* query )
{
	struct stream* s;
	int index;
	make_stream(s);
	init_stream(s,1024);

	actions[action_index].device = device_id;
	actions[action_index].file_id = action_index;
	actions[action_index].last_req = IRP_MJ_QUERY_VOLUME_INFORMATION;
	actions[action_index].message_id = 0;
	g_strcpy(actions[action_index].path, query);

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_readdir]:"
  		"Process job[%s]",query);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[action_index].device);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, IRP_MJ_QUERY_VOLUME_INFORMATION); 			/* major version */
	out_uint32_le(s, 0);																		/* minor version */

	out_uint32_le(s, information);													/* FsInformationClass */
	out_uint32_le(s, g_strlen(query));											/* length */
	out_uint8s(s, 24);																			/* allocationSizee */
	out_uint8p(s, query, g_strlen(query));									/* query */

	s_mark_end(s);
	rdpfs_send(s);
	free_stream(s);
  return ;
}

/*****************************************************************************/
int APP_CC
rdpfs_list_reply(int handle)
{
  struct stream* s;
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_reply]:"
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
  rdpfs_send(s);
  free_stream(s);
  return 0;
}




/*****************************************************************************/
int APP_CC
rdpfs_list_announce(struct stream* s)
{
  int device_list_count, device_id, device_type, device_data_length;
  int i;
  char dos_name[9] = {0};
  int handle;
  char* p;

  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
  		"	new message: PAKID_CORE_DEVICELIST_ANNOUNCE");
  in_uint32_le(s, device_list_count);	/* DeviceCount */
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
		  "%i device(s) declared", device_list_count);
  /* device list */
  for( i=0 ; i<device_list_count ; i++)
  {
    in_uint32_le(s, device_type);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
    		"device type: %i", device_type);
    in_uint32_le(s, device_id);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
  		  "device id: %i", device_id);

    in_uint8a(s,dos_name,8)
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
  		  "dos name: '%s'", dos_name);
    in_uint32_le(s, device_data_length);
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
  		  "data length: %i", device_data_length);

    if (device_type !=  RDPDR_DTYP_FILESYSTEM)
    {
      log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
  					"The device is not a disk");
    	continue;
    }
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
					"Add disk device");
    p = s->p;
    handle = 0;
    disk_dev_add(s, device_data_length, device_id, dos_name);
    s->p = p + device_data_length;
    if (handle != 1)
    {
    	device_list[device_count].device_id = device_id;
    	device_list[device_count].device_type = RDPDR_DTYP_FILESYSTEM;
    	device_list[device_count].ready = 0;
    	device_count++;
    	rdpfs_list_reply(handle);
    	continue;
    }
    log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_list_announce]: "
    		"Unable to add printer device");
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
rdpfs_confirm_clientID_request()
{
  struct stream* s;
  log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_send_capability_request]: Send Server Client ID Confirm Request");
  make_stream(s);
  init_stream(s, 256);
  out_uint16_le(s, RDPDR_CTYP_CORE);
  out_uint16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
  out_uint16_le(s, 0x1);  							/* major version */
  out_uint16_le(s, RDP5);							/* minor version */

  s_mark_end(s);
  rdpfs_send(s);
  free_stream(s);
  return 0;
}



/*****************************************************************************/
int APP_CC
rdpfs_process_message(struct stream* s, int length, int total_length)
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
    	result = rdpfs_list_announce(packet);
    	rdpfs_drive_init();
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

