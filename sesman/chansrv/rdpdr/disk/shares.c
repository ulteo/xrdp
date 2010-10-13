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


#include <shares.h>

extern struct log_config *l_config;


/*****************************************************************************/
int share_add_to_desktop(const char* share_name){
	char* home_dir = g_getenv("HOME");
	char desktop_file_path[1024] = {0};
	char share_path[256] = {0};
	char file_content[1024] = {0};
	int handle = 0;

	g_snprintf((char*)desktop_file_path, 256, "%s/Desktop", home_dir);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_add_to_desktop]: "
	        		"Desktop file path : %s", desktop_file_path);


	g_mkdir(desktop_file_path);
	if (g_file_exist(desktop_file_path) == 0){
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[share_add_to_desktop]: "
		        		"Desktop already exist");
		return 1;
	}

	g_snprintf((char*)desktop_file_path, sizeof(desktop_file_path), "%s/Desktop/%s.desktop", home_dir, share_name);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_add_to_desktop]: "
	        		"Desktop file path : %s", desktop_file_path);

	if (g_file_exist(desktop_file_path) != 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[share_add_to_desktop]: "
		        		"Share already exist");
		return 0;
	}

	g_snprintf(share_path, sizeof(desktop_file_path), "%s/%s/%s", home_dir, RDPDRIVE_NAME, share_name);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_add_to_desktop]: "
		        		"Share path : %s", share_path);

	g_strcpy(file_content, DESKTOP_FILE_TEMPLATE);
	g_str_replace_first(file_content, "%ShareName%", (char*)share_name);
	g_str_replace_first(file_content, "%SharePath%", share_path);

	handle = g_file_open(desktop_file_path);
	if(handle < 0)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_add_to_desktop]: "
		        		"Unable to create : %s", desktop_file_path);
		return 1;
	}
	g_file_write(handle, file_content, g_strlen(file_content));
	g_file_close(handle);

	return 0;
}

/*****************************************************************************/
int share_remove_from_desktop(const char* share_name){
	char* home_dir = g_getenv("HOME");
	char desktop_file_path[1024] = {0};
	char share_path[1024] = {0};
	char file_content[1024] = {0};
	int handle = 0;

	g_snprintf((char*)desktop_file_path, sizeof(desktop_file_path), "%s/Desktop", home_dir);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_remove_from_desktop]: "
	        		"Desktop file path : %s", desktop_file_path);

	g_snprintf((char*)desktop_file_path, sizeof(desktop_file_path), "%s/Desktop/%s.desktop", home_dir, share_name);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_remove_from_desktop]: "
	        		"Desktop file path : %s", desktop_file_path);

	if (g_file_exist(desktop_file_path) != 0)
	{
		g_file_delete(desktop_file_path);
		return 0;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_remove_from_desktop]: "
			"Desktop file did not exixt");

	return 1;
}

