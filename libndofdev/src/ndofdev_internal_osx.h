/*
 @file ndofdev_internal_osx.h
 @brief Declaration of functions only used internally.
 
 Copyright (c) 2007, 3Dconnexion, Inc. - All rights reserved.
 
 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:
 - Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution.
 - Neither the name of the <ORGANIZATION> nor the names of its contributors 
   may be used to endorse or promote products derived from this software 
   without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ndofhid_internal_osx_h__
#define __ndofhid_internal_osx_h__

#include <IOKit/usb/IOUSBLib.h>
#include "ndofdev_hidutils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Type equivalences between new and old version of HID_Utilities. */
typedef hu_device_t *pRecDevice;
typedef hu_element_t *pRecElement;
	
typedef struct NDOF_DevicePrivate {
    hu_device_t *dev;
    hu_element_t *hid_axes[NDOF_MAX_AXES_COUNT];
    hu_element_t *hid_btn[NDOF_MAX_BUTTONS_COUNT];
	float scale[NDOF_MAX_AXES_COUNT];
	float offset[NDOF_MAX_AXES_COUNT];
    long curr_loc_id; /* identifies where the device is currently connected */
    long curr_vendor_id;
    long curr_product_id;
} NDOF_DevicePrivate;

void ndof_cleanup_internal();

/** Makes sure everything related to `priv' is tidily disposed. */
void ndof_dev_private_dispose(NDOF_DevicePrivate *priv);

/** Returns 1 if d1 and d2 are the same device at the same port. */
unsigned char ndof_match_private(NDOF_DevicePrivate *d1, NDOF_DevicePrivate *d2);

#ifdef __cplusplus
}
#endif
	
#endif /* __ndofhid_internal_osx_h__ */
