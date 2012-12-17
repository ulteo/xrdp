#include <stdlib.h>
#include <sys/eventfd.h>
#include <time.h>

#include "ulteo.h"

void ThrowWandException(struct ulteo* v, MagickWand *wand) 
{ 
  char *description; 
  ExceptionType severity; 
  char text[255]; 

  description=MagickGetException(wand,&severity);
  g_sprintf(text,"%s %s %lu %s\n",GetMagickModule(),description);
  v->server_msg(v, text, 1);
  description=(char *) MagickRelinquishMemory(description);
  exit(-1);
}

/******************************************************************************/
int DEFAULT_CC
lib_mod_event(struct ulteo* v, int msg, long param1, long param2,
              long param3, long param4)
{
  LIB_DEBUG(v, "lib_mod_event");
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
lib_mod_signal(struct ulteo* v)
{
  LIB_DEBUG(v, "lib_mod_signal");
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
lib_mod_start(struct ulteo* v, int w, int h, int bpp)
{
  char text[256];
  
  v->server_begin_update(v);
  v->server_set_fgcolor(v, 0x00AABBCC);
  v->server_fill_rect(v, 0, 0, w, h);
  v->server_end_update(v);
  v->server_width = w;
  v->server_height = h;
  v->server_bpp = bpp;
  
  g_sprintf(text, "Starting ulteo %dx%d in %d bpp", w, h, bpp);
  v->server_msg(v, text, 1);
  
  PixelIterator *iterator = NewPixelIterator(v->image_wand);
  PixelWand **pixels;
  MagickPixelPacket pixel;
  size_t y, x, width, height;
  
  int Bpp = 4;
  width = MagickGetImageWidth(v->image_wand);
  height = MagickGetImageHeight(v->image_wand);
  
  v->image = g_malloc(width * height * Bpp, 0);
  
  MagickGetImagePixels(v->image_wand, 0, 0, width, height, "BGRA", CharPixel, v->image);

  LIB_DEBUG(v, "lib_mod_start");
  return 0;
}

/******************************************************************************/
/*
  return error
*/
int DEFAULT_CC
lib_mod_connect(struct ulteo* v)
{
  LIB_DEBUG(v, "lib_mod_connect");
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
lib_mod_end(struct ulteo* v)
{
  if (v->ulteo_desktop != 0)
  {
  }
  g_free(v->clip_data);
  v->clip_data = 0;
  v->clip_data_size = 0;
  LIB_DEBUG(v, "lib_mod_end");
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
lib_mod_set_param(struct ulteo* v, char* name, char* value)
{
  LIB_DEBUG(v, "lib_mod_set_param");
  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_get_wait_objs(struct ulteo* v, tbus* read_objs, int* rcount,
                      tbus* write_objs, int* wcount, int* timeout)
{
  int i;
  
  LIB_DEBUG(v, "lib_mod_get_wait_objs");
  
  i = *rcount;
  if (v != 0)
  {
    if (v->efd != 0)
    {
      read_objs[i++] = v->efd;
    }
  }
  *rcount = i;

  return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_check_wait_objs(struct ulteo* v)
{
  char buf[8];
  int x, y, w, h;
  
  LIB_DEBUG(v, "lib_mod_check_wait_objs");

  read(v->efd, buf, sizeof(buf));
  
  if (buf[0]>0) 
  {
    v->server_begin_update(v);
    v->server_set_fgcolor(v, random());

    x = random() % (v->server_width);
    y = random() % (v->server_height);
    w = random() % v->server_width;
    h = random() % v->server_height;

    //v->server_fill_rect(v, x, y, w, h);
    
    w = MagickGetImageWidth(v->image_wand);
    h = MagickGetImageHeight(v->image_wand);
    
    v->server_paint_rect(v, x, y, w, h, v->image, w, h, 0, 0);
    
    v->server_end_update(v);
  }

  return 0;
}

void *lib_thread_run(void *arg)
{
  struct ulteo* v = arg;
  struct timespec tim, tim2;
  char buf[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
  
  while (! v->terminate) 
  {
    tim.tv_sec = 0;
    tim.tv_nsec = 50000000L;

    if (nanosleep(&tim , &tim2) < 0 )
      break;

    write(v->efd, buf, sizeof(buf));
  }
}

/******************************************************************************/
struct ulteo* EXPORT_CC
mod_init(void)
{
  struct ulteo* v;

  v = (struct ulteo*)g_malloc(sizeof(struct ulteo), 1);
  v->size = sizeof(struct ulteo);
  v->version = CURRENT_MOD_VER;
  v->handle = (long)v;
  v->mod_connect = lib_mod_connect;
  v->mod_start = lib_mod_start;
  v->mod_event = lib_mod_event;
  v->mod_signal = lib_mod_signal;
  v->mod_end = lib_mod_end;
  v->mod_set_param = lib_mod_set_param;
  v->mod_get_wait_objs = lib_mod_get_wait_objs;
  v->mod_check_wait_objs = lib_mod_check_wait_objs;
  v->efd = eventfd(1, 0);
  v->terminate = 0;
  pthread_create(&v->thread, NULL, lib_thread_run, v);
  
  MagickBooleanType status;

  MagickWandGenesis(); 
  v->image_wand = NewMagickWand(); 
  status = MagickReadImage(v->image_wand, XRDP_SHARE_PATH "/ad24b.bmp");
  if (status == MagickFalse) 
    ThrowWandException(v, v->image_wand);
  
  return v;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(struct ulteo* v)
{
  LIB_DEBUG(v, "mod_exit");
  if (v == 0)
  {
    return 0;
  }
  v->terminate = 1;
  pthread_join(v->thread, NULL);
  close(v->efd);
  g_free(v);
  return 0;
}
