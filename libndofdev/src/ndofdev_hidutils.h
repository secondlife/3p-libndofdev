/*
 @file ndofdev_hidutils.h
 @brief This is a slightly modified subset of the original "HID Utilities" 
        sample code from Apple. Some type casting is required so
        library is framework and carbon free.
 
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

#ifndef _HID_Utilities_External_h_
#define _HID_Utilities_External_h_

/*****************************************************/
#pragma mark - Compiling directives to enable/disable code
/*****************************************************/

#define USE_HOTPLUGGING 	TRUE	// set TRUE to enable hot plugging!
#define USE_NOTIFICATIONS 	TRUE	// set TRUE to use notifications instead of device callbacks for hot unplugging!
#define LOG_DEVICES			FALSE	// for debugging; Logs new devices to stdout
#define LOG_ELEMENTS		FALSE	// for debugging; Logs new elements to stdout
#define LOG_SCORING			FALSE	// for debugging; Logs HIDFindDevice & HIDFindActionDeviceAndElement scoring to stdout
#define LOG_SEARCHING		FALSE	// for debugging; Logs HIDFindSubElement searching info to stdout

/*****************************************************/
#pragma mark - includes & imports
/*****************************************************/
#if !TARGET_RT_MAC_CFM
#include <IOKit/hid/IOHIDLib.h>
#endif TARGET_RT_MAC_CFM

#include <stdio.h>

/*****************************************************/
#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_IMPORT
#pragma import on
#endif

#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
#pragma pack( push, 2 )
#elif PRAGMA_STRUCT_PACK
#pragma pack( 2 )
#endif

/*****************************************************/
#pragma mark - typedef's, struct's, enums, defines, etc.
/*****************************************************/
#if TARGET_RT_MAC_CFM
// from IOHIDKeys.h( IOKit )
// this can't be included since the original file has framework includes
// developers may need to add definitions here
typedef enum IOHIDElementType {
	kIOHIDElementTypeInput_Misc    = 1, 
	kIOHIDElementTypeInput_Button   = 2, 
	kIOHIDElementTypeInput_Axis    = 3, 
	kIOHIDElementTypeInput_ScanCodes = 4, 
	kIOHIDElementTypeOutput      = 129, 
	kIOHIDElementTypeFeature      = 257, 
	kIOHIDElementTypeCollection    = 513
}IOHIDElementType;

typedef enum IOHIDReportType {
	kIOHIDReportTypeInput = 0, 
	kIOHIDReportTypeOutput, 
	kIOHIDReportTypeFeature, 
	kIOHIDReportTypeCount
}IOHIDReportType;

// Notes: This is a MachO function pointer. If you're using CFM you have to call MachOFunctionPointerForCFMFunctionPointer.
typedef void ( *IOHIDCallbackFunction )( void* target, unsigned long result, void* refcon, void* sender );
typedef void* IOHIDEventStruct;
#endif TARGET_RT_MAC_CFM

// Device and Element Interfaces

typedef enum HIDElementTypeMask
{
	kHIDElementTypeInput				= 1 << 1, 
	kHIDElementTypeOutput      	= 1 << 2, 
	kHIDElementTypeFeature      	= 1 << 3, 
	kHIDElementTypeCollection    	= 1 << 4, 
	kHIDElementTypeIO					= kHIDElementTypeInput | kHIDElementTypeOutput | kHIDElementTypeFeature, 
	kHIDElementTypeAll					= kHIDElementTypeIO | kHIDElementTypeCollection
}HIDElementTypeMask;

struct hu_element_t
{
	unsigned long type;						// the type defined by IOHIDElementType in IOHIDKeys.h
	long usage;								// usage within above page from IOUSBHIDParser.h which defines specific usage
	long usagePage;							// usage page from IOUSBHIDParser.h which defines general usage
	void* cookie;				 			// unique value( within device of specific vendorID and productID ) which identifies element, will NOT change
	long min;								// reported min value possible
	long max;								// reported max value possible
	long scaledMin;							// reported scaled min value possible
	long scaledMax;							// reported scaled max value possible
	long size;								// size in bits of data return from element
	unsigned char relative;					// are reports relative to last report( deltas )
	unsigned char wrapping;					// does element wrap around( one value higher than max is min )
	unsigned char nonLinear;				// are the values reported non-linear relative to element movement
	unsigned char preferredState;			// does element have a preferred state( such as a button )
	unsigned char nullState;				// does element have null state
	long units;								// units value is reported in( not used very often )
	long unitExp;							// exponent for units( also not used very often )
	char name[256];							// name of element( c string )

