/*
 @file ndofdev_osx.c 
 @brief Mac OS X functions implementation.
 
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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <CoreServices/CoreServices.h>
#include "ndofdev_external.h"
#include "ndofdev_internal.h"
#include "ndofdev_internal_osx.h"

#define _REENTRANT 

/* -------------------------------------------------------------------------- */
#pragma mark * Static variables *

static CFRunLoopRef					s_runloop = NULL;
static NDOF_DeviceAddCallback		s_add_callback;
static NDOF_DeviceRemovalCallback	s_removal_callback;
static dispatch_semaphore_t         s_init_sem = nil;
static dispatch_queue_t             s_hotplug_queue = nil;

/* -------------------------------------------------------------------------- */
#pragma mark * Function prototypes for local functions

static NDOF_Device *ndof_idsearch(long loc_id);
static OSStatus ndof_add_callback(hu_device_t *d);
static OSStatus ndof_removal_callback(hu_device_t *d);
static short ndof_isndof(hu_device_t *dev);
static void ndof_init(NDOF_Device *dev, hu_device_t *hiddev);
static Boolean ndof_equivalent(NDOF_Device *dev1, hu_device_t *hiddev2);

#pragma mark * Function implementations *

/* -------------------------------------------------------------------------- */
static short ndof_isndof(hu_device_t *dev)
{   
    /* see IOKit/hid/IOHIDUsageTables.h */
    if (dev && 
        (dev->axis >= 3
         || (dev->usagePage == kHIDPage_GenericDesktop 
             && (dev->usage == kHIDUsage_GD_MultiAxisController
                 || dev->usage == kHIDUsage_GD_GamePad
                 || dev->usage == kHIDUsage_GD_Joystick))
         || (dev->usagePage == kHIDPage_Game
             && (dev->usage == kHIDUsage_Game_3DGameController))
         || strstr(dev->manufacturer, "3Dconnexion")))
    {
        return 1;
    }
    
    return 0;
}

/* -------------------------------------------------------------------------- */
static NDOF_Device *ndof_idsearch(long loc_id)
{
	NDOF_DeviceListNode *node = g_ndof_list_head;
	while (node)
    {
        if (node->dev && node->dev->private_data &&
           ((NDOF_DevicePrivate*)node->dev->private_data)->dev &&
		   ((NDOF_DevicePrivate*)node->dev->private_data)->dev->locID == loc_id)
        {
            break;
        }
        else 
        {
            node = node->next;
        }
	}
    
	if (node)
		return node->dev;
	else
		return NULL;
}

/* -------------------------------------------------------------------------- */
static Boolean ndof_equivalent(NDOF_Device *dev1, hu_device_t *hiddev2)
{
    if (dev1 && dev1->private_data)
    {
        hu_device_t *hiddev1 = ((NDOF_DevicePrivate*)dev1->private_data)->dev;
        return (hiddev1 && hiddev2 && hiddev1->vendorID == hiddev2->vendorID 
                && hiddev1->productID == hiddev2->productID);
    }
    
    return FALSE;
}

