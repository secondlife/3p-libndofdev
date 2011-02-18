/*
 @file ndofdev_linux.c
 @brief UNMAINTAINED Linux implementation
 Created by Ettore Pasquini on 8/7/07.
 
 Copyright (c) 2007, 3Dconnexion, Inc. - All rights reserved.
 
 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:
 - Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution.
 - Neither the name of the 3Dconnexion, Inc. nor the names of its contributors 
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
#include "ndofhid_external.h"
#include "ndofhid_internal_linux.h"

typedef enum ndof_vendor_id {
    kNdof3Dconnexion = 0x046d, //1133
} ndof_vendor_id;

typedef enum ndof_product_id {
    kNdofSpaceNavigator = 0xc626, //50726,
} ndof_product_id;


/* -------------------------------------------------------------------------- */
int ndof_libinit()
{
    #if NDOF_DEBUG
    HIDDebugLevel debug_level = HID_DEBUG_ERRORS;
    #else
    HIDDebugLevel debug_level = HID_DEBUG_ALL;
    #endif
    int err;
    
    /* see include/debug.h for possible values */
    hid_set_debug(debug_level);
    hid_set_debug_stream(stderr);
    /* passed directly to libusb */
    hid_set_usb_debug(0);

    err = hid_init();
    if (err != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_init failed with return code %d\n", err);
    }
    
    return err;
}

/* -------------------------------------------------------------------------- */
/*static short ndof_isndof(pRecDevice dev)
{
    if (dev && 
        (dev->axis > 2
         || strstr(dev->manufacturer, "3Dconnexion")))
    {
#if NDOF_DEBUG
        fprintf(stderr, "  NDOF device found: %s %s: #inputs=%ld; #axes=%ld; "
                "#buttons=%ld\n", dev->manufacturer, dev->product, 
                dev->inputs, dev->axis, dev->buttons);
#endif
        
        return 1;
    }
    
    return 0;
}*/

/* -------------------------------------------------------------------------- */
int ndof_dev_count()
{
    /*    
    int num = 0;
    pRecDevice dev = HIDGetFirstDevice();

    while (dev)
    {
        if (ndof_isndof(dev))
            num++;
        
        dev = HIDGetNextDevice(dev);
    }
    */
    return -1;
}

/* -------------------------------------------------------------------------- */
int ndof_firstdev(NDOF_Device *dev)
{
    int notfound = -1;
    HIDInterface *hid;
    hid_return ret;

    HIDInterfaceMatcher matcher = { kNdof3Dconnexion, kNdofSpaceNavigator, NULL, NULL, 0 };
    
    hid = hid_new_HIDInterface();
    if (hid == 0) {
        fprintf(stderr, "hid_new_HIDInterface() failed, out of memory?\n");
        return notfound;
    }

    ret = hid_force_open(hid, 0, &matcher, 3);
    //ret = hid_open(hid, 0, &matcher);
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_force_open failed with return code %d\n", ret);
        return notfound;
    }

    ret = hid_write_identification(stdout, hid);
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_write_identification failed with return code %d\n", ret);
        return notfound;
    }
  
    ret = hid_dump_tree(stdout, hid);
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_dump_tree failed with return code %d\n", ret);
        return notfound;
    }
    /*
    pRecDevice d = HIDGetFirstDevice();
    
    while (d)
    {
        if (ndof_isndof(d) && 
            ((dev->axes_count == 0 || dev->axes_count == d->axis)
             && (dev->btn_count == -1 || dev->btn_count == d->buttons)))
        {
            size_t lenm = strlen(d->manufacturer);
            size_t lenp = strlen(d->product);

            if ((*dev->manufacturer == '\0'
                 || strncmp(dev->manufacturer, d->manufacturer, lenm) == 0)
                && (*dev->product == '\0'
                    || strncmp(dev->product, d->product, lenp) == 0))
            {
                NDOF_DevicePrivate *priv;
                pRecElement elem = NULL;
                long axesCnt = 0, btnCnt = 0;
                #if NDOF_DEBUG
                int debug_miscCnt = 0;
                #endif
                
                dev->axes_count = d->axis;
                dev->btn_count  = d->buttons;
                strncpy(dev->manufacturer, d->manufacturer, lenm);
                strncpy(dev->product, d->product, lenp);
                priv = (NDOF_DevicePrivate*) dev->private_data;
                priv->dev = d;
                
                // now save all elements relative to the axes and buttons
                // to speed up future access (ndof_update_status)
                elem = HIDGetFirstDeviceElement(d, kHIDElementTypeIO);
                while (elem && !(axesCnt == d->axis && btnCnt == d->buttons))
                {
                    // know how many axes and buttons we are looking for,
                    // make sure this is respected
                    // to do: what about kIOHIDElementTypeInput_Axis elements?
                    if (elem->type == kIOHIDElementTypeInput_Misc
                        && axesCnt < d->axis 
                        && elem->size > 1 && (elem->max - elem->min > 2))
                        priv->axesElem[axesCnt++] = elem;
                    else if (elem->type == kIOHIDElementTypeInput_Button
                             && btnCnt < d->buttons)
                        priv->btnElem[btnCnt++] = elem;
                    
                    #if NDOF_DEBUG
                    if (elem->type == kIOHIDElementTypeInput_Misc
                        && elem->size > 1 && (elem->max - elem->min > 2)
                        )
                    {
                        debug_miscCnt++;
                        fprintf(stderr, "tmp_miscCnt=%d;\n", debug_miscCnt);
                    }
                    #endif
                    
                    elem = HIDGetNextDeviceElement(elem, kHIDElementTypeAll);
                }
                assert(axesCnt == d->axis);
                assert(btnCnt == d->buttons);
                #if NDOF_DEBUG
                assert(debug_miscCnt == d->axis);
                #endif
                
                notfound = 0;
                break;
            }
        }
        
        d = HIDGetNextDevice(d);
    }
    
    */
    
    return notfound;
}

/* -------------------------------------------------------------------------- */
void ndof_update_status(NDOF_Device *in_dev)
{
    /*
    int i;
    NDOF_DevicePrivate *priv = (NDOF_DevicePrivate*) in_dev->private_data;
    
    if (priv->dev == NULL)
        return; // attempting to read status from uninitialized structure
    
    for (i = 0; i < in_dev->axes_count; i++)
    {
        // get raw values
        in_dev->axes[i] = HIDGetElementValue(priv->dev, priv->axesElem[i]);
        
        // get calibrated values
        //SInt32 valueCal = HIDCalibrateValue(valueRaw, priv->axesElem[i]);
    }

    for (i = 0; i < in_dev->btn_count; i++)
    {
        in_dev->buttons[i] = HIDGetElementValue(priv->dev, priv->btnElem[i]);
    }
    */
}

/* -------------------------------------------------------------------------- */
void ndof_cleanup_internal()
{
    hid_cleanup();
}
