/*
 * xrdp_vchannel.h
 *
 *  Created on: 8 févr. 2013
 *      Author: david
 */

#ifndef XRDP_VCHANNEL_H_
#define XRDP_VCHANNEL_H_


#include <vchannel/lib/chansrv.h>
#include "xrdp_mm.h"



vchannel* APP_CC
xrdp_vchannel_create();

bool APP_CC
xrdp_vchannel_setup(struct xrdp_mm* self, vchannel* vc);




#endif /* XRDP_VCHANNEL_H_ */