/* -------------------------------------------------------------------------- */
static void ndof_init(NDOF_Device *dev, hu_device_t *hiddev)
{
    NDOF_DevicePrivate *priv;
    hu_element_t *elem = NULL;
    long axes_cnt = 0, btn_cnt = 0;
    size_t lenm, lenp;
    
    lenm = strlen(hiddev->manufacturer);
    lenp = strlen(hiddev->product);
    lenm = (lenm < 255 ? lenm : 255); // prevent buffer overflow
    lenp = (lenp < 255 ? lenp : 255);
    strncpy(dev->manufacturer, hiddev->manufacturer, lenm + 1);
    strncpy(dev->product, hiddev->product, lenp + 1);
    priv = (NDOF_DevicePrivate*) dev->private_data;
    priv->dev = hiddev;
    priv->curr_loc_id = hiddev->locID;
    priv->curr_vendor_id = hiddev->vendorID;
    priv->curr_product_id = hiddev->productID;
        
    // save all elements info relative to axes and buttons into the
    // 'priv' space (to avoid searching them each time we need them)
    elem = HIDGetFirstDeviceElement(hiddev, kHIDElementTypeIO);
    while (elem && (axes_cnt < NDOF_MAX_AXES_COUNT 
                    || btn_cnt < NDOF_MAX_BUTTONS_COUNT))
    {
        // explore each element of the current device 'hiddev', 
        // and determine if it's an element we can use
        if (axes_cnt < NDOF_MAX_AXES_COUNT      // set minimal
            && elem->size > 4                   // conditions for
            && elem->max - elem->min > 15       // all usable axes
            && (elem->type == kIOHIDElementTypeInput_Axis
                || (elem->type == kIOHIDElementTypeInput_Misc
                    && elem->size >= 8)
                || (elem->usagePage == kHIDPage_GenericDesktop
                    && (elem->usage == kHIDUsage_GD_X
                        || elem->usage == kHIDUsage_GD_Y
                        || elem->usage == kHIDUsage_GD_Z
                        || elem->usage == kHIDUsage_GD_Rx
                        || elem->usage == kHIDUsage_GD_Ry
                        || elem->usage == kHIDUsage_GD_Rz))))
        {
            priv->hid_axes[axes_cnt] = elem;
            
            /*  y_min = offset + scale*x_min; 
            y_max = offset + scale*x_max */
            priv->scale[axes_cnt] = (float) 
                (dev->axes_max - dev->axes_min) / 
                (elem->max - elem->min);
            priv->offset[axes_cnt] = 
                dev->axes_min - priv->scale[axes_cnt] * elem->min;
            
            axes_cnt++;
        } 
        else if (elem->type == kIOHIDElementTypeInput_Button
                 && btn_cnt < NDOF_MAX_BUTTONS_COUNT)
        {
            priv->hid_btn[btn_cnt++] = elem;
        }
        
        elem = HIDGetNextDeviceElement(elem, kHIDElementTypeAll);
    }
    
    dev->axes_count = axes_cnt;
    dev->btn_count  = btn_cnt;
    dev->valid = 1;
}

/* -------------------------------------------------------------------------- 
    In this implementation we originally wanted to allow passing in a partially
	initialized structure to be used as a constraint set for matching
	devices found on the USB bus. It actually works here, but it's
	unlikely it will be developed on other platforms.
*/
int ndof_init_first(NDOF_Device *dev, void *param)
{
    int notfound = -1;
    hu_device_t *d = HIDGetFirstDevice();
    
    while (d)
    {
        if (ndof_isndof(d) && 
            ((dev->axes_count == 0				// axes_count is uninitialized
			  || dev->axes_count == d->axis)	// ... or matches the curr dev
             && (dev->btn_count == -1			// btn count is uninitialized
				 || dev->btn_count == d->buttons)))	// ... or matches curr dev
        {
            size_t lenm = strlen(d->manufacturer);
            size_t lenp = strlen(d->product);
            
            if ((*dev->manufacturer == '\0'
                 || strncmp(dev->manufacturer, d->manufacturer, lenm) == 0)
                && (*dev->product == '\0'
                    || strncmp(dev->product, d->product, lenp) == 0))
            {
                ndof_init(dev, d);
                notfound = 0;
                break; // we exit at the first match we find
            }
        }
        
        d = HIDGetNextDevice(d);
    }
	
    if (notfound)
	{
        fprintf(stderr, "libndofdev: no NDOF HID device found.\n");
    }
	else
	{
        fprintf(stderr, "libndofdev: using %s HID device:\n",
                (HIDIsValidDevice(((NDOF_DevicePrivate*)dev->private_data)->dev) ?
                 "valid" : "invalid"));
		ndof_dump(stderr, dev);
	}
    
    return notfound;
}

