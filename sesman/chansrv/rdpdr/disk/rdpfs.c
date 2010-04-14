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
#include <xrdp_constants.h>



extern struct log_config *l_config;
int  disk_sock;
int rdpdr_sock;
static int client_id;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
static Action actions[128];
static struct request_response rdpfs_response[128];
static int action_index=0;
struct device device_list[128];
int device_count = 0;
pthread_cond_t reply_cond;
pthread_mutex_t mutex;
extern int disk_up;



/* Convert seconds since 1970 to a filetime */
static void
seconds_since_1970_to_filetime(time_t seconds, uint32 * high, uint32 * low)
{
	unsigned long long ticks;

	ticks = (seconds + 11644473600LL) * 10000000;
	*low = (uint32) ticks;
	*high = (uint32) (ticks >> 32);
}

/* Convert seconds since 1970 back to filetime */
static time_t
convert_1970_to_filetime(uint32 high, uint32 low)
{
	unsigned long long ticks;
	time_t val;

	ticks = low + (((unsigned long long) high) << 32);
	ticks /= 10000000;
	ticks -= 11644473600LL;

	val = (time_t) ticks;
	return (val);

}


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
rdp_in_unistr(struct stream* s, char *string, int str_size, int in_len)
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
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[printer_dev_in_unistr]: "
        		"Iconv_open[%s -> %s] fail %p",
					WINDOWS_CODEPAGE, g_codepage, iconv_h);
        g_iconv_works = False;
        return rdp_in_unistr(s, string, str_size, in_len);
      }
    }

    if (iconv(iconv_h, (ICONV_CONST char **) &pin, &ibl, &pout, &obl) == (size_t) - 1)
    {
      if (errno == E2BIG)
      {
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[printer_dev_in_unistr]: "
							"Server sent an unexpectedly long string, truncating");
      }
      else
      {
        iconv_close(iconv_h);
        iconv_h = (iconv_t) - 1;
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[printer_dev_in_unistr]: "
							"Iconv fail, errno %d\n", errno);
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
      log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_printer[printer_dev_in_unistr]: "
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


void APP_CC
rdpfs_wait_reply()
{
  if (pthread_cond_wait(&reply_cond, &mutex) != 0) {
    perror("pthread_cond_timedwait() error");
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_wait_reply]: "
				"pthread_mutex_lock()");
    return;
  }
}


/*****************************************************************************/
int APP_CC
rdpfs_open()
{
	int ret;
	pthread_cond_init(&reply_cond, NULL);
	pthread_mutex_init(&mutex, NULL);
	log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_open]: "
			"after cond init");

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
	int completion_id = action_index;
	action_index++;

	for (i=0 ; i<device_count ; i++)
	{
		device_id = device_list[i].device_id;
		rdpfs_create(device_id, GENERIC_READ|FILE_EXECUTE_ATTRIBUTES,	FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,	FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, "");
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_open]: "
				"wait creation reply");
		rdpfs_wait_reply();
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_open]: "
				"after wait creation reply");

		//volume information
		rdpfs_query_volume_information(completion_id, device_id, FileFsVolumeInformation, "");
		rdpfs_wait_reply();
		//rdpfs_query_information(device_id, FileBasicInformation,"");
		//rdpfs_query_information(device_id, FileStandardInformation,"");
		//rdpfs_request_close(device_id);
		//close
//		if (pthread_mutex_unlock(&mutex) != 0) {
//		    perror("pthread_mutex_lock() error");
//				log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[rdpfs_open]: "
//						"pthread_mutex_lock()");
//		    return;
//		}

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
int APP_CC
rdpfs_create(int device_id, int desired_access, int shared_access,
		int creation_disposition, int flags, const char* path)
{
	struct stream* s;
	int index;
	int completion_id;
	make_stream(s);
	init_stream(s,1024);

	completion_id = action_index;
	action_index++;
	actions[completion_id].device = device_id;
	actions[completion_id].file_id = completion_id;
	actions[completion_id].last_req = IRP_MJ_CREATE;
	g_strcpy(actions[completion_id].path, path);

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_readdir]:"
  		"Process job[%s]",path);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, device_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, completion_id);												/* completion id */
	out_uint32_le(s, IRP_MJ_CREATE);   											/* major version */
	out_uint32_le(s, 0);																		/* minor version */

	out_uint32_be(s, desired_access);												/* FsInformationClass */
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
  return completion_id;
}


/*****************************************************************************/
void APP_CC
rdpfs_request_close(int completion_id, int device_id)
{
	struct stream* s;
	int index;
	make_stream(s);
	init_stream(s,1024);

	actions[action_index].device = device_id;
	actions[action_index].file_id = action_index;
	actions[action_index].last_req = IRP_MJ_CLOSE;
	actions[action_index].message_id = 0;


	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_request_close]:"
			"close device id %i\n",device_id);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[action_index].device);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, actions[action_index].file_id);
	out_uint32_le(s, IRP_MJ_CLOSE);   											/* major version */
	out_uint32_le(s, 0);																		/* minor version */

	out_uint8s(s, 32);																				/* padding */
	s_mark_end(s);
	rdpfs_send(s);
	free_stream(s);
	//remove action
  return ;
}

