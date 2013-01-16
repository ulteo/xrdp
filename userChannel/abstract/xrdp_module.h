/*
 * xrdp_module.h
 *
 *  Created on: 16 janv. 2013
 *      Author: david
 */

#ifndef XRDP_MODULE_H_
#define XRDP_MODULE_H_

struct xrdp_user_channel;

typedef bool (*init_func_pointer)(struct xrdp_user_channel*);
typedef int (*exit_func_pointer)(struct xrdp_user_channel*);
typedef int (*get_data_descriptor_func_pointer)(struct xrdp_user_channel*, tbus* robjs, int* rc, tbus* wobjs, int* wc, int* timeout);
typedef int (*get_data_func_pointer)(struct xrdp_user_channel*);
typedef int (*disconnect_func_pointer)(struct xrdp_user_channel*);
typedef int (*end_funct_pointer)(struct xrdp_user_channel*);
typedef bool (*connect_func_pointer)(struct xrdp_user_channel* user_channel, int session_id, struct xrdp_session* session);
typedef int (*callback_funct_pointer)(long id, int msg, long param1, long param2, long param3, long param4);
typedef int (*is_term_func_pointer)();


struct xrdp_user_channel
{
  tbus handle;
  tbus wm;
  tbus self_term_event;

  init_func_pointer init;
  exit_func_pointer exit;

  // module functions
  get_data_descriptor_func_pointer get_data_descriptor;
  get_data_func_pointer get_data;
  connect_func_pointer connect;
  disconnect_func_pointer disconnect;
  callback_funct_pointer callback;
  end_funct_pointer end;

  // xrdp function
  is_term_func_pointer is_term;
};


#endif /* XRDP_MODULE_H_ */