	// runtime variables
	long initialCenter; 					// center value at start up
	unsigned char hasCenter; 				// whether or not to use center for calibration
	long minReport; 						// min returned value
	long maxReport; 						// max returned value( calibrate call )
	long userMin; 							// user set value to scale to( scale call )
	long userMax;

	struct hu_element_t* pPrevious;			// previous element( NULL at list head )
	struct hu_element_t* pChild;			// next child( only of collections )
	struct hu_element_t* pSibling;			// next sibling( for elements and collections )

	long depth;
};
typedef struct hu_element_t hu_element_t;

struct hu_device_t
{
	void* interface;						// interface to device, NULL = no interface
	void* queue;							// device queue, NULL = no queue
	void* runLoopSource;					// device run loop source, NULL == no source
	void* queueRunLoopSource;				// device queue run loop source, NULL == no source
	void* transaction;						// output transaction interface, NULL == no interface
	void* notification;						// notifications
	char transport[256];					// device transport( c string )
	long vendorID;							// id for device vendor, unique across all devices
	long productID;							// id for particular product, unique across all of a vendors devices
	long version;							// version of product
	char manufacturer[256];					// name of manufacturer
	char product[256];						// name of product
	char serial[256];						// serial number of specific product, can be assumed unique across specific product or specific vendor( not used often )
	long locID;								// long representing location in USB( or other I/O ) chain which device is pluged into, can identify specific device on machine
	long usage;								// usage page from IOUSBHID Parser.h which defines general usage
	long usagePage;							// usage within above page from IOUSBHID Parser.h which defines specific usage
	long totalElements;						// number of total elements ( should be total of all elements on device including collections )( calculated, not reported by device )
	long features;							// number of elements of type kIOHIDElementTypeFeature
	long inputs;							// number of elements of type kIOHIDElementTypeInput_Misc or kIOHIDElementTypeInput_Button or kIOHIDElementTypeInput_Axis or kIOHIDElementTypeInput_ScanCodes
	long outputs;							// number of elements of type kIOHIDElementTypeOutput
	long collections;						// number of elements of type kIOHIDElementTypeCollection
	long axis;								// number of axis( calculated, not reported by device )
	long buttons;							// number of buttons( calculated, not reported by device )
	long hats;								// number of hat switches( calculated, not reported by device )
	long sliders;							// number of sliders( calculated, not reported by device )
	long dials;								// number of dials( calculated, not reported by device )
	long wheels;							// number of wheels( calculated, not reported by device )
	hu_element_t* pListElements;			// head of linked list of elements
	struct hu_device_t* pNext; 				// next device
};
typedef struct hu_device_t hu_device_t;

// this is the procedure type for a client hot plug callback
typedef OSStatus (*HotPlugCallbackProcPtr)(hu_device_t *inDevice);
typedef OSStatus (*HotUnplugCallbackProcPtr)(hu_device_t *inDevice);

/*****************************************************/
#pragma mark HID Utilities interface
/*****************************************************/
// Create and open an interface to device, required prior to extracting values or building queues
// Notes: appliction now owns the device and must close and release it prior to exiting
extern unsigned long HIDCreateOpenDeviceInterface( UInt32 inHIDDevice, hu_device_t* inDevice );

// builds list of device with elements( allocates memory and captures devices )
// list is allocated internally within HID Utilites and can be accessed via accessor functions
// structures within list are considered flat and user accessable, but not user modifiable
// can be called again to rebuild list to account for new devices( will do the right thing in case of disposing existing list )
// inUsagePage, usage are each a inNumDeviceTypes sized array of matching usage and usage pages
// returns TRUE if successful

extern Boolean HIDBuildMultiDeviceList( UInt32 *inUsagePage, UInt32 *inUsage, UInt32 inNumDeviceTypes );

// same as above but this uses a single inUsagePage and usage

extern Boolean HIDBuildDeviceList( UInt32 inUsagePage, UInt32 inUsage );

// updates the current device list for any new/removed devices
// if this is called before HIDBuildDeviceList the it functions like HIDBuildMultiDeviceList
// inUsagePage, usage are each a inNumDeviceTypes sized array of matching usage and usage pages
// returns TRUE if successful which means if any device were added or removed( the device config changed )

extern Boolean HIDUpdateDeviceList( UInt32 *inUsagePage, UInt32 *inUsage, UInt32 inNumDeviceTypes );

// release list built by above function
// MUST be called prior to application exit to properly release devices
// if not called( or app crashes ) devices can be recovered by pluging into different location in USB chain
extern void HIDReleaseDeviceList( void );

// does a device list exist
extern Boolean HIDHaveDeviceList( void );

// how many HID devices have been found
// returns 0 if no device list exist
extern UInt32 HIDCountDevices( void );

