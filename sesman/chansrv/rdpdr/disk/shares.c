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


static int file_contain(char* filename, char* pattern)
{
	char* buffer = NULL;
	char* pos = NULL;
	int res = 0;
	int file_size;
	int fd = 0;

	if (! g_file_exist(filename))
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[file_contain]: "
				"%s do not exist", filename);
		return 1;
	}
	file_size = g_file_size(filename);
	if (file_size < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[file_contain]: "
				"Unable to get size of file %s", filename);
		return 1;
	}
	buffer = g_malloc(file_size, 1);
	fd = g_file_open(filename);
	if (fd < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[file_contain]: "
		        		"Unable to open file %s [%s]", filename, strerror(errno));
		goto fail;
	}
	if (g_file_read(fd, buffer, file_size) < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[file_contain]: "
				"Unable to read the file %s [%s]", filename, strerror(errno));
		goto fail;
	}
	pos = g_strstr(buffer, pattern);
	if (pos == NULL)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[file_contain]: "
				"Unable to find the pattern %s in the file %s", pattern, filename);
		res = 1;
	}

	if (buffer)
	{
		g_free(buffer);
	}	return res;

fail:
	if (buffer)
	{
		g_free(buffer);
	}
	return 1;
}


int
share_desktop_purge()
{
	char* home_dir = g_getenv("HOME");
	char path[1024] = {0};
	char share_preffix[1024] = {0};
	char desktop_file_path[1024] = {0};
	struct dirent *dir_entry = NULL;
	DIR *dir;

	g_snprintf((char*)desktop_file_path, sizeof(desktop_file_path), "%s/Desktop", home_dir);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_purge]: "
	        		"Desktop file path : %s", desktop_file_path);

	g_snprintf(share_preffix, sizeof(desktop_file_path), "%s/%s", home_dir, RDPDRIVE_NAME);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_purge]: "
		        		"Share path : %s", share_preffix);

	dir = opendir(desktop_file_path);
	if( dir == NULL)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_purge]: "
		        		"Unable to open the directory: %s", desktop_file_path);
		return 1;
	}
	while ((dir_entry = readdir(dir)) != NULL)
	{
		if((g_strcmp(dir_entry->d_name, ".") == 0)
				|| (g_strcmp(dir_entry->d_name, "..") == 0)
				|| ((dir_entry->d_type == DT_DIR) == 0))
		{
			continue;
		}
		g_snprintf(path, sizeof(path), "%s/%s", desktop_file_path, dir_entry->d_name);

		if (g_str_end_with(path, ".desktop") == 0)
		{
			if (file_contain(path, share_preffix) == 0)
			{
				log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_purge]: "
					        		"Desktop file : %s", path);
				g_file_delete(path);
			}
		}
	}

	closedir(dir);
	return 0;
}

/*****************************************************************************/
int
share_add_to_desktop(const char* share_name)
{
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

/*****************************************************************************/
int share_add_to_bookmark(const char* share_name){
	char* home_dir = g_getenv("HOME");
	char bookmark_file_content[1024] = {0};
	char bookmark_file_path[1024] = {0};
	char *escaped_bookmark_file_content;
	int fd;
	int error;

	g_snprintf((char*)bookmark_file_path, 256, "%s/%s", home_dir, BOOKMARK_FILENAME);

	if (g_file_exist(bookmark_file_path) == 0){
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_add_to_bookmark]: "
		        		"Bookmark already exist");
	}

	fd = g_file_append(bookmark_file_path);
	if (fd < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[share_add_to_bookmark]: "
		        		"Unable to open file %s", bookmark_file_path);
		return 1;
	}

	g_snprintf(bookmark_file_content, sizeof(bookmark_file_content), "file://%s/%s/%s\n", home_dir, RDPDRIVE_NAME, share_name);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_add_to_bookmark]: "
		        		"Entry to add: %s", bookmark_file_content);

	escaped_bookmark_file_content = w_markup_escape_text(bookmark_file_content, -1);

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_add_to_bookmark]: "
		        		"Entry escaped added: %s", bookmark_file_content);

	error = g_file_write(fd, escaped_bookmark_file_content, g_strlen(escaped_bookmark_file_content));
	g_free(escaped_bookmark_file_content);
	if (error < 0 )
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[share_add_to_bookmark]: "
		        		"Unable to write in the file %s [%s]", bookmark_file_path, strerror(errno));
		return 1;
	}
	g_file_close(fd);

}


static int
replace_in_file(char* filename, char* pattern)
{
	int file_size = 0;
	int pattern_size = 0;
	char* buffer = NULL;
	char* pos = NULL;
	int file_seek = 0;
	int fd = 0;

	pattern_size = g_strlen(pattern);
	file_size = g_file_size(filename);
	if (file_size < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to get size of file %s", filename);
		return 1;
	}
	buffer = g_malloc(file_size, 0);
	fd = g_file_open(filename);
	if (fd < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
		        		"Unable to open file %s", filename);
		goto fail;
	}
	if (g_file_read(fd, buffer, file_size) < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to read the file %s [%s]", filename, strerror(errno));
		goto fail;
	}

	pos = g_strstr(buffer, pattern);
	if (pos == NULL)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to find the pattern %s in the file %s", pattern, filename);
		goto fail;
	}
	file_seek = pos - buffer;
	if (file_seek < 0 || file_seek > file_size)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Invalid position %i", file_seek);
		goto fail;
	}
	if (g_file_seek(fd, file_seek + pattern_size) < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to go to the position %i [%s]", file_seek, strerror(errno));
	}
	if (g_file_read(fd, pos, file_size-pattern_size) < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to read the end of the file %s", filename);
		goto fail;
	}

	g_file_close(fd);
	if (g_file_delete(filename) == 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to delete file %s", filename);
		goto fail;
	}
	fd = g_file_open(filename);
	if (fd < 0)
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to open file %s", filename);
		goto fail;
	}
	if (g_file_write(fd, buffer, file_size - pattern_size) < 0)
	{
				log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[replace_in_file]: "
				"Unable to write the file %s", filename);
		goto fail;
	}

	g_file_close(fd);
	return 0;

fail:
	if (buffer)
	{
		g_free(buffer);
	}
	return 1;
}

/*****************************************************************************/
int share_remove_from_bookmarks(const char* share_name){
	char* home_dir = g_getenv("HOME");
	char bookmark_file_content[1024] = {0};
	char bookmark_file_path[1024] = {0};
	char *escaped_bookmark_file_content;
	int fd;
	int error;

	g_snprintf((char*)bookmark_file_path, 256, "%s/%s", home_dir, BOOKMARK_FILENAME);

	g_snprintf(bookmark_file_content, sizeof(bookmark_file_content), "file://%s/%s/%s\n", home_dir, RDPDRIVE_NAME, share_name);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_remove_from_bookmarks]: "
		        		"Entry to remove: %s", bookmark_file_content);

	escaped_bookmark_file_content = w_markup_escape_text(bookmark_file_content, -1);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[share_remove_from_bookmarks]: "
		        		"Entry escaped to remove: %s", escaped_bookmark_file_content);

	if (replace_in_file(bookmark_file_path, escaped_bookmark_file_content) == 1 )
	{
		log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[share_remove_from_bookmarks]: "
		        		"Unable to remove entry in the file %s ", bookmark_file_path);
		return 1;
	}
	return 0;
}

/*****************************************************************************/
int
share_bookmark_purge()
{
	return 0;
}