/* -------------------------------------------------------------------------- */
void ndof_update(NDOF_Device *in_dev)
{
    int i;
    static Boolean log_error_flag = TRUE; 
    NDOF_DevicePrivate *priv = (NDOF_DevicePrivate*) in_dev->private_data;
    assert(priv);
    
    if (priv->dev == NULL)
    {
		fprintf(stderr, "libndofdev: unable to read input " \
                    "(NULL hu_device_t pointer)\n");
        return; // attempting to read status from uninitialized structure
    }
    
    if (in_dev->valid == 0)
    {
        if (log_error_flag)
            fprintf(stderr, "libndofdev: unable to read input (invalid structure)\n");
        log_error_flag = FALSE;
        return;
    }
    
    log_error_flag = TRUE;
    
    for (i = 0; i < in_dev->axes_count; i++)
    {
        /* get raw values but scale them accordingly to user settings */
        in_dev->axes[i] = priv->offset[i] + 
			priv->scale[i] * HIDGetElementValue(priv->dev, priv->hid_axes[i]);
    }
    
    for (i = 0; i < in_dev->btn_count; i++)
    {
        in_dev->buttons[i] = HIDGetElementValue(priv->dev, priv->hid_btn[i]);
    }
    
#ifdef NDOF_DEBUG
    if (in_dev->axes[0] || in_dev->axes[1] || in_dev->axes[2] 
        || in_dev->axes[3] || in_dev->axes[4] || in_dev->axes[5]
        || in_dev->buttons[0] || in_dev->buttons[1])
    {
        fprintf(NDOF_DEBUG, "ndof_update(): [%6ld %6ld %6ld %6ld %6ld %6ld] " \
                "[%4ld %4ld]\n",
                in_dev->axes[0], in_dev->axes[1], in_dev->axes[2], 
                in_dev->axes[3], in_dev->axes[4], in_dev->axes[5],
                in_dev->buttons[0], in_dev->buttons[1]);
    }
#endif    
}

/* -------------------------------------------------------------------------- */
int ndof_devcount()
{
    int num = 0;
    hu_device_t *dev = HIDGetFirstDevice();
    
    while (dev)
    {
        if (ndof_isndof(dev))
            num++;
        
        dev = HIDGetNextDevice(dev);
    }
    
    return num;
}

/* -------------------------------------------------------------------------- */
unsigned char ndof_match_private(NDOF_DevicePrivate *dev1, 
								 NDOF_DevicePrivate *dev2)
{
    return (dev1 && dev2 && dev1->curr_loc_id == dev2->curr_loc_id 
            && dev1->curr_product_id == dev2->curr_product_id
            && dev1->curr_vendor_id == dev2->curr_product_id);
}

#pragma mark * Library initialization and cleanup *

/* -------------------------------------------------------------------------- */
int ndof_libinit(NDOF_DeviceAddCallback in_add_cb, 
				 NDOF_DeviceRemovalCallback in_removal_cb,
				 void *param)
{
	fprintf(stderr, "libndofdev: initializing...\n");

    s_add_callback = in_add_cb;
	s_removal_callback = in_removal_cb;
    
    // setup dispatch serial queue
    s_init_sem = dispatch_semaphore_create(1);
    s_hotplug_queue = dispatch_queue_create("hotplug", DISPATCH_QUEUE_SERIAL);

    // initialize the hotplug loop
    dispatch_async(s_hotplug_queue, ^{
        HIDSetHotPlugCallback(ndof_add_callback, ndof_removal_callback);
        HIDBuildDeviceList(0, 0);
        s_runloop = CFRunLoopGetCurrent();
        
        /* signal father that we are about to start the runloop */
        fprintf(stderr, "libndofdev: starting runloop...\n");
        dispatch_semaphore_signal(s_init_sem);
        
        /* Start the run loop. Now we'll receive hotplugging notifications. */
        CFRunLoopRun();
        
        fprintf(stderr, "libndofdev: runloop terminated, destroying HID data...\n");
        HIDReleaseDeviceList();
    });
    
    // we block until the other thread has completed initialization
    dispatch_semaphore_wait(s_init_sem, kDurationMillisecond * 60000);
    
    // be extra cautious, make sure thread gets into runloop before we return
    usleep(10000);
    
    return 0;
}

/* -------------------------------------------------------------------------- */
void ndof_cleanup_internal()
{
	if (s_runloop)
	{
		fprintf(stderr, "libndofdev: stopping runloop\n");
		CFRunLoopStop(s_runloop);
	}
    
    dispatch_release(s_init_sem);
}

/* -------------------------------------------------------------------------- */
void ndof_dev_private_dispose(NDOF_DevicePrivate *priv)
{
    if (priv) {
        free(priv);
    }
}

#pragma mark * Hot-plugging *

