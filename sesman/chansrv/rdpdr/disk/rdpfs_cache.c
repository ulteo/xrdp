/**
 * Copyright (C) 2008 Ulteo SAS
 * http://www.ulteo.com
 * Author David Lechevalier <david@ulteo.com> 2010
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


#include "rdpfs_cache.h"
#include "rdpfs.h"


static struct rdpfs_volume_cache volume_cache;
static struct rdpfs_direntry_cache direntry_cache;

static void rdpfs_destroyEntry(void *arg, char *key, void *value)
{
//  free(key);
//  free(value);
}



/************************************************************************/
static void rdpfs_update_fs_cache_index()
{
	struct fs_info* fs;
	if(direntry_cache.index == RDPFS_CACHE_FS_SIZE - 1)
	{
		direntry_cache.index = 0;
		return;
	}
	else
	{
		direntry_cache.index++;
	}
	fs = &direntry_cache.fs_list[direntry_cache.index];
	fs->allocation_size     = 0;
	fs->create_access_time  = 0;
	fs->detele_request      = 0;
	fs->file_attributes     = 0;
	fs->file_size           = 0;
	fs->is_dir              = 0;
	fs->last_access_time    = 0;
	fs->last_change_time    = 0;
	fs->last_write_time     = 0;
	fs->nlink               = 0;
	fs->filename[0]         = 0;
	deleteFromHashMap(direntry_cache.fs_map, fs->filename);
	fs->filename[0] = 0;
}

/************************************************************************/
void rdpfs_cache_init()
{
	direntry_cache.fs_list = g_malloc(RDPFS_CACHE_FS_SIZE * sizeof(struct fs_info*), 0);
	direntry_cache.index = 0;
	direntry_cache.fs_map = newHashMap(rdpfs_destroyEntry, NULL);
	direntry_cache.fs_map->mapSize = RDPFS_CACHE_FS_SIZE;
}

/************************************************************************/
void rdpfs_cache_dinit()
{
	g_free(direntry_cache.fs_list);
	deleteHashMap(direntry_cache.fs_map);
}




/************************************************************************/
void APP_CC
rdpfs_cache_add_fs(const char* path, struct fs_info* fs_inf)
{
	struct fs_info* fs;

	/* test if present */
	fs = getFromHashMap(direntry_cache.fs_map, path);
	if( fs == NULL)
	{
		fs = &direntry_cache.fs_list[direntry_cache.index];
		fs->allocation_size = fs_inf->allocation_size;
		fs->create_access_time = fs_inf->create_access_time;
		fs->detele_request = fs_inf->detele_request;
		fs->file_attributes = fs_inf->file_attributes;
		fs->file_size = fs_inf->file_size;
		fs->is_dir = fs_inf->is_dir;
		fs->last_access_time = fs_inf->last_access_time;
		fs->last_change_time = fs_inf->last_change_time;
		fs->last_write_time = fs_inf->last_write_time;
		fs->nlink = fs_inf->nlink;
		strncpy(fs->filename, fs_inf->filename, 256);

		rdpfs_update_fs_cache_index();
		addToHashMap(direntry_cache.fs_map, fs->filename, fs);
		return;
	}
	fs->allocation_size = fs_inf->allocation_size;
	fs->create_access_time = fs_inf->create_access_time;
	fs->detele_request = fs_inf->detele_request;
	fs->file_attributes = fs_inf->file_attributes;
	fs->file_size = fs_inf->file_size;
	fs->is_dir = fs_inf->is_dir;
	fs->last_access_time = fs_inf->last_access_time;
	fs->last_change_time = fs_inf->last_change_time;
	fs->last_write_time = fs_inf->last_write_time;
	fs->nlink = fs_inf->nlink;
	strncpy(fs->filename, fs_inf->filename, 256);
	return;
}

/************************************************************************/
struct fs_info*
rdpfs_cache_get_fs(const char* fs_name)
{
	return (struct fs_info*)getFromHashMap(direntry_cache.fs_map, fs_name);
}

/************************************************************************/
void APP_CC
rdpfs_cache_remove_fs(const char* fs_name)
{

}

/************************************************************************/
int APP_CC
rdpfs_cache_contain_fs(const char* fs_name)
{
	return 1;
}


