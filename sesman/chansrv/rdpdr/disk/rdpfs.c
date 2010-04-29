/**
 * Copyright (C) 2008 Ulteo SAS
 * http://www.ulteo.com
 * Author David LECHEVALIER <david@ulteo.com> 2010
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
#include <fuse.h>



extern struct log_config *l_config;
int  disk_sock;
int rdpdr_sock;
static int client_id;
static int is_fragmented_packet = 0;
static int fragment_size;
static struct stream* splitted_packet;
static Action actions[128];
struct request_response rdpfs_response[128];
static int action_index=0;
static struct disk_device disk_devices[128];
static int disk_devices_count = 0;
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
convert_1970_to_filetime(uint64_t ticks)
{
	time_t val;

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
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[printer_dev_in_unistr]: "
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
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[printer_dev_in_unistr]: "
							"Server sent an unexpectedly long string, truncating");
      }
      else
      {
        iconv_close(iconv_h);
        iconv_h = (iconv_t) - 1;
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[printer_dev_in_unistr]: "
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
      log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[printer_dev_in_unistr]: "
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

/*****************************************************************************/
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
rdpfs_get_device_count()
{
	return disk_devices_count;
}

/*****************************************************************************/
struct disk_device* APP_CC
rdpfs_get_device_by_index(int device_index)
{
	return &disk_devices[device_index];
}


/************************************************************************/
struct disk_device* APP_CC
rdpfs_get_dir(int device_id)
{
	int i;
	for (i=0 ; i< disk_devices_count ; i++)
	{
		if(device_id == disk_devices[i].device_id)
		{
			return &disk_devices[i];
		}
	}
	return 0;
}

/************************************************************************/
struct disk_device* APP_CC
rdpfs_get_device_from_path(const char* path)
{
	//extract device name
	char *pos;
	int count;
	int i;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_get_device_from_path]: "
				"The path is: %s", path);

	path++;
	pos = strchr(path, '/');
	if(pos == NULL)
	{
		count = strlen(path);
	}
	else
	{
		count = pos-path;
	}

	for (i=0 ; i< disk_devices_count ; i++)
	{
		if(g_strncmp(path, disk_devices[i].dir_name, count) == 0)
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_get_device_from_path]: "
						"The drive is: %s", disk_devices[i].dir_name);
			return &disk_devices[i];
		}
	}
	return 0;
}

/************************************************************************/
int APP_CC
rdpfs_convert_fs_to_stat(struct fs_info* fs, struct stat* st)
{
	st->st_mode = S_IFREG | 0744;
	if( fs->file_attributes & FILE_ATTRIBUTE_DIRECTORY )
	{
		st->st_mode = S_IFDIR | 0744;
	}

	if( fs->file_attributes & FILE_ATTRIBUTE_READONLY )
	{
		st->st_mode |= 0444;
	}

	st->st_size = fs->file_size;
	st->st_atim.tv_sec = fs->last_access_time;
	st->st_ctim.tv_sec = fs->create_access_time;
	st->st_mtim.tv_sec = fs->last_change_time;
	st->st_blocks = fs->allocation_size;
	st->st_uid = g_getuid();
	st->st_gid = g_getuid();

	st->st_nlink = 2;
	return 0;
}

/************************************************************************/
int APP_CC
rdpfs_get_owner_permission(mode_t mode)
{
	int owner_perm = 0;

	if( mode & 0100)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_get_owner_permission]: "
					"Owner can execute");
		owner_perm |= GENERIC_EXECUTE;
	}
	if( mode & 0200)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_get_owner_permission]: "
					"Owner can write");
		owner_perm |= GENERIC_WRITE;
	}
	if( mode & 0400)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_get_owner_permission]: "
					"Owner can read ");
		owner_perm |= GENERIC_READ;
	}
	return owner_perm;
}