/*****************************************************************************/
void APP_CC
rdpfs_query_volume_information(int completion_id, int device_id, int information, const char* query )
{
	struct stream* s;
	int index;
	make_stream(s);
	init_stream(s,1024);

	g_strcpy(actions[completion_id].path, query);
	actions[completion_id].last_req = IRP_MJ_QUERY_VOLUME_INFORMATION;
	actions[completion_id].request_param = information;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_query_volume_information]:"
  		"Process job[%s]",query);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_QUERY_VOLUME_INFORMATION); 			/* major version */
	out_uint32_le(s, 0);																		/* minor version */

	out_uint32_le(s, information);													/* FsInformationClass */
	out_uint32_le(s, g_strlen(query));											/* length */
	out_uint8s(s, 24);																			/* padding */
	out_uint8p(s, query, g_strlen(query));									/* query */

	s_mark_end(s);
	rdpfs_send(s);
	free_stream(s);
  return ;
}

/*****************************************************************************/
void APP_CC
rdpfs_query_information(int completion_id, int device_id, int information, const char* query )
{
	struct stream* s;
	int index;
	make_stream(s);
	init_stream(s,1024);

	g_strcpy(actions[completion_id].path, query);
	actions[completion_id].last_req = IRP_MJ_QUERY_VOLUME_INFORMATION;
	actions[completion_id].request_param = information;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_query_information]:"
  		"Process job[%s]",query);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_QUERY_INFORMATION);             /* major version */
	out_uint32_le(s, 0);																		/* minor version */

	out_uint32_le(s, information);													/* FsInformationClass */
	out_uint32_le(s, g_strlen(query));											/* length */
	out_uint8s(s, 24);																			/* padding */
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
rdpfs_process_create_response(int completion_id, struct stream* s)
{
	in_uint32_le(s, actions[completion_id].file_id);			/* client file id */
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"IRP_MJ_CREATE response: update file id with %i", actions[completion_id].file_id);

}


/*****************************************************************************/
int APP_CC
rdpfs_process_volume_information_response(int completion_id, struct stream* s)
{
	int length;
	struct request_response* rep;
	int label_length;
	int object_fs;
	int low;
	int high;
	int ignored;
	int fs_name_len;


	rep = &rdpfs_response[completion_id];

	in_uint32_le(s, length);			/* response length */
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"IRP_MJ_QUERY_VOLUME_INFORMATION response : response length : %i", length);
	rep->Request_type = RDPDR_IRP_MJ_QUERY_VOLUME_INFORMATION;

	switch (actions[completion_id].request_param)
	{
	case FileFsVolumeInformation:

		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
				"IRP_MJ_QUERY_VOLUME_INFORMATION response : extract FileFsVolumeInformation information");
		rep->Request_param = actions[completion_id].request_param;

		in_uint32_le(s, rep->volume_inf.creation_time_low);     /* volume creation time low */
		in_uint32_le(s, rep->volume_inf.creation_time_high);    /* volume creation time high */
		in_uint32_le(s, rep->volume_inf.serial);            	  /* serial */

		in_uint32_le(s, label_length);	                        /* length of string */

		in_uint8(s, object_fs);	                                /* support objects? */
		if (object_fs != 0)
		{
			log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[rdpfs_process_iocompletion]: "
					"IRP_MJ_QUERY_VOLUME_INFORMATION response : Xrdp did not support object file system");
		}
		rdp_in_unistr(s, rep->volume_inf.label, sizeof(rep->volume_inf.label), label_length);
		break;

	case FileFsSizeInformation:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
						"IRP_MJ_QUERY_VOLUME_INFORMATION response : extract FileFsSizeInformation information");

		in_uint32_le(s, low);                          /* Total allocation units low */
		in_uint32_le(s, high);                         /* Total allocation high units */
		in_uint32_le(s, rep->volume_inf.f_bfree);	     /* Available allocation units */
		in_uint32_le(s, rep->volume_inf.f_blocks);     /* Available allowcation units */
		in_uint32_le(s, ignored);                      /* Sectors per allocation unit */
		in_uint32_le(s, ignored);                      /* Bytes per sector */
		break;

	case FileFsAttributeInformation:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
						"IRP_MJ_QUERY_VOLUME_INFORMATION response : extract FileFsAttributeInformation information");

		in_uint32_le(s, ignored);                      /* fs attributes */
		in_uint32_le(s, rep->volume_inf.f_namelen);	   /* max length of filename */

		in_uint32_le(s, fs_name_len);                  /* length of fs_type */
		rdp_in_unistr(s, rep->volume_inf.fs_type, sizeof(rep->volume_inf.fs_type), fs_name_len);
		break;

	case FileFsDeviceInformation:
	case FileFsFullSizeInformation:

	default:

		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_volume_information_response]: "
						"IRP_MJ_QUERY_VOLUME_INFORMATION response : unknow response");
		return 1;
	}

}

