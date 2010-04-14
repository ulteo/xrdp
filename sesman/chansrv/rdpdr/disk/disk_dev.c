/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` fusexmp.c -o fusexmp
*/

#define FUSE_USE_VERSION 26



#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif


#include "disk_dev.h"
#include "rdpfs.h"

static struct disk_device disk_devices[128];
static int disk_devices_count = 0;
extern pthread_t vchannel_thread;
extern void *thread_vchannel_process (void * arg);
extern struct log_config *l_config;
extern char mount_point[256];
extern int rdpdr_sock;
extern int disk_up;

/*****************************************************************************/
int APP_CC
disk_dev_in_unistr(struct stream* s, char *string, int str_size, int in_len)
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
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[disk_dev_in_unistr]: "
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
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[disk_dev_in_unistr]: "
							"Server sent an unexpectedly long string, truncating");
      }
      else
      {
        iconv_close(iconv_h);
        iconv_h = (iconv_t) - 1;
        log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[disk_dev_in_unistr]: "
							"Iconv fail, errno %d", errno);
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
      log_message(l_config, LOG_LEVEL_WARNING, "rdpdr_disk[disk_dev_in_unistr]: "
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

/************************************************************************/
struct disk_device* APP_CC
disk_dev_get_dir(int device_id)
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
disk_dev_get_device_from_path(const char* path)
{
	//extract device name
	char *pos;
	int count;
	char device_name[256];
	int i;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_get_device_from_path]: "
				"The path is: %s", path);

	pos = strchr(path+1, '/');
	if(pos == NULL)
	{
		count = strlen(path);
	}
	else
	{
		count = pos-path;
	}
	strncpy(device_name, path+1, count);
	device_name[count+1] = 0;

	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_get_device_from_path]: "
				"The drive is: %s", device_name);

	for (i=0 ; i< disk_devices_count ; i++)
	{
		if(strcmp(device_name, disk_devices[i].dir_name) == 0)
		{
			return &disk_devices[i];
		}
	}
	return 0;
}




/************************************************************************/
int APP_CC
disk_dev_add(struct stream* s, int device_data_length,
								int device_id, char* dos_name)
{


	disk_devices[disk_devices_count].device_id = device_id;
	g_strcpy(disk_devices[disk_devices_count].dir_name, dos_name);
	disk_devices_count++;
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_add]: "
				"Succedd to add disk");
  return g_time1();
}


/************************************************************************
	Begin of fuse operation
 */

/************************************************************************/
static int disk_dev_getattr(const char *path, struct stat *stbuf)
{
	struct disk_device *disk;
	char* rdp_path;

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
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = disk_devices_count + 2;
		return 0;
	}

	disk = disk_dev_get_device_from_path(rdp_path);
	if ( disk == 0)
	{
		log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_getattr]: "
				"Device from path(%s) did not exists", rdp_path);
		return 0;
	}
	g_str_replace_first(rdp_path, disk->dir_name, "");

	/* test volume */
	if (strcmp(rdp_path, "/") == 0)
	{
		int i;
		int completion_id = 0;

		completion_id = rdpfs_create(disk->device_id, GENERIC_READ|FILE_EXECUTE_ATTRIBUTES,	FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,	FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, "");
		rdpfs_wait_reply();
		rdpfs_query_volume_information(completion_id, disk->device_id, FileFsVolumeInformation, "");
		rdpfs_wait_reply();
		rdpfs_query_information(completion_id, disk->device_id, FileBasicInformation,"");
		rdpfs_wait_reply();
		rdpfs_query_information(completion_id, disk->device_id, FileStandardInformation,"");
		rdpfs_wait_reply();
		rdpfs_request_close(completion_id, disk->device_id);
		rdpfs_wait_reply();
	}
	/* test path */
	stbuf->st_mode = S_IFDIR | 0755;
	stbuf->st_nlink = disk_devices_count + 2;
	return 0;

}

/************************************************************************/
static int disk_dev_access(const char *path, int mask)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_access]: "
				"access on %s", path);
	int res;

	res = access(mount_point, mask);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_readlink(const char *path, char *buf, size_t size)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_readlink");
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
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
	char* rdp_path;


	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
				"readdir on %s", path);

	if (strcmp(path, "/") == 0)
	{
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		for (i=0 ; i<disk_devices_count ; i++)
		{
			disk = &disk_devices[i];
			filler(buf, disk->dir_name , NULL, 0);
		}
		return 0;
	}

	/*disk = disk_dev_get_device_from_path(path);
	if(disk == 0)
	{
		return 0;
	}
	g_sprintf(rdp_path, "%s/*", path, "/*");
	g_str_replace_first(rdp_path, disk->dir_name, "");
	g_str_replace_first(rdp_path, "//", "/");
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_readdir]: "
				"Request rdp path %s", rdp_path );
	if(rdp_path == NULL)
	{
		rdpfs_readdir(disk->device_id, "/*");
	}
	else
	{
		rdpfs_readdir(disk->device_id, rdp_path);
	}
*/
	return -errno;
}

/************************************************************************/
static int disk_dev_mknod(const char *path, mode_t mode, dev_t rdev)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_mknod");
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_mkdir(const char *path, mode_t mode)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_mkdir");
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_unlink(const char *path)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_unlink");
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_rmdir(const char *path)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_rmdir");
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_symlink(const char *from, const char *to)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_symlink");
	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_rename(const char *from, const char *to)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_rename");
	int res;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_link(const char *from, const char *to)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_link");
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_chmod(const char *path, mode_t mode)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_chmod");
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_chown(const char *path, uid_t uid, gid_t gid)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_chown");
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_truncate(const char *path, off_t size)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_truncate");
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_utimens(const char *path, const struct timespec ts[2])
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_utimens");
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

/************************************************************************/
static int disk_dev_open(const char *path, struct fuse_file_info *fi)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_open");
	int res;

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

/************************************************************************/
static int disk_dev_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_read");
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

/************************************************************************/
static int disk_dev_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_write");
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

/************************************************************************/
static int disk_dev_statfs(const char *path, struct statvfs *stbuf)
{
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_statfs");
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

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
static int disk_dev_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
	log_message(l_config, LOG_LEVEL_DEBUG, "disk_dev_fsync");
	(void) path;
	(void) isdatasync;
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
	log_message(l_config, LOG_LEVEL_DEBUG, "rdpdr_disk[disk_dev_destroy]: ");
	pthread_cancel(vchannel_thread);
	pthread_join(vchannel_thread, NULL);

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
	.readlink	= disk_dev_readlink,
	.readdir	= disk_dev_readdir,
	.mknod		= disk_dev_mknod,
	.mkdir		= disk_dev_mkdir,
	.symlink	= disk_dev_symlink,
	.unlink		= disk_dev_unlink,
	.rmdir		= disk_dev_rmdir,
	.rename		= disk_dev_rename,
	.link			= disk_dev_link,
	.chmod		= disk_dev_chmod,
	.chown		= disk_dev_chown,
	.truncate	= disk_dev_truncate,
	.utimens	= disk_dev_utimens,
	.open			= disk_dev_open,
	.read			= disk_dev_read,
	.write		= disk_dev_write,
	.statfs		= disk_dev_statfs,
	.release	= disk_dev_release,
	.fsync		= disk_dev_fsync,
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
		log_message(l_config, LOG_LEVEL_ERROR, "rdpdr_disk[main]: "
				"Unable to initialize the mount point");
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