/************************************************************************/
int APP_CC
rdpfs_get_other_permission(mode_t mode)
{
	int other_perm = 0;

	if( mode & 0002)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_get_other_permission]: "
					"Other can write");
		other_perm |= FILE_SHARE_WRITE;
	}
	if( mode & 0004)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_get_other_permission]: "
					"Other can read");
		other_perm |= FILE_SHARE_READ;
	}
	return other_perm;
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
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_open]: "
			"Initialise the rdpfs cache");
	rdpfs_cache_init();
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

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_create]:"
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

	actions[completion_id].last_req = IRP_MJ_CLOSE;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_request_close]:"
			"close device id %i\n", actions[completion_id].device);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
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
	int query_length;
	make_stream(s);
	init_stream(s,1024);

	g_strcpy(actions[completion_id].path, query);
	actions[completion_id].last_req = IRP_MJ_QUERY_VOLUME_INFORMATION;
	actions[completion_id].request_param = information;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_query_volume_information]:"
  		"Process job[%s]",query);
	query_length = (g_strlen(query)+1)*2;
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_QUERY_VOLUME_INFORMATION); 			/* major version */
	out_uint32_le(s, 0);																		/* minor version */

	out_uint32_le(s, information);													/* FsInformationClass */
	out_uint32_le(s, query_length);                         /* length */
	out_uint8s(s, 24);                                      /* padding */
	out_uint8p(s, query, query_length);                     /* query */

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
	actions[completion_id].last_req = IRP_MJ_QUERY_INFORMATION;
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
void APP_CC
rdpfs_query_directory(int completion_id, int device_id, int information, const char* query )
{
	struct stream* s;
	int index;
	int query_length;
	make_stream(s);
	init_stream(s,1024);

	if (g_strcmp(query, "") != 0)
	{
		g_strcpy(actions[completion_id].path, query);
	}
	actions[completion_id].last_req = IRP_MN_QUERY_DIRECTORY;
	actions[completion_id].request_param = information;

	query_length = (g_strlen(query)+1)*2;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_query_directory]:"
  		"Process query[%s]",query);
	out_uint16_le(s, RDPDR_CTYP_CORE);
	out_uint16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
	out_uint32_le(s, actions[completion_id].device);
	out_uint32_le(s, actions[completion_id].file_id);
	out_uint32_le(s, completion_id);
	out_uint32_le(s, IRP_MJ_DIRECTORY_CONTROL);           /* major version */
	out_uint32_le(s, IRP_MN_QUERY_DIRECTORY);             /* minor version */

	out_uint32_le(s, information);                        /* FsInformationClass */
	out_uint8(s, 1);                                      /* path is considered ? */
	out_uint32_le(s, query_length);                       /* length */
	out_uint8s(s, 23);                                    /* padding */
	rdp_out_unistr(s, (char*)query, query_length);     /* query */

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

/************************************************************************/
int APP_CC
rdpfs_add(struct stream* s, int device_data_length,
								int device_id, char* dos_name)
{
	disk_devices[disk_devices_count].device_id = device_id;
	g_strcpy(disk_devices[disk_devices_count].dir_name, dos_name);
	disk_devices_count++;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_add]: "
				"Succedd to add disk");
  return disk_devices_count-1;
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
    handle = rdpfs_add(s, device_data_length, device_id, dos_name);
    s->p = p + device_data_length;
    if (handle != 1)
    {
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
rdpfs_process_directory_response(int completion_id, struct stream* s)
{
	if(rdpfs_response[completion_id].request_status != 0)
	{
		return 0;
	}
	int is_last_entry;
	int length;
	int filename_length;
	int short_filename_length;

	struct fs_info* file_response;
	int label_length;
	int object_fs;
	uint64_t time;
	int ignored;
	int fs_name_len;
	char path[256];
	struct disk_device* disk;

	file_response = &rdpfs_response[completion_id].fs_inf;
	in_uint32_le(s, length);			/* response length */
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_directory_response]: "
			"IRP_MN_QUERY_DIRECTORY response : response length : %i", length);

	if (actions[completion_id].request_param != FileBothDirectoryInformation)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_directory_response]: "
				"IRP_MN_QUERY_DIRECTORY response : bad flag for request parameter : %04x", actions[completion_id].request_param);
		return 1;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
			"IRP_MJ_QUERY_DIRECTORY response : extract FileBothDirectoryInformation information");

	in_uint32_le(s, is_last_entry );
	in_uint8s(s, 4);

	in_uint64_le(s, time);	/* creation time */
	file_response->create_access_time = convert_1970_to_filetime(time);

	in_uint64_le(s, time);	/* last_access_time */
	file_response->last_access_time = convert_1970_to_filetime(time);

	in_uint64_le(s, time);	/* last_write_time */
	file_response->last_write_time = convert_1970_to_filetime(time);

	in_uint64_le(s, time);	/* last_change_time */
	file_response->last_change_time = convert_1970_to_filetime(time);

	in_uint64_le(s, file_response->file_size);	/* filesize */
	in_uint64_le(s, file_response->allocation_size);   /* allocated filesize  */
	in_uint32_le(s, file_response->file_attributes);   /* attributes  */

	in_uint32_le(s, filename_length);        /* unicode filename length */
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_volume_information_response]: "
			"filename length : %i", filename_length);

	in_uint8s(s, 29);               /* we ignore the short file name */
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_volume_information_response]: "
			"message : ");
	log_hexdump(l_config, LOG_LEVEL_DEBUG, s->p, filename_length);

	rdp_in_unistr(s, file_response->filename, sizeof(file_response->filename), filename_length);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_volume_information_response]: "
						"IRP_MJ_QUERY_DIRECTORY response : get filename : %s", file_response->filename);

	disk = rdpfs_get_dir(actions[completion_id].device);
	g_sprintf(path, "/%s%s",  disk->dir_name, actions[completion_id].path);
	path[g_strlen(path)-1] = 0;
	g_sprintf(path, "%s%s",  path, file_response->filename);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_volume_information_response]: "
							"IRP_MJ_QUERY_DIRECTORY response : path added in cache : %s", path);

	rdpfs_cache_add_fs(g_strdup(path), file_response);


	return 0;
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

		in_uint64_le(s, rep->fs_inf.create_access_time);    /* volume creation time */
		in_uint8s(s, 4);                                    /* serial (ignored) */
		in_uint32_le(s, label_length);	                        /* length of string */
		in_uint8(s, object_fs);	                                /* support objects? */
		if (object_fs != 0)
		{
			log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[rdpfs_process_iocompletion]: "
					"IRP_MJ_QUERY_VOLUME_INFORMATION response : Xrdp did not support object file system");
		}
		rdp_in_unistr(s, rep->fs_inf.filename, sizeof(rep->fs_inf.filename), label_length);
		break;

	case FileFsSizeInformation:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
						"IRP_MJ_QUERY_VOLUME_INFORMATION response : extract FileFsSizeInformation information");