/*  **********************
    Hot-plugging use cases
    **********************

    1- Huser removes device because he doesn't want to use it anymore
    ==> simply disable it, do not destroy NDOF_Device struct
    2- user removes device, connects a new one to the same port and wants to use that instead
    ==> disable it first, then reinit NDOF_Device struct
    3- user removes device and reconnect the same device to a different port
    ==> disable it first, then refresh NDOF_Device struct
    4- user connects new device and would like to use that 
    ==> notify client of new NDOF_Device, client can decide to keep it or not
*/

/* -------------------------------------------------------------------------- 
	Purpose:    Callback invoked by HiD Utils when a new device is plugged in
    Inputs:     in_dev - device that was just plugged in
    Notes:      Covers Hot Plug Use Cases #2 and #3 (see above)
    Returns:    0 if ok.
*/
static OSStatus ndof_add_callback(hu_device_t *in_dev)
{
    NDOF_DeviceListNode *node;
    Boolean found = FALSE;
    
	fprintf(stderr, "libndofdev: hot-plugged device:\n");
    
    // Note: since we handle removal notifications, the whole HID Utilities
    // device list should be okay. No need to refresh it.
	
    /* let's see if we were already using the same device (Use Case #2) */
    node = g_ndof_list_head;
	while (node)
	{
        if (node->dev
            && !node->dev->valid /* nothing to change for a valid device */
            && ndof_equivalent(node->dev, in_dev)) 
        {
            found = TRUE;
            break;
        }
        else
        {
            node = node->next;
        }
    }

    if (!found)
    {
        /* let's see if we were usinga device at the same port (Use Case #3) */
        node = g_ndof_list_head;
        while (node)
        {
            if (node->dev
                && !node->dev->valid /* nothing to change for a valid device */
                && ((NDOF_DevicePrivate*)node->dev->private_data)->curr_loc_id 
                    == in_dev->locID)
            {
                found = TRUE;
                break;
            }
            else
            {
                node = node->next;
            }
        }
    }

    if (found)
    {
        ndof_init(node->dev, in_dev);
        if (s_add_callback
            && s_add_callback(node->dev) == NDOF_DISCARD_HOTPLUGGED)
        {
            ndof_destroy(node->dev);
        }
    }
    else
    {
        /* (Use Case #4) */
        if (s_add_callback && ndof_isndof(in_dev))
        {
            NDOF_Device *new_device;
            
            /* create new device ... */
            new_device = ndof_create();
            ndof_init(new_device, in_dev);
            
            /* ...get client interest in new device and eventually clip it */
            if (s_add_callback(new_device) == NDOF_DISCARD_HOTPLUGGED)
                ndof_destroy(new_device);
        }
    }

#ifdef NDOF_DEBUG
    ndof_dump_list(NDOF_DEBUG);
#if 0
	hu_device_t *d = HIDGetFirstDevice();
	fprintf(NDOF_DEBUG, "libndofdev: new device list:\n");
	while (d)
	{
		fprintf( NDOF_DEBUG, "Device = {m: \"%s\" p: \"%s\", vid: %ld, pid: %ld, " \
				"loc: %08lX, axes:  %ld, usage: %4.4lX:%4.4lX}.\n",
				d->manufacturer, d->product, d->vendorID, d->productID,
				d->locID, d->axis, d->usagePage, d->usage);
		fflush( NDOF_DEBUG );
		
		d = HIDGetNextDevice(d);
	}
#endif
#endif
    
    return 0;
}

/* -------------------------------------------------------------------------- 
	Purpose:    Callback invoked when a generic USB device is unplugged.
    Inputs:     removed_dev - device that was just removed
    Notes:      Covers Hot Plug Use Case #1. (See above.)
    Returns:    0 if ok.
*/
static OSStatus ndof_removal_callback(hu_device_t *removed_dev)
{
    NDOF_Device *ndof_dev;
    
	fprintf(stderr, "libndofdev: removed device:\n");
    
    /* verify it's actually a device we care about */
    ndof_dev = ndof_idsearch(removed_dev->locID);
    if (ndof_dev)
    {
        ndof_dev->valid = 0;
        if (s_removal_callback)
            s_removal_callback(ndof_dev);
    }
        
	return 0;
}
