/*
 @file ndofdev_unittests.c
 @brief Basic unit tests.
 
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

#if LIBNDOF_UNIT_TESTS

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "ndofdev_external.h"

/* -------------------------------------------------------------------------- */
/* see ndifdev.c */
void test_device_list_add();

/* -------------------------------------------------------------------------- */
void test_ndof_libinit()
{
    int err;
    fprintf(stderr, "____ test_ndof_libinit _______________________________\n");
    err = ndof_libinit(NULL, NULL, NULL);
    fprintf(stderr, "ndof_libinit returned %d\n", err);
    assert(err == 0);
    fprintf(stderr, "  done\n");
}

#if TARGET_OS_MAC
/* -------------------------------------------------------------------------- */
void test_ndof_devcount()
{
    int n = -1;
    fprintf(stderr, "____ test_ndof_devcount _____________________________\n");
    n = ndof_devcount();
	fprintf(stderr, "number of NDOF devices found: %d\n", n);
    assert(n>0);
    fprintf(stderr, "  done\n");
}
#endif

/* -------------------------------------------------------------------------- */
void test_ndof_create()
{
    NDOF_Device *dev;
    unsigned i;
	long z = 0;
	
    fprintf(stderr, "____ test_ndof_create ____________________________\n");
	
    dev = ndof_create();
    
	ndof_dump(dev);
    assert(dev->private_data != NULL);
    assert(strcmp(dev->manufacturer, "") == 0);
    assert(strcmp(dev->product, "") == 0);
    assert(dev->axes_count == 0);
    assert(dev->btn_count == -1);
    assert(dev->axes_min == -500);
    assert(dev->axes_max == +500);
	for (i=0; i<NDOF_MAX_AXES_COUNT; i++)
		assert(memcmp(&dev->axes[i], &z, sizeof(dev->axes[i])) == 0);
	for (i=0; i<NDOF_MAX_BUTTONS_COUNT; i++)
		assert(memcmp(&dev->buttons[i], &z, sizeof(dev->buttons[i])) == 0);

    fprintf(stderr, "  done\n");
}

/* -------------------------------------------------------------------------- */
void test_ndof_init_first()
{
    int err;
    unsigned i;
    NDOF_Device *dev;
	long z = 0;
    
    fprintf(stderr, "____ test_ndof_init_first ______________________________\n");
    
    dev = ndof_create();
    err = ndof_init_first(dev, NULL);
    
	ndof_dump(dev);
    fprintf(stderr, "err=%d\n", err);
    assert(err == 0);
    assert(dev->private_data != NULL);
    assert(dev->axes_count > 2);
    assert(dev->btn_count >= 2);
    assert(dev->axes_min < dev->axes_max);
	#if 0 && !defined(_WIN32) && !defined(WIN32)
	assert(strcmp(dev->manufacturer, "3Dconnexion") == 0 || 
		   strcmp(dev->manufacturer, "Logitech") == 0);
    #endif
	for (i=0; i<NDOF_MAX_AXES_COUNT; i++)
		assert(memcmp(&dev->axes[i], &z, sizeof(dev->axes[i])) == 0);
	for (i=0; i<NDOF_MAX_BUTTONS_COUNT; i++)
		assert(memcmp(&dev->buttons[i], &z, sizeof(dev->buttons[i])) == 0);

    fprintf(stderr, "  done\n");
}

/* -------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" 
#endif
void run_noninteractive_tests()
{
    test_ndof_libinit();
	test_device_list_add();

    #if TARGET_OS_MAC
	test_ndof_devcount();
    #endif

    test_ndof_create();
    test_ndof_init_first();
    
    ndof_libcleanup();
}

#endif