//		in_uint32_le(s, low);                          /* Total allocation units low */
//		in_uint32_le(s, high);                         /* Total allocation high units */
//		in_uint32_le(s, rep->volume_inf.f_bfree);	     /* Available allocation units */
//		in_uint32_le(s, rep->volume_inf.f_blocks);     /* Available allowcation units */
//		in_uint32_le(s, ignored);                      /* Sectors per allocation unit */
//		in_uint32_le(s, ignored);                      /* Bytes per sector */
		break;

	case FileFsAttributeInformation:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
						"IRP_MJ_QUERY_VOLUME_INFORMATION response : extract FileFsAttributeInformation information");

//		in_uint32_le(s, ignored);                      /* fs attributes */
//		in_uint32_le(s, rep->volume_inf.f_namelen);	   /* max length of filename */
//
//		in_uint32_le(s, fs_name_len);                  /* length of fs_type */
//		rdp_in_unistr(s, rep->volume_inf.fs_type, sizeof(rep->volume_inf.fs_type), fs_name_len);
		break;

	case FileFsDeviceInformation:
	case FileFsFullSizeInformation:

	default:

		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_volume_information_response]: "
						"IRP_MJ_QUERY_VOLUME_INFORMATION response : unknow response");
		return 1;
	}
	return 0;
}

/*****************************************************************************/
int APP_CC
rdpfs_process_information_response(int completion_id, struct stream* s)
{
	int length;
	struct request_response* rep;
	int label_length;
	int object_fs;
	int ignored;
	int fs_name_len;
	uint64_t time;

	rep = &rdpfs_response[completion_id];

	in_uint32_le(s, length);			/* response length */
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_information_response]: "
			"IRP_MJ_QUERY_INFORMATION response : response length : %i", length);
	rep->Request_type = RDPDR_IRP_MJ_QUERY_INFORMATION;


	switch (actions[completion_id].request_param)
	{
	case FileBasicInformation:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_information_response]: "
				"IRP_MJ_QUERY_INFORMATION response : param response FileBasicInformation");

		in_uint64_le(s, time);	/* create_access_time */
		rep->fs_inf.create_access_time = convert_1970_to_filetime(time);

		in_uint64_le(s, time);	/* last_access_time */
		rep->fs_inf.last_access_time = convert_1970_to_filetime(time);

		in_uint64_le(s, time);	/* last_write_time */
		rep->fs_inf.last_write_time = convert_1970_to_filetime(time);

		in_uint64_le(s, time);	/* last_change_time */
		rep->fs_inf.last_change_time = convert_1970_to_filetime(time);

		in_uint32_le(s, rep->fs_inf.file_attributes);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_information_response]: "
				"IRP_MJ_QUERY_INFORMATION response : param response FileBasicInformation : %04x", rep->fs_inf.file_attributes);
		break;

	case FileStandardInformation:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_information_response]: "
				"IRP_MJ_QUERY_INFORMATION response : param response FileStandardInformation");
		in_uint64_le(s, rep->fs_inf.allocation_size);       /* Allocation size */
		in_uint64_le(s, rep->fs_inf.file_size);             /* End of file */
		in_uint32_le(s, rep->fs_inf.nlink);                 /* Number of links */
		in_uint32_le(s, rep->fs_inf.detele_request);        /* Delete pending */
		in_uint8(s, rep->fs_inf.is_dir);                     /* Directory */
		in_uint32_le(s, ignored);
		break;

	default:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_information_response]: "
						"IRP_MJ_QUERY_INFORMATION response : unknow response");
		return 0;
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

	rdpfs_response[completion_id].request_status = io_status;
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

	case IRP_MN_QUERY_DIRECTORY:
		result = rdpfs_process_directory_response(completion_id, s);
		break;

	case IRP_MJ_WRITE:
		in_uint32_le(s, size);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
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
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
				"file '%s' closed",actions[completion_id].path);
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
				"toto");

		result = 0;
		break;
	default:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[rdpfs_process_iocompletion]: "
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