// get the first device in the device list
// returns NULL if no list exists
extern hu_device_t* HIDGetFirstDevice( void );

// get next device in list given current device as parameter
// returns NULL if end of list
extern hu_device_t* HIDGetNextDevice( const hu_device_t* inDevice );

// get the first element of device passed in as parameter
// returns NULL if no list exists or device does not exists or is NULL
// uses mask of HIDElementTypeMask to restrict element found
// use kHIDElementTypeIO to get previous HIDGetFirstDeviceElement functionality
extern hu_element_t* HIDGetFirstDeviceElement( const hu_device_t* inDevice, HIDElementTypeMask inTypeMask );

// get next element of given device in list given current element as parameter
// will walk down each collection then to next element or collection( depthwise traverse )
// returns NULL if end of list
// uses mask of HIDElementTypeMask to restrict element found
// use kHIDElementTypeIO to get previous HIDGetNextDeviceElement functionality
extern hu_element_t* HIDGetNextDeviceElement( hu_element_t* inElement, HIDElementTypeMask inTypeMask );

// Sets the client hot plug callback routine
extern OSStatus HIDSetHotPlugCallback(HotPlugCallbackProcPtr inAddCallbackPtr,
									  HotUnplugCallbackProcPtr inRemoveCallbackPtr);

/*****************************************************/
#pragma mark Name Lookup Interfaces
/*****************************************************/

// get vendor name from vendor ID
extern Boolean HIDGetVendorNameFromVendorID( long inVendorID, char* outCStrName );

// get product name from vendor/product ID
extern Boolean HIDGetProductNameFromVendorProductID( long inVendorID, long inProductID, char* outCStrName );

// set name from vendor id/product id look up( using cookies )
extern Boolean HIDGetElementNameFromVendorProductCookie( long inVendorID, long inProductID, long inCookie, char* inCStrName );

// set name from vendor id/product id look up( using usage page & usage )
extern Boolean HIDGetElementNameFromVendorProductUsage( long inVendorID, long inProductID, long inUsagePage, long inUsage, char* inCStrName );

// returns C string type name given a type enumeration passed in as parameter( see IOHIDKeys.h )
// returns empty string for invalid types
extern void HIDGetTypeName( IOHIDElementType inIOHIDElementType, char* inCStrName );

// returns C string usage given usage page and usage passed in as parameters( see IOUSBHIDParser.h )
// returns usage page and usage values in string form for unknown values
extern void HIDGetUsageName( long inUsagePage, long inUsage, char* inCStrName );

// print out all of an elements information
extern int HIDPrintElement( const hu_element_t* inElement );

// return TRUE if this is a valid device pointer
extern Boolean HIDIsValidDevice( const hu_device_t* inDevice );

// return TRUE if this is a valid element pointer for this device
extern Boolean HIDIsValidElement( const hu_device_t* inDevice, const hu_element_t* inElement );

/*****************************************************/
#pragma mark Element Event Queue and Value Interfaces
/*****************************************************/
enum
{
	kDefaultUserMin = 0, 					// default user min and max used for scaling
	kDefaultUserMax = 255
};

enum
{
	kDeviceQueueSize = 50	// this is wired kernel memory so should be set to as small as possible
							// but should account for the maximum possible events in the queue
							// USB updates will likely occur at 100 Hz so one must account for this rate of
							// if states change quickly( updates are only posted on state changes )
};

// completely removes all elements from queue and releases queue and device
extern unsigned long HIDDequeueDevice( hu_device_t* inDevice );

// releases interface to device, should be done prior to exiting application( called from HIDReleaseDeviceList )
extern unsigned long HIDCloseReleaseInterface( hu_device_t* inDevice );

// returns TRUE if an event is avialable for the element and fills out *outHIDEvent structure, returns FALSE otherwise
// outHIDEvent is a poiner to a IOHIDEventStruct, using void here for compatibility, users can cast a required
extern unsigned char HIDGetEvent( const hu_device_t* inDevice, void* outHIDEvent );

// returns current value for element, creating device interface as required, polling element
extern long HIDGetElementEvent( const hu_device_t* inDevice, hu_element_t* inElement, IOHIDEventStruct* outIOHIDEvent );

// returns current value for element, creating device interface as required, polling element
extern long HIDGetElementValue( const hu_device_t* inDevice, hu_element_t* inElement );

/*****************************************************/
#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
#pragma pack( pop )
#elif PRAGMA_STRUCT_PACK
#pragma pack( )
#endif

#ifdef PRAGMA_IMPORT_OFF
#pragma import off
#elif PRAGMA_IMPORT
#pragma import reset
#endif

#ifdef __cplusplus
}
#endif
/*****************************************************/
#endif // _HID_Utilities_External_h_