/*****************************************************************************/
int APP_CC
rdpfs_process_information_response(int completion_id, struct stream* s)
{
	int length;
	struct request_response* rep;
	int label_length;
	int object_fs;
	int low;
	int high;
	int ignored;
	int fs_name_len;


	rep = &rdpfs_response[completion_id];

	in_uint32_le(s, length);			/* response length */
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"IRP_MJ_QUERY_INFORMATION response : response length : %i", length);
	rep->Request_type = RDPDR_IRP_MJ_QUERY_INFORMATION;

	switch (actions[completion_id].request_param)
	{
	case FileBasicInformation:
		printf("\tFileBasicInformation\n");

		in_uint32_le(s, low);	/* create_access_time */
		in_uint32_le(s, high);
		rep->fs_inf.create_access_time = convert_1970_to_filetime(high, low);

		in_uint32_le(s, low);	/* last_access_time */
		in_uint32_le(s, high);
		rep->fs_inf.last_access_time = convert_1970_to_filetime(high, low);

		in_uint32_le(s, low);	/* last_write_time */
		in_uint32_le(s, high);
		rep->fs_inf.last_write_time = convert_1970_to_filetime(high, low);

		in_uint32_le(s, low);	/* last_change_time */
		in_uint32_le(s, high);
		rep->fs_inf.last_change_time = convert_1970_to_filetime(high, low);

		out_uint32_le(s, rep->fs_inf.file_attributes);
		break;

//	case FileStandardInformation:
//		printf("\tFileStandardInformation\n");
//		out_uint32_le(out, filestat.st_size);	/* Allocation size */
//		out_uint32_le(out, 0);
//		out_uint32_le(out, filestat.st_size);	/* End of file */
//		out_uint32_le(out, 0);
//		out_uint32_le(out, filestat.st_nlink);	/* Number of links */
//		out_uint8(out, 0);	/* Delete pending */
//		out_uint8(out, S_ISDIR(filestat.st_mode) ? 1 : 0);	/* Directory */
//		break;
//
//	case FileObjectIdInformation:
//		printf("\tFileObjectIdInformation\n");
//		out_uint32_le(out, file_attributes);	/* File Attributes */
//		out_uint32_le(out, 0);	/* Reparse Tag */
//		break;
//
	default:

		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_information_response]: "
						"IRP_MJ_QUERY_INFORMATION response : unknow response");
		return 1;
	}
}


/*****************************************************************************/
int APP_CC
rdpfs_process_iocompletion(struct stream* s)
{
	int device;
	int completion_id;
	int io_status;
	int result;
	int offset;
	int size;


	result = 0;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"device reply");
	in_uint32_le(s, device);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"device : %i",device);
	in_uint32_le(s, completion_id);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"completion id : %i", completion_id);
	in_uint32_le(s, io_status);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"io_statio : %08x", io_status);
	if( io_status != STATUS_SUCCESS)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
  			"Action  %04x failed with the status : %08x", actions[completion_id].last_req, io_status);
  	result = -1;
	}

	switch(actions[completion_id].last_req)
	{
	case IRP_MJ_CREATE:
		result = rdpfs_process_create_response(completion_id, s);
		break;

	case IRP_MJ_QUERY_VOLUME_INFORMATION:
		result = rdpfs_process_volume_information_response(completion_id, s);
		break;

	case IRP_MJ_QUERY_INFORMATION:
		result = rdpfs_process_information_response(completion_id, s);
		break;

	case IRP_MJ_WRITE:
		in_uint32_le(s, size);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[printer_process_iocompletion]: "
				"%i octect written for the jobs %s",size, actions[completion_id].path);
		offset = 1024* actions[completion_id].message_id;
		size = g_file_size(actions[completion_id].path);
		if(offset > size)
		{
			result = 0;//printer_process_close_io_request(completion_id);
			break;
		}
		result = 0;//printer_process_write_io_request(completion_id, offset);
		break;

	case IRP_MJ_CLOSE:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[printer_process_iocompletion]: "
				"file %s closed",actions[completion_id].path);
		//TODO
		//result = printer_dev_delete_job(actions[completion_id].path);
		break;
	default:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_printer[printer_process_iocompletion]: "
				"last request %08x is invalid",actions[completion_id].last_req);
		return -1;
	}
	pthread_cond_signal(&reply_cond);
	return result;
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
    	disk_up = 1;
      break;
    case PAKID_CORE_DEVICE_IOCOMPLETION:
    	result = rdpfs_process_iocompletion(packet);
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

