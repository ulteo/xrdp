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


#include "rdpdr.h"
#include "disk_dev.h"
#include "rdpfs.h"

struct log_config	*l_config;
int disk_up = 0;
char username[256];
char mount_point[256];
pthread_t vchannel_thread;


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
	int i;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_deinit]: "
						"unmounting drive");
	rdpfs_close();
	fuse_unmount(mount_point);
	g_exit(0);

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
		rv = rdpfs_receive(s->data, &length, &total_length);

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
			rdpfs_process_message(s, length, total_length);
			break;
		}
		free_stream(s);
	}
	pthread_exit (0);
}


/*****************************************************************************/
int main(int argc, char** argv, char** environ)
{
	int fuse_group = 0;
	int ok = 0;
	l_config = g_malloc(sizeof(struct log_config), 1);
	if (argc != 2)
	{
		g_printf("Usage : rdpdr_disk USERNAME\n");
		return 1;
	}
	//decrease_right

	if (disk_init() != LOG_STARTUP_OK)
	{
		g_printf("rdpdr_disk[main]: Enable to init log system\n");
		g_free(l_config);
		return 1;
	}

	if ( g_getuser_info(argv[1], 0, 0, 0, 0, 0) == 1)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[main]: "
				"The username '%s' did not exist\n", argv[1]);
	}
	g_strncpy(username, argv[1], sizeof(username));
	g_getgroup_info("fuse", &fuse_group);
	if (g_check_user_in_group(username, fuse_group, &ok) == 1)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[main]: "
				"Error while testing if user %s is member of fuse group", username);
		return 1;
	}
	if (ok == 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[main]: "
				"User %s is not allow to use fuse", username);
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
	if (rdpfs_open() == 1)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[main]: "
					"Unable to open a connection to RDP filesystem");
	}

	g_sprintf(mount_point, "/home/%s/.rdp_drive", username);
	return fuse_run();
}
