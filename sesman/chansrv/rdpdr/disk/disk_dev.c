/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` fusexmp.c -o fusexmp
*/
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

#define FUSE_USE_VERSION 26



#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include "disk_dev.h"
#include "rdpfs.h"
#include "rdpfs_cache.h"

extern pthread_t vchannel_thread;
extern void *thread_vchannel_process (void * arg);
extern struct log_config *l_config;
extern char mount_point[256];
extern int rdpdr_sock;
extern int disk_up;
extern struct request_response rdpfs_response[128];




/************************************************************************
	Begin of fuse operation
 */

/************************************************************************/
static struct fs_info* disk_dev_file_getattr(struct disk_device* disk, const char* path, struct stat *stbuf, int *error)
{
	int i;
	int completion_id = 0;
	int status;

	completion_id = rdpfs_create(disk->device_id, GENERIC_READ|FILE_EXECUTE_ATTRIBUTES,	FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,	FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, path);
	rdpfs_wait_reply(completion_id);
	status = rdpfs_response[completion_id].request_status;
	switch(status)
	{
	case STATUS_SUCCESS:
		rdpfs_query_information(completion_id, disk->device_id, FileBasicInformation, path);
		rdpfs_wait_reply(completion_id);
		rdpfs_query_information(completion_id, disk->device_id, FileStandardInformation,path);
		rdpfs_wait_reply(completion_id);
		rdpfs_request_close(completion_id, disk->device_id);
		rdpfs_wait_reply(completion_id);
		return &rdpfs_response[completion_id].fs_inf;

	case STATUS_NO_SUCH_FILE:
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_file_getattr]: "
					"file did not exist");
		*error = -ENOENT;
		return NULL;
	default:
		*error = -EIO;
		return NULL;
	}
}

/************************************************************************/
static int disk_dev_getattr(const char *path, struct stat *stbuf)
{
	struct disk_device *disk;
	char* rdp_path;
	struct fs_info* fs;
	int error;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_getattr]: "
				"getattr on %s", path);
	if (disk_up == 0)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_getattr]: "
				"get_attr return");
		return -1;
	}

	rdp_path = g_strdup(path);
	if (strcmp(rdp_path, "/") == 0) {
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_getattr]: "
				"%i device mounted ", rdpfs_get_device_count());
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = rdpfs_get_device_count() + 2;
		return 0;
	}

	fs = rdpfs_cache_get_fs(rdp_path);
	if (fs == NULL)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_getattr]: "
				"%s not in cache", rdp_path);
		disk = rdpfs_get_device_from_path(rdp_path);
		if ( disk == 0)
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_getattr]: "
					"Device from path(%s) did not exists", rdp_path);
			return 0;
		}
		g_str_replace_first(rdp_path, disk->dir_name, "");


		if (strcmp(rdp_path, "/") == 0)
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_getattr]: "
					"request (disk_dev_volume_getattr)");
			fs = disk_dev_file_getattr(disk, "", stbuf, &error);
		}
		else
		{
			fs = disk_dev_file_getattr(disk, rdp_path+1, stbuf, &error);
		}
		if (fs == NULL)
		{
			return error;
		}
		rdpfs_cache_add_fs(g_strdup(path), fs);
	}

	return rdpfs_convert_fs_to_stat(fs, stbuf);
}

/************************************************************************/
static int disk_dev_access(const char *path, int mask)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_access]: "
				"access on %s", path);

	return F_OK;
}

/************************************************************************/
static int disk_dev_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	int i;
	int index;
	(void) fi;
	(void) offset;
	struct disk_device *disk;
	char rdp_path[256];
	int completion_id;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
				"readdir on %s", path);

	if (strcmp(path, "/") == 0)
	{
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		for (i=0 ; i<rdpfs_get_device_count() ; i++)
		{
			disk = rdpfs_get_device_by_index(i);
			filler(buf, disk->dir_name , NULL, 0);
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
					"the device name is : %s", disk->dir_name);

		}
		return 0;
	}

	disk = rdpfs_get_device_from_path(path);
	if(disk == 0)
	{
		return 0;
	}

	g_sprintf(rdp_path, "%s/", path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
				"Request rdp path %s", rdp_path );

	completion_id = rdpfs_create(disk->device_id,
                               GENERIC_ALL|DELETE,
                               FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,
                               FILE_OPEN,
                               FILE_SYNCHRONOUS_IO_NONALERT|FILE_DIRECTORY_FILE,
                               rdp_path);
	rdpfs_wait_reply(completion_id);
	g_sprintf(rdp_path, "%s*", rdp_path);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
				"Request rdp path %s", rdp_path );


	struct stat st;
	rdpfs_query_directory(completion_id, disk->device_id, FileBothDirectoryInformation, rdp_path);
	rdpfs_wait_reply(completion_id);
	if (rdpfs_response[completion_id].request_status == 0)
	{
		if (filler(buf, rdpfs_response[completion_id].fs_inf.filename, NULL, 0))
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
						"Failed to add the file %s", rdpfs_response[completion_id].fs_inf.filename);
		}
	}
	while( rdpfs_response[completion_id].request_status == 0 )
	{
		rdpfs_query_directory(completion_id, disk->device_id, FileBothDirectoryInformation, "");
		rdpfs_wait_reply(completion_id);
		if (rdpfs_response[completion_id].request_status == 0)
		{
			log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
						"Add %s in filler", rdpfs_response[completion_id].fs_inf.filename);
			if (filler(buf, rdpfs_response[completion_id].fs_inf.filename, NULL, 0))
			{
				log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
							"Failed to add the file %s", rdpfs_response[completion_id].fs_inf.filename);
			}
		}
	}
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
				"Fin");

	return 0;
}

/************************************************************************/
static int disk_dev_mknod(const char *path, mode_t mode, dev_t rdev)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_mknod] ");
	int res;
	int completion_id = 0;
	struct disk_device* disk;
	int owner_perm = 0;
	int other_perm = 0;
	int attributes = 0;
	char* rdp_path;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_mknod]: rdev : %i", rdev);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_mknod]: rdev : %i", mode);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_mknod]: rdev : %s", path);

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_mknod]:"
					"Unable to get device from path : %s", path);
		return -errno;
	}


	owner_perm = rdpfs_get_owner_permission(mode);
	other_perm = rdpfs_get_other_permission(mode);

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	attributes = FILE_SYNCHRONOUS_IO_NONALERT |FILE_NON_DIRECTORY_FILE;
	completion_id = rdpfs_create(disk->device_id, owner_perm,	other_perm,	FILE_CREATE, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
		return -errno;
	}
	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);

	return 0;
}

/************************************************************************/
static int disk_dev_mkdir(const char *path, mode_t mode)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_mkdir] ");
	int res;
	int completion_id = 0;
	struct disk_device* disk;
	int owner_perm = 0;
	int other_perm = 0;
	int attributes = 0;
	char* rdp_path;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_mkdir]: rdev : %i", mode);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_mkdir]: rdev : %s", path);

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_mknod]:"
					"Unable to get device from path : %s", path);
		return -errno;
	}

	owner_perm = rdpfs_get_owner_permission(mode);
	other_perm = rdpfs_get_other_permission(mode);

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	attributes = FILE_SYNCHRONOUS_IO_NONALERT |FILE_DIRECTORY_FILE;
	completion_id = rdpfs_create(disk->device_id, owner_perm,	other_perm,	FILE_CREATE, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
		return -errno;
	}
	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);

	return 0;
}

/************************************************************************/
static int disk_dev_unlink(const char *path)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_unlink] ");
	int completion_id = 0;
	struct disk_device* disk;
	int attributes = 0;
	int desired_access = 0;
	int shared_access = 0;
	char* rdp_path;
	struct fs_info fs;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_unlink]: rdev : %s", path);

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_unlink]:"
					"Unable to get device from path : %s", path);
		return -errno;
	}

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	fs.delele_request = 1;

	attributes = FILE_SYNCHRONOUS_IO_NONALERT;
	desired_access = GENERIC_READ|FILE_EXECUTE_ATTRIBUTES;
	shared_access = FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE;
	completion_id = rdpfs_create(disk->device_id, desired_access , shared_access,	FILE_OPEN, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
		return -errno;
	}
	rdpfs_query_setinformation(completion_id, FileDispositionInformation, &fs);
	rdpfs_wait_reply(completion_id);
	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);

	return 0;
}

/************************************************************************/
static int disk_dev_rmdir(const char *path)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_unlink] ");
	int completion_id = 0;
	struct disk_device* disk;
	int attributes = 0;
	int desired_access = 0;
	int shared_access = 0;
	char* rdp_path;
	struct fs_info fs;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_rmdir]: rdev : %s", path);

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_rmdir]:"
					"Unable to get device from path : %s", path);
		return -errno;
	}

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	fs.delele_request = 1;

	attributes = FILE_SYNCHRONOUS_IO_NONALERT;
	desired_access = GENERIC_READ|FILE_EXECUTE_ATTRIBUTES;
	shared_access = FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE;
	completion_id = rdpfs_create(disk->device_id, desired_access , shared_access,	FILE_OPEN, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
		return -errno;
	}
	rdpfs_query_setinformation(completion_id, FileDispositionInformation, &fs);
	rdpfs_wait_reply(completion_id);
	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);
}

/************************************************************************/
static int disk_dev_rename(const char *from, const char *to)
 {
	int completion_id = 0;
	struct disk_device* disk;
	int attributes = 0;
	int desired_access = 0;
	int shared_access = 0;
	char* rdp_path;
	struct fs_info fs;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_rename]: "
			"change from %s to %s", from, to);

	disk = rdpfs_get_device_from_path(to);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_rename]:"
					"Unable to get device from path : %s", from);
 		return -errno;
	}

	rdp_path = g_strdup(from);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	g_strcpy(fs.filename, rdp_path);

	attributes = FILE_SYNCHRONOUS_IO_NONALERT;
	desired_access = GENERIC_READ|FILE_EXECUTE_ATTRIBUTES;
	shared_access = FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE;
	completion_id = rdpfs_create(disk->device_id, desired_access , shared_access,	FILE_OPEN, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
 		return -errno;
	}

	rdp_path = g_strdup(to);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	g_strcpy(fs.filename, rdp_path);

	rdpfs_query_setinformation(completion_id, FileRenameInformation, &fs);
	rdpfs_wait_reply(completion_id);
	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);
 	return 0;
 }

/************************************************************************/
static int disk_dev_chmod(const char *path, mode_t mode)
 {
	int completion_id = 0;
	struct disk_device* disk;
	int attributes = 0;
	int desired_access = 0;
	int shared_access = 0;
	char* rdp_path;
	struct fs_info* fs;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_chmod]: "
			"Path : %s", path);

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_chmod]:"
					"Unable to get device from path : %s", path);
 		return -errno;
	}

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	//fs.file_size = size;

	attributes = FILE_SYNCHRONOUS_IO_NONALERT;
	desired_access = GENERIC_READ|FILE_EXECUTE_ATTRIBUTES;
	shared_access = FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE;
	completion_id = rdpfs_create(disk->device_id, desired_access , shared_access,	FILE_OPEN, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
 		return -errno;
	}
	fs = rdpfs_cache_get_fs(path);
	if (fs == NULL)
	{
		rdpfs_query_information(completion_id, disk->device_id, FileBasicInformation, rdp_path);
		rdpfs_wait_reply(completion_id);
		fs = &rdpfs_response[completion_id].fs_inf;
	}
	fs->file_attributes = mode;

	rdpfs_query_setinformation(completion_id, FileBasicInformation, fs);
	rdpfs_wait_reply(completion_id);
	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);

}

/************************************************************************/
static int disk_dev_chown(const char *path, uid_t uid, gid_t gid)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_chown]: "
				"Chown operation is not supported");
	return 0;
}

/************************************************************************/
static int disk_dev_truncate(const char *path, off_t size)
{
	int completion_id = 0;
	struct disk_device* disk;
	int attributes = 0;
	int desired_access = 0;
	int shared_access = 0;
	char* rdp_path;
	struct fs_info fs;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_truncate]: "
			"path : %s", path);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_truncate]: "
			"size : %i", size);

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_truncate]:"
					"Unable to get device from path : %s", path);
		return -errno;
	}

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");

	fs.file_size = size;

	attributes = FILE_SYNCHRONOUS_IO_NONALERT;
	desired_access = GENERIC_READ|FILE_EXECUTE_ATTRIBUTES;
	shared_access = FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE;
	completion_id = rdpfs_create(disk->device_id, desired_access , shared_access,	FILE_OPEN, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
		return -errno;
	}
	rdpfs_query_setinformation(completion_id, FileEndOfFileInformation, &fs);
	rdpfs_wait_reply(completion_id);
	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);

	return 0;
}

/************************************************************************/
static int disk_dev_utimens(const char *path, const struct timespec ts[2])
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_utimens");
	return 0;
}

/************************************************************************/
static int disk_dev_open(const char *path, struct fuse_file_info *fi)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_open");
	return 0;
}

/************************************************************************/
static int disk_dev_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_read]: "
			"Path to read : %s", path);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_read]: "
			"Size to read : %i", size);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_read]: "
			"Read from : %i", offset);

	int completion_id = 0;
	struct disk_device* disk;
	int attributes = 0;
	int desired_access = 0;
	int shared_access = 0;
	char* rdp_path;
	struct fs_info* fs;
	int current_size = 0;
	int current_offset = offset;
	int size_read = 1;
	int chunk_size_to_read;
	int size_to_read = size;

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_read]:"
					"Unable to get device from path : %s", path);
		return -errno;
	}

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");


	attributes = FILE_SYNCHRONOUS_IO_NONALERT;
	desired_access = GENERIC_READ|FILE_EXECUTE_ATTRIBUTES;
	shared_access = FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE;
	completion_id = rdpfs_create(disk->device_id, desired_access , shared_access,	FILE_OPEN, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
		return -errno;
	}

	rdpfs_query_information(completion_id, disk->device_id, FileStandardInformation,path);
	rdpfs_wait_reply(completion_id);

	fs = &rdpfs_response[completion_id].fs_inf;

	rdpfs_response[completion_id].buffer = buf;
	while (size_to_read != 0)
	{
		chunk_size_to_read = size_to_read > MAX_SIZE ? MAX_SIZE : size_to_read;

		rdpfs_request_read(completion_id, disk->device_id, chunk_size_to_read, current_offset);
		rdpfs_wait_reply(completion_id);

		size_read = rdpfs_response[completion_id].buffer_length;
		current_offset += size_read;
		rdpfs_response[completion_id].buffer += size_read;
		size_to_read -= size_read;
		if (size_read != chunk_size_to_read)
		{
			//file end
			break;
		}
	}

	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);

	return size - size_to_read;
}

/************************************************************************/
static int disk_dev_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_write]: "
			" Path to write : %s", path);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_write]: "
			" Size to write : %i", size);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_write]: "
			" Write from : %i", offset);

	int completion_id = 0;
	struct disk_device* disk;
	int attributes = 0;
	int desired_access = 0;
	int shared_access = 0;
	char* rdp_path;
	struct fs_info* fs;
	int file_size;
	int current_size = 0;
	int current_offset = offset;
	int size_write = 1;
	int chunk_size_to_write;
	int size_to_write = size;

	disk = rdpfs_get_device_from_path(path);
	if (disk == NULL)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_write]:"
					"Unable to get device from path : %s", path);
		return -errno;
	}

	rdp_path = g_strdup(path);
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");


	attributes = FILE_SYNCHRONOUS_IO_NONALERT;
	desired_access = GENERIC_WRITE|FILE_EXECUTE_ATTRIBUTES;
	shared_access = FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE;
	completion_id = rdpfs_create(disk->device_id, desired_access , shared_access,	FILE_OPEN, attributes, rdp_path);
	rdpfs_wait_reply(completion_id);

	if( rdpfs_response[completion_id].request_status != 0 )
	{
		return -errno;
	}

	rdpfs_query_information(completion_id, disk->device_id, FileStandardInformation,path);
	rdpfs_wait_reply(completion_id);

	fs = &rdpfs_response[completion_id].fs_inf;


	rdpfs_response[completion_id].buffer = (unsigned char*)buf;
	rdpfs_response[completion_id].buffer_length = size;


	while (size_to_write != 0)
	{
		chunk_size_to_write = size_to_write > MAX_SIZE ? MAX_SIZE : size_to_write;

		rdpfs_request_write(completion_id, current_offset, chunk_size_to_write);
		rdpfs_wait_reply(completion_id);

		size_write = rdpfs_response[completion_id].buffer_length;
		current_offset += size_write;
		rdpfs_response[completion_id].buffer += size_write;
		size_to_write -= size_write;
	}

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_write]:"
				"try to write %i -> really write : %i", size, rdpfs_response[completion_id].buffer_length);

	rdpfs_request_close(completion_id, disk->device_id);
	rdpfs_wait_reply(completion_id);

	return rdpfs_response[completion_id].buffer_length;
}

/************************************************************************/
static int disk_dev_statfs(const char *path, struct statvfs *stbuf)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_statfs");
	return 0;
}

/************************************************************************/
static int disk_dev_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_release");
	(void) path;
	(void) fi;
	return 0;
}


/************************************************************************/
static void* disk_dev_init()
{
	void *ret;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_init]: ");
	if (pthread_create (&vchannel_thread, NULL, thread_vchannel_process, (void*)0) < 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[disk_dev_init]: "
				"Pthread_create error for thread : vchannel_thread");
		return NULL;
	}
	disk_up = 1;
	return NULL;
}


/************************************************************************/
static void disk_dev_destroy(void *private_data)
{
	(void)private_data;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_destroy]");
	pthread_cancel(vchannel_thread);
	pthread_join(vchannel_thread, NULL);
	g_exit(0);

}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
/************************************************************************/
static int disk_dev_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

/************************************************************************/
static int disk_dev_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

/************************************************************************/
static int disk_dev_listxattr(const char *path, char *list, size_t size)
{
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

/************************************************************************/
static int disk_dev_removexattr(const char *path, const char *name)
{
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */


/*
 * End of fuse operation
 */


static struct fuse_operations disk_dev_oper = {
	.getattr	= disk_dev_getattr,
	.access		= disk_dev_access,
	.readlink	= NULL,
	.readdir	= disk_dev_readdir,
	.mknod		= disk_dev_mknod,
	.mkdir		= disk_dev_mkdir,
	.symlink	= NULL,
	.unlink		= disk_dev_unlink,
	.rmdir		= disk_dev_rmdir,
	.rename		= disk_dev_rename,
	.link			= NULL,
	.chmod		= disk_dev_chmod,
	.chown		= disk_dev_chown,
	.truncate	= disk_dev_truncate,
	.utimens	= disk_dev_utimens,
	.read			= disk_dev_read,
	.write		= disk_dev_write,
	.statfs		= disk_dev_statfs,
	.release	= disk_dev_release,
	.fsync		= NULL,
	.init			= disk_dev_init,
	.destroy	= disk_dev_destroy,
#ifdef HAVE_SETXATTR
	.setxattr	= disk_dev_setxattr,
	.getxattr	= disk_dev_getxattr,
	.listxattr	= disk_dev_listxattr,
	.removexattr	= disk_dev_removexattr,
#endif
};

int DEFAULT_CC
fuse_run()
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	g_mkdir(mount_point);
	if( g_directory_exist(mount_point) == 0)
	{
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[fuse_run]: "
				"Unable to initialize the mount point : %s", mount_point);
	}

	umask(0);
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[main]: "
			"Configuration of fuse");
	fuse_opt_add_arg(&args, "");
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[main]: "
			"Setup of the main mount point: %s", mount_point);
  fuse_opt_add_arg(&args, mount_point);

	ret = fuse_main(args.argc, args.argv, &disk_dev_oper, NULL);
	fuse_opt_free_args(&args);
}

