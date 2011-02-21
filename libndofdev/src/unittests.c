/*
 @file ndofdev_unittests.c
 @brief Not-fully-automatic unit tests.
 
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
#include <assert.h>
#include <string.h>
#include "ndofdev_external.h"

#define LIBNDOF_UNIT_TESTS 1

unsigned long long usecs_since_startup();

#if TARGET_OS_MAC
#include <unistd.h>
#include <Carbon/Carbon.h>
unsigned long long usecs_since_startup()
{
    static UnsignedWide usecs = {0, 0};
    Microseconds(&usecs);
    return UnsignedWideToUInt64(usecs);
}
#elif defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <tchar.h>
#include <Windows.h>
int usleep(unsigned int usec)
{
    Sleep(usec/1000);
    return 0;
}
unsigned long long usecs_since_startup()
{
	return GetTickCount() * 1000;
}
#else /* linux */
#include <unistd.h>
unsigned long long usecs_since_startup()
{
	// to do 
    return 0;
}
#endif

/* -------------------------------------------------------------------------- */

/* see ndifdev_unittests.c */
extern void run_noninteractive_tests();
extern void test_ndof_libinit();

static NDOF_Device *hotplug_dev;

/* -------------------------------------------------------------------------- */
void test_read_values_loop()
{
	int err;
	unsigned long pollDelta = 1000000/10; // 10 frame/sec
    unsigned int usecs;
    unsigned long long t0, tstart;
#if _WIN32
    const long tolerance = 1000;
#else
    const long tolerance = 1;
#endif
    
    fprintf(stderr, "____ test_read_values_loop ___________________________\n");
    
    // init device
    hotplug_dev = ndof_create();
	hotplug_dev->axes_max = +3000;
	hotplug_dev->axes_min = -3000;
	hotplug_dev->absolute = 0;
    err = ndof_init_first(hotplug_dev, NULL);
    fprintf(stderr, "test_read_values_loop: err=%d. Using device:\n", err);
    ndof_dump(hotplug_dev);

    // poll device
	tstart = t0 = usecs_since_startup();
    while (t0 < tstart + 10000000) /* exit after 10 secs */
    {
        t0 = usecs_since_startup();
        
		ndof_update(hotplug_dev);
		
        assert(hotplug_dev->axes[0] >= hotplug_dev->axes_min - tolerance);
        assert(hotplug_dev->axes[0] <= hotplug_dev->axes_max + tolerance);
        assert(hotplug_dev->axes[1] >= hotplug_dev->axes_min - tolerance);
        assert(hotplug_dev->axes[1] <= hotplug_dev->axes_max + tolerance);
        assert(hotplug_dev->axes[2] >= hotplug_dev->axes_min - tolerance);
        assert(hotplug_dev->axes[2] <= hotplug_dev->axes_max + tolerance);
        assert(hotplug_dev->axes[3] >= hotplug_dev->axes_min - tolerance);
        assert(hotplug_dev->axes[3] <= hotplug_dev->axes_max + tolerance);
        assert(hotplug_dev->axes[4] >= hotplug_dev->axes_min - tolerance);
        assert(hotplug_dev->axes[4] <= hotplug_dev->axes_max + tolerance);
        assert(hotplug_dev->axes[5] >= hotplug_dev->axes_min - tolerance);
        assert(hotplug_dev->axes[5] <= hotplug_dev->axes_max + tolerance);
        
        /* otherwise, sleep until the next timeslot */
        usecs = (unsigned int)(pollDelta - (usecs_since_startup() - t0));
        if (0 < usecs)
            usleep(usecs);
    }
    
	fprintf(stderr, "  done\n");
}

NDOF_HotPlugResult test_add_callback(NDOF_Device *dev)
{
	fprintf(stderr, "test_add_callback\n");
    hotplug_dev = dev;
	return NDOF_KEEP_HOTPLUGGED;
}

void test_removal_callback(NDOF_Device *dev)
{
	fprintf(stderr, "test_removal_callback\n");	
}


/* -------------------------------------------------------------------------- */
void run_all_tests()
{
    int err;
    
#if NDOF_DEBUG
    // does its own libinit and libcleanup
    //run_noninteractive_tests();
#endif
    
    err = ndof_libinit(test_add_callback, test_removal_callback, NULL);
    fprintf(stderr, "ndof_libinit returned %d\n", err);
    assert(err == 0);

    test_read_values_loop();
    ndof_libcleanup();
}

/* -------------------------------------------------------------------------- */
int main(int argc, const char * argv[]) 
{
    fprintf(stderr, "libndofdev Unit Tests\n");
    run_all_tests();
    fprintf(stderr, "libndofdev Unit Tests: all done. Exiting.\n");
    return 0;
}
