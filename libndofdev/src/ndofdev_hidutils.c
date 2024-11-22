/*
 @file ndofdev_hidutils.c
 @brief These functions are a slightly modified subset of the original
        "HID Utilities" sample code from Apple. 
 
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

/*****************************************************/
#pragma mark - typedefs, enums, defines, etc.
/*****************************************************/
#define FAKE_MISSING_NAMES		0	// for debugging; returns the vendor, product & cookie ( or usage info ) as numbers.
#define VERBOSE_ELEMENT_NAMES	0	// set TRUE to include vender & product names in element names ( useful for debugging )

#define kNameKeyCFStringRef CFSTR( "Name" )

/*****************************************************/
#pragma mark - includes & imports
/*****************************************************/

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include "ndofdev_hidutils_err.h"
#include "ndofdev_hidutils.h"

/*****************************************************/
#pragma mark - local ( static ) function prototypes
/*****************************************************/
static CFPropertyListRef hu_XMLLoad( CFStringRef inResourceName, CFStringRef inResourceExtension );
static Boolean hu_XMLSearchForElementNameByCookie( long inVendorID, long inProductID, long inCookie, char* outCStr );
static Boolean hu_XMLSearchForElementNameByUsage( long inVendorID, long inProductID, long inUsagePage, long inUsage, char* outCStr );
static char hu_MapChar( char c );
static void hu_CleanString( char* inCStr );
static void hu_GetElementInfo( CFTypeRef inElementCFDictRef, hu_element_t* inElement );
static void hu_AddElement( CFTypeRef inElementCFDictRef, hu_element_t **inCurrentElement );
static void hu_GetElementsCFArrayHandler( const void* value, void* parameter );
static void hu_GetElements( CFTypeRef inElementCFDictRef, hu_element_t **outCurrentElement );
static void hu_GetCollectionElements( CFDictionaryRef deviceProperties, hu_element_t **outCurrentCollection );
static void hu_TopLevelElementHandler( const void* value, void* parameter );
static void hu_GetDeviceInfo( io_object_t inHIDDevice, CFDictionaryRef inDeviceCFDictionaryRef, hu_device_t* inDevice );
static hu_device_t **hu_AddDevice( hu_device_t **inDeviceListHead, hu_device_t* inNewDevice );
static hu_device_t* hu_MoveDevice( hu_device_t **inDeviceListHead, hu_device_t* inNewDevice, hu_device_t **inOldListDeviceHead );
static hu_device_t* hu_BuildDevice( io_object_t inHIDDevice );
static hu_device_t* hu_CreateSingleTypeDeviceList( io_iterator_t inHIDObjectIterator );
static hu_device_t* hu_CreateMultiTypeDeviceList( UInt32 *inUsagePage, UInt32 *inUsage, UInt32 inNumDeviceTypes );
static Boolean hu_FindDeviceInList( hu_device_t* inDeviceList, hu_device_t* inFindDevice );
static void hu_MergeDeviceList( hu_device_t **inNewDeviceList, hu_device_t **inDeviceList );
static void hu_AddDevices(hu_device_t **inDeviceListHead, 
                          io_iterator_t inIODeviceIterator,
                          Boolean inIsHotPlugEvent);
static CFMutableDictionaryRef hu_SetUpMatchingDictionary( UInt32 inUsagePage, UInt32 inUsage );
static void hu_DisposeDeviceElements( hu_element_t* inElement );
static hu_device_t* hu_DisposeDevice( hu_device_t* inDevice );
static UInt32 hu_CountCurrentDevices( void );
static Boolean hu_MatchElementTypeMask( IOHIDElementType inIOHIDElementType, HIDElementTypeMask inTypeMask );
static hu_element_t* hu_GetDeviceElement( hu_element_t* inElement, HIDElementTypeMask inTypeMask );

#if USE_NOTIFICATIONS
// note: was called 'hu_DeviceNotification' in HID Utilities
static void hu_RemovalNotification(void *inRefCon, io_service_t inService, 
                                   natural_t inMessageType, void *inMmsgArg);
#else
static void hu_RemovalCallbackFunction(void* target, IOReturn inResult, 
                                       void* inRefCon, void* inSender);
#endif // USE_NOTIFICATIONS

#if USE_HOTPLUGGING
// note: was called 'hu_IOServiceMatchingNotification' in HID Utilities
static void hu_HotPlugAddNotification(void *inRefCon, io_iterator_t inIODeviceIter);
#endif // USE_HOTPLUGGING

/*****************************************************/
#pragma mark - local ( static ) globals
/*****************************************************/

static CFPropertyListRef		gCookieCFPropertyListRef = NULL;
static CFPropertyListRef		gUsageCFPropertyListRef = NULL;

#if USE_HOTPLUGGING
static HotPlugCallbackProcPtr	gHotPlugAddCallbackPtr = NULL;
static HotUnplugCallbackProcPtr	gHotPlugRemovalCallbackPtr = NULL;
static IONotificationPortRef	gNotifyPort;
static CFRunLoopRef				gRunLoop;
#endif // USE_HOTPLUGGING

// for element retrieval
static hu_device_t*				gCurrentDevice  = NULL;
static Boolean					gAddAsChild		= FALSE;
static int						gDepth			= FALSE;

// our global list of HID devices
static hu_device_t*				gDeviceList		= NULL;
static UInt32					gNumDevices		= 0;

/*****************************************************/
#pragma mark - exported function implementations
/*****************************************************/

/*************************************************************************
*
* hu_LoadFromXMLFile( inCFURLRef )
*
* Purpose:  load a property list from an XML file
*
* Inputs:   inCFURLRef			- URL for the file
*
* Returns:  CFPropertyListRef   - the data
*/
static CFPropertyListRef hu_LoadFromXMLFile( CFURLRef inCFURLRef )
{
	CFPropertyListRef propertyList = nil;
	
	// Read the XML file.
    CFPropertyListFormat propertyListFormat;
    
    CFReadStreamRef fileStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, inCFURLRef);
    if (fileStream != nil) {
        if ( CFReadStreamOpen(fileStream) ) {
            propertyList = CFPropertyListCreateWithStream(
                                kCFAllocatorDefault, fileStream, 0,
                                kCFPropertyListImmutable, &propertyListFormat, nil);
        }
        CFRelease(fileStream);
    }
    
    return propertyList;
}	// hu_LoadFromXMLFile


/*************************************************************************
*
* HIDCreateOpenDeviceInterface( inHIDDevice, inDevice )
*
* Purpose:  Create and open an interface to device
*
* Notes:	required prior to extracting values or building queues
*			appliction now owns the device and must close and release it prior to exiting
*
* Inputs:   inHIDDevice  - the HID ( service ) device
*			inDevice		- the device
*
* Returns:  SInt32		- error code ( if any )
*/

unsigned long HIDCreateOpenDeviceInterface( UInt32 inHIDDevice, hu_device_t* inDevice )
{
	IOReturn				result				= kIOReturnSuccess;	// assume success( optimist! )
	HRESULT					plugInResult		= S_OK;				// assume success( optimist! )
	SInt32					score				= 0;
	IOCFPlugInInterface		**tPlugInInterface = NULL;
	
	if ( !inDevice->interface ) {
		result = IOCreatePlugInInterfaceForService( inHIDDevice, kIOHIDDeviceUserClientTypeID,
													kIOCFPlugInInterfaceID, &tPlugInInterface, &score );
		if ( kIOReturnSuccess == result ) {
			// Call a method of the intermediate plug-in to create the device interface
			plugInResult = ( *tPlugInInterface )->QueryInterface( tPlugInInterface,
																  CFUUIDGetUUIDBytes( kIOHIDDeviceInterfaceID ), ( void* ) & ( inDevice->interface ) );
			if ( S_OK != plugInResult ) {
				HIDReportErrorNum( "\nHIDCreateOpenDeviceInterface: Couldn’t query HID class device interface from plugInInterface", plugInResult );
			}
			IODestroyPlugInInterface( tPlugInInterface ); // replace( *tPlugInInterface )->Release( tPlugInInterface )
		} else {
			HIDReportErrorNum( "\nHIDCreateOpenDeviceInterface: Failed to create plugInInterface via IOCreatePlugInInterfaceForService.", result );
		}
	}
	if ( inDevice->interface ) {
		result = ( *( IOHIDDeviceInterface** )inDevice->interface )->open( inDevice->interface, 0 );
		if ( kIOReturnSuccess != result ) {
			HIDReportErrorNum( "\nHIDCreateOpenDeviceInterface: Failed to open inDevice->interface via open.", result );
		}
	}
	return result;
}

/*************************************************************************
*
* HIDBuildMultiDeviceList( pUsagePages, pUsages, inNumDeviceTypes )
*
* Purpose:  builds list of devices with elements
*
* Notes:	allocates memory and captures devices
*			list is allocated internally within HID Utilites and can be accessed via accessor functions
*			structures within list are considered flat and user accessable, but not user modifiable
*			can be called again to rebuild list to account for new devices
*			( will do the right thing in case of disposing existing list )
*
* Inputs:   pUsagePages		- inNumDeviceTypes sized array of matching usage pages
*			pUsages			- inNumDeviceTypes sized array of matching usages
*			inNumDeviceTypes - number of usage pages & usages
*
* Returns:  Boolean		- if successful
*/

Boolean HIDBuildMultiDeviceList( UInt32 *pUsagePages, UInt32 *pUsages, UInt32 inNumDeviceTypes )
{
	gDeviceList = hu_CreateMultiTypeDeviceList( pUsagePages, pUsages, inNumDeviceTypes );
	gNumDevices = hu_CountCurrentDevices( ); // set count
	
	return( NULL != gDeviceList );
}

/*************************************************************************
*
* HIDBuildDeviceList( inUsagePage, inUsage )
*
* Purpose:  builds list of devices with elements
*
* Notes:	same as above but this uses a single inUsagePage and usage
*			allocates memory and captures devices
*			list is allocated internally within HID Utilites and can be accessed via accessor functions
*			structures within list are considered flat and user accessable, but not user modifiable
*			can be called again to rebuild list to account for new devices
*			( will do the right thing in case of disposing existing list )
*
* Inputs:   inUsagePage		- usage page
*			inUsage			- usages
*
* Returns:  Boolean		- if successful
*/

Boolean HIDBuildDeviceList( UInt32 inUsagePage, UInt32 inUsage )
{
    // call HIDBuildMultiDeviceList with a single usage
	return HIDBuildMultiDeviceList( &inUsagePage, &inUsage, 1 ); 
}

/*************************************************************************
*
* HIDReleaseDeviceList( void )
*
* Purpose:  release list built by above functions
*
* Notes:	MUST be called prior to application exit to properly release devices
*			if not called( or app crashes ) devices can be recovered by pluging into different location in USB chain
*
* Inputs:   none
*
* Returns:  none
*/

void HIDReleaseDeviceList( void )
{
	while ( gDeviceList ) {
		gDeviceList = hu_DisposeDevice( gDeviceList ); // dispose current device return next device will set gDeviceList to NULL
	}
	gNumDevices = 0;
}

/*************************************************************************
*
* HIDHaveDeviceList( void )
*
* Purpose:  does a device list exist?
*
* Inputs:   none
*
* Returns:  Boolean		- TRUE if we have previously built a device list
*/

Boolean HIDHaveDeviceList( void )
{
	return( NULL != gDeviceList );
}

/*************************************************************************
*
* HIDGetFirstDevice( void )
*
* Purpose:  get the first device in the device list
*
* Notes:	returns NULL if no list exists
*
* Inputs:   none
*
* Returns:  hu_device_t  - the first device in our device list
*/

hu_device_t* HIDGetFirstDevice( void )
{
	return gDeviceList;
}

/*************************************************************************
*
* HIDGetNextDevice( inDevice )
*
* Purpose:  get the next device in the device list
*
* Notes:	returns NULL if end-of-list
*
* Inputs:   inDevice		- the current device
*
* Returns:  hu_device_t - the next device in our device list
*/

hu_device_t* HIDGetNextDevice( const hu_device_t* inDevice )
{
	hu_device_t* result = NULL;	// assume failure ( pessimist! )
	if ( HIDIsValidDevice( inDevice ) ) {
		result = inDevice->pNext;
	}
	return result;
}

/*************************************************************************
*
* HIDGetFirstDeviceElement( inDevice, inTypeMask )
*
* Purpose:  get the first element of this type on this device
*
* Notes:	returns NULL if no list exists or device does not exists or
*			is NULL or no elements of this type exist on this device
*
* Inputs:   inDevice		- the current device
*			inTypeMask   - the type of element we're interested in
*
* Returns:  hu_element_t  - the next element
*/

hu_element_t* HIDGetFirstDeviceElement( const hu_device_t* inDevice, HIDElementTypeMask inTypeMask )
{
	hu_element_t* result = NULL;
	if ( HIDIsValidDevice( inDevice ) ) {
		if ( inDevice->pListElements && 
			 hu_MatchElementTypeMask( inDevice->pListElements->type, inTypeMask ) ) 
		{	// ensure first type matches
			result = inDevice->pListElements;
		} else {
			result = HIDGetNextDeviceElement( inDevice->pListElements, inTypeMask );
		}
	}
	return result;
}

/*************************************************************************
*
* HIDGetNextDeviceElement( inElement, inTypeMask )
*
* Purpose:  get the next element of this type on this device
*
* Notes:	returns NULL if no list exists or device does not exists or
*			is NULL or no elements of this type exist on this device
*
* Inputs:   inElement	- the current element
*			inTypeMask   - the type of element we're interested in
*
* Returns:  hu_element_t  - the next element
*/

hu_element_t* HIDGetNextDeviceElement( hu_element_t* inElement, HIDElementTypeMask inTypeMask )
{
	// should only have elements passed in( though someone could mix calls and pass us a collection )
	// collection means return the next child or sibling( in that order )
	// element means return the next sibling( as elements can't have children )
	if ( inElement ) {
		if ( inElement->pChild ) {
			if ( inElement->type != kIOHIDElementTypeCollection ) {
				HIDReportError( "Malformed element list: found child of element." );
			} else {
				return hu_GetDeviceElement( inElement->pChild, inTypeMask ); // return the child of this element
			}
		} else if ( inElement->pSibling ) {
			return hu_GetDeviceElement( inElement->pSibling, inTypeMask ); //return the sibling of this element
		} else {	// at end, back up correctly
			hu_element_t* previousElement = NULL;
			// malformed device ending in collection
			if ( kIOHIDElementTypeCollection == inElement->type ) {
				HIDReportError( "Malformed device: found collection at end of element chain." );
			}
			// walk back up tree to element prior to first collection ecountered and take next element
			while ( inElement->pPrevious ) {
				previousElement = inElement;
				inElement = inElement->pPrevious; // look at previous element
												  // if we have a collection and the previous element is the branch element( should have both a colection and next element attached to it )
												  // if we found a collection, which we are not at the sibling level that actually does have siblings
				if ( ( ( kIOHIDElementTypeCollection == inElement->type ) && ( previousElement != inElement->pSibling ) && inElement->pSibling ) ||
					 // or if we are at the top
					 ( !inElement->pPrevious ) ) // at top of tree
					break;
			}
			if ( ( !inElement->pPrevious ) && ( ( !inElement->pSibling ) || ( previousElement == inElement->pSibling ) ) )
				return NULL; // got to top of list with only a collection as the first element
							 // now we must have been down the child route so go down the sibling route
			inElement = inElement->pSibling; // element of interest
			return hu_GetDeviceElement( inElement, inTypeMask ); // otherwise return this element
		}
	}
	return NULL;
}

/*************************************************************************
*
* HIDGetElementEvent( inDevice, inElement, outIOHIDEvent )
*
* Purpose:  returns current value for element, polling element
*
* Inputs:   inDevice				- the device
*			inElement			- the element
*			outIOHIDEvent			- address where to return the event
*
* Returns:  UInt32				- error code ( if any )
*			outIOHIDEvent			- the event
*/
long HIDGetElementEvent( const hu_device_t* inDevice, hu_element_t* inElement, IOHIDEventStruct* outIOHIDEvent )
{
	IOReturn result = kIOReturnBadArgument; 	// assume failure ( pessimist! )
	IOHIDEventStruct hidEvent;
	
	hidEvent.value = 0;
	hidEvent.longValueSize = 0;
	hidEvent.longValue = nil;
	
	if ( HIDIsValidElement( inDevice, inElement ) ) {
		if ( inDevice->interface ) {
			// ++ NOTE: If the element type is feature then use queryElementValue instead of getElementValue
			if ( kIOHIDElementTypeFeature == inElement->type ) {
				result = ( *( IOHIDDeviceInterface** ) inDevice->interface )->queryElementValue( inDevice->interface, (IOHIDElementCookie)inElement->cookie, &hidEvent, 0, NULL, NULL, NULL );
				if ( kIOReturnUnsupported == result )	// unless it's unsuported.
					goto try_getElementValue;
				else if ( kIOReturnSuccess != result ) {
					HIDReportErrorNum( "\nHIDGetElementEvent - Could not get HID element value via queryElementValue.", result );
				}
			} else if ( inElement->type <= kIOHIDElementTypeInput_ScanCodes ) {
try_getElementValue:
				result = ( *( IOHIDDeviceInterface** ) inDevice->interface )->getElementValue( inDevice->interface, (IOHIDElementCookie)inElement->cookie, &hidEvent );
				if ( kIOReturnSuccess != result ) {
					HIDReportErrorNum( "\nHIDGetElementEvent - Could not get HID element value via getElementValue.", result );
				}
			}
			// on 10.0.x this returns the incorrect result for negative ranges, so fix it!!!
			// this is not required on Mac OS X 10.1+
			if ( ( inElement->min < 0 ) && ( hidEvent.value > inElement->max ) ) // assume range problem
				hidEvent.value = hidEvent.value + inElement->min - inElement->max - 1;
			
			*outIOHIDEvent = hidEvent;
		} else {
			HIDReportError( "\nHIDGetElementEvent - no interface for device." );
		}
	} else {
		HIDReportError( "\nHIDGetElementEvent - invalid device and/or element." );
	}
	
	// record min and max for auto scale and auto ...
	if ( hidEvent.value < inElement->minReport )
		inElement->minReport = hidEvent.value;
	if ( hidEvent.value > inElement->maxReport )
		inElement->maxReport = hidEvent.value;
	return result;
}

/*************************************************************************
*
* HIDGetElementValue( inDevice, inElement )
*
* Purpose:  returns the current value for an element( polling )
*
* Notes:		will return 0 on error conditions which should be accounted for by application
*
* Inputs:   inDevice		- the device
*			inElement	- the element
*
* Returns:  SInt32		- current value for element
*/

long HIDGetElementValue( const hu_device_t* inDevice, hu_element_t* inElement )
{
	long result = 0;
	
	IOHIDEventStruct hidEvent;
	if ( kIOReturnSuccess == HIDGetElementEvent( inDevice, inElement, &hidEvent ) ) {
		result = hidEvent.value;
	}
	return result;
}

/*************************************************************************
*
* HIDGetElementNameFromVendorProductCookie( inVendorID, inProductID, inCookie, outCStrName )
*
* Purpose:  Uses an elements vendor, product & cookie to generate a name for it.
*
* Notes:	Now uses XML files to store dictionary of names
*
* Inputs:   inVendorID   - the elements vendor ID
*			inProductID  - the elements product ID
*			inCookie		- the elements cookie
*			outCStrName   - address where result will be returned
* Returns:  Boolean		- if successful
*/
Boolean HIDGetElementNameFromVendorProductCookie( long inVendorID, long inProductID, long inCookie, char* outCStrName )
{
	Boolean result = FALSE;
	*outCStrName = 0; // clear name
	
	// Look in the XML file first
	if ( hu_XMLSearchForElementNameByCookie( inVendorID, inProductID, inCookie, outCStrName ) )
		return TRUE;
	
#if FAKE_MISSING_NAMES
	sprintf( outCStrName, "#{V:%ld, P:%ld, C:%ld}#", inVendorID, inProductID, inCookie );
#else
	result = FALSE;
#endif // FAKE_MISSING_NAMES
	return result;
}	// HIDGetElementNameFromVendorProductCookie

/*************************************************************************
*
* HIDGetElementNameFromVendorProductUsage( inVendorID, inProductID, inUsagePage, inUsage, outCStrName )
*
* Purpose:  Uses an elements vendor, product & usage info to generate a name for it.
*
* Notes:	Now uses XML files to store dictionary of names
*
* Inputs:   inVendorID   - the elements vendor ID
*			inProductID  - the elements product ID
*			inUsagePage	- the elements usage page
*			inUsage		- the elements usage
*			outCStrName   - address where result will be returned
* Returns:  Boolean		- if successful
*/
Boolean HIDGetElementNameFromVendorProductUsage( long inVendorID, long inProductID, long inUsagePage, long inUsage, char* outCStrName )
{
	Boolean result = FALSE;
	*outCStrName = 0; // clear name
	
	if ( hu_XMLSearchForElementNameByUsage( inVendorID, inProductID, inUsagePage, inUsage, outCStrName ) )
		return TRUE;
	
#if FAKE_MISSING_NAMES
	sprintf( outCStrName, "#{V:%ld, P:%ld, U:%ld:%ld}#", inVendorID, inProductID, inUsagePage, inUsage );
	result = TRUE;
#endif // FAKE_MISSING_NAMES
	return result;
}	// HIDGetElementNameFromVendorProductUsage

/*************************************************************************
*
* hu_XMLGetUsageName( inUsagePage, inUsage, outCStr )
*
* Purpose:  Find a usage string in the <HID_usage_strings.plist> resource( XML ) file
*
* Inputs:   inUsagePage	- the usage page
*			inUsage		- the usage
*			outCStr		- address where the usage name will be returned
*
* Returns:  Boolean		- if successful
*			outCStr		- the usage name
*/

static Boolean hu_XMLGetUsageName( long inUsagePage, long inUsage, char* outCStr )
{
	static CFPropertyListRef tCFPropertyListRef = NULL;
	Boolean results = FALSE;
	
	if ( !tCFPropertyListRef )
		tCFPropertyListRef = hu_XMLLoad( CFSTR( "HID_usage_strings" ), CFSTR( "plist" ) );
	
	if ( tCFPropertyListRef ) {
		if ( CFDictionaryGetTypeID( ) == CFGetTypeID( tCFPropertyListRef ) ) {
			CFStringRef	pageKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "0x%4.4lX" ), inUsagePage );
			if ( pageKeyCFStringRef ) {
				CFDictionaryRef pageCFDictionaryRef;
				if ( CFDictionaryGetValueIfPresent( tCFPropertyListRef, pageKeyCFStringRef, ( const void** ) &pageCFDictionaryRef ) ) {
					CFStringRef	pageCFStringRef;
					if ( CFDictionaryGetValueIfPresent( pageCFDictionaryRef, kNameKeyCFStringRef, ( const void** ) &pageCFStringRef ) ) {
						CFStringRef fullCFStringRef = NULL;
						CFStringRef	usageKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "0x%4.4lX" ), inUsage );
						if ( usageKeyCFStringRef ) {
							CFStringRef	usageCFStringRef;
							if ( CFDictionaryGetValueIfPresent( pageCFDictionaryRef, usageKeyCFStringRef, ( const void** ) &usageCFStringRef ) ) {
								fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@ %@" ), pageCFStringRef, usageCFStringRef );
							}
#if FAKE_MISSING_NAMES
							else {
								fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@ #%@" ), pageCFStringRef, usageKeyCFStringRef );
							}
#endif
							if ( fullCFStringRef ) {
								// CFShow( fullCFStringRef );
								results = CFStringGetCString( fullCFStringRef, outCStr, CFStringGetLength( fullCFStringRef ) * sizeof( UniChar ) + 1, kCFStringEncodingMacRoman );
								CFRelease( fullCFStringRef );
							}
							CFRelease( usageKeyCFStringRef );
						}
					}
				}
				CFRelease( pageKeyCFStringRef );
			}
		}
		//++CFRelease( tCFPropertyListRef );	// Leak this!
	}
	return results;
}	// hu_XMLGetUsageName

/*************************************************************************
*
* hu_XMLLoad( inResourceName, inResourceExtension )
*
* Purpose:  Load a resource( XML ) file into a CFPropertyListRef
*
* Inputs:   inResourceName		- name of the resource file
*			inResourceExtension  - extension of the resource file
*
* Returns:  CFPropertyListRef   - the data
*/
static CFPropertyListRef hu_XMLLoad( CFStringRef inResourceName, CFStringRef inResourceExtension )
{
	CFURLRef resFileCFURLRef;
	CFPropertyListRef tCFPropertyListRef = NULL;
	
	resFileCFURLRef = CFBundleCopyResourceURL( CFBundleGetMainBundle( ), inResourceName, inResourceExtension, NULL );
	if ( resFileCFURLRef ) {
		tCFPropertyListRef = hu_LoadFromXMLFile( resFileCFURLRef );
		CFRelease( resFileCFURLRef );
	}
	return tCFPropertyListRef;
}	// hu_XMLLoad

/*************************************************************************
*
* hu_XMLSearchForElementNameByCookie( inVendorID, inProductID, inCookie, outCStr )
*
* Purpose:  Find an element string in the <HID_cookie_strings.plist> resource( XML ) file
*
* Inputs:   inVendorID   - the elements vendor ID
*			inProductID  - the elements product ID
*			inCookie		- the elements cookie
*			outCStr		- address where result will be returned
*
* Returns:  Boolean		- if successful
*/
static Boolean hu_XMLSearchForElementNameByCookie( long inVendorID, long inProductID, long inCookie, char* outCStr )
{
	Boolean results = FALSE;
	
	if ( !gCookieCFPropertyListRef )
		gCookieCFPropertyListRef = hu_XMLLoad( CFSTR( "HID_cookie_strings" ), CFSTR( "plist" ) );
	
	if ( gCookieCFPropertyListRef ) {
		if ( CFDictionaryGetTypeID( ) == CFGetTypeID( gCookieCFPropertyListRef ) ) {
			CFStringRef	vendorKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%ld" ), inVendorID );
			if ( vendorKeyCFStringRef ) {
				CFDictionaryRef vendorCFDictionaryRef;
				if ( CFDictionaryGetValueIfPresent( gCookieCFPropertyListRef, vendorKeyCFStringRef, ( const void** ) &vendorCFDictionaryRef ) ) {
					CFDictionaryRef productCFDictionaryRef;
					CFStringRef	productKeyCFStringRef;
					CFStringRef	vendorCFStringRef;
					
					if ( CFDictionaryGetValueIfPresent( vendorCFDictionaryRef, kNameKeyCFStringRef, ( const void** ) &vendorCFStringRef ) ) {
						//CFShow( vendorCFStringRef );
					}
					productKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%ld" ), inProductID );
					
					if ( CFDictionaryGetValueIfPresent( vendorCFDictionaryRef, productKeyCFStringRef, ( const void** ) &productCFDictionaryRef ) ) {
						CFStringRef fullCFStringRef = NULL;
						CFStringRef	cookieKeyCFStringRef;
						CFStringRef	productCFStringRef;
						CFStringRef	cookieCFStringRef;
						
						if ( CFDictionaryGetValueIfPresent( productCFDictionaryRef, kNameKeyCFStringRef, ( const void** ) &productCFStringRef ) ) {
							//CFShow( productCFStringRef );
						}
						cookieKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%ld" ), inCookie );
						
						if ( CFDictionaryGetValueIfPresent( productCFDictionaryRef, cookieKeyCFStringRef, ( const void** ) &cookieCFStringRef ) ) {
#if VERBOSE_ELEMENT_NAMES
							fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@ %@ %@" ), vendorCFStringRef, productCFStringRef, cookieCFStringRef );
#else
							fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@" ), cookieCFStringRef );
#endif // VERBOSE_ELEMENT_NAMES
							// CFShow( cookieCFStringRef );
						}
#if FAKE_MISSING_NAMES
						else {
							fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@ %@ #%@" ), vendorCFStringRef, productCFStringRef, cookieKeyCFStringRef );
						}
#endif // FAKE_MISSING_NAMES
						if ( fullCFStringRef ) {
							// CFShow( fullCFStringRef );
							results = CFStringGetCString( fullCFStringRef, outCStr, CFStringGetLength( fullCFStringRef ) * sizeof( UniChar ) + 1, kCFStringEncodingMacRoman );
							CFRelease( fullCFStringRef );
						}
						CFRelease( cookieKeyCFStringRef );
					}
					CFRelease( productKeyCFStringRef );
				}
				CFRelease( vendorKeyCFStringRef );
			}
		}
		//++CFRelease( gCookieCFPropertyListRef );	// Leak this!
	}
	return results;
}	// hu_XMLSearchForElementNameByCookie

/*************************************************************************
*
* hu_XMLSearchForElementNameByUsage( inVendorID, inProductID, inUsagePage, inUsage, outCStr )
*
* Purpose:  Find an element string in the <HID_device_usage_strings.plist> resource( XML ) file
*
* Inputs:   inVendorID   - the elements vendor ID
*			inProductID  - the elements product ID
*			inUsagePage	- the elements usage page
*			inUsage		- the elements usage
*			outCStr		- address where result will be returned
*
* Returns:  Boolean		- if successful
*/
static Boolean hu_XMLSearchForElementNameByUsage( long inVendorID, long inProductID, long inUsagePage, long inUsage, char* outCStr )
{
	Boolean results = FALSE;
	
	if ( !gUsageCFPropertyListRef )
		gUsageCFPropertyListRef = hu_XMLLoad( CFSTR( "HID_device_usage_strings" ), CFSTR( "plist" ) );
	
	if ( gUsageCFPropertyListRef ) {
		if ( CFDictionaryGetTypeID( ) == CFGetTypeID( gUsageCFPropertyListRef ) ) {
			CFDictionaryRef vendorCFDictionaryRef;
			CFStringRef	vendorKeyCFStringRef;
			vendorKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%ld" ), inVendorID );
			
			if ( CFDictionaryGetValueIfPresent( gUsageCFPropertyListRef, vendorKeyCFStringRef, ( const void** ) &vendorCFDictionaryRef ) ) {
				CFDictionaryRef productCFDictionaryRef;
				CFStringRef	productKeyCFStringRef;
				CFStringRef	vendorCFStringRef;
				
				if ( !CFDictionaryGetValueIfPresent( vendorCFDictionaryRef, kNameKeyCFStringRef, ( const void** ) &vendorCFStringRef ) ) {
					vendorCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "v: %ld" ), inVendorID );
					//CFShow( vendorCFStringRef );
					CFRelease(vendorCFStringRef);
				}
				productKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%ld" ), inProductID );
				
				if ( CFDictionaryGetValueIfPresent( vendorCFDictionaryRef, productKeyCFStringRef, ( const void** ) &productCFDictionaryRef ) ) {
					CFStringRef fullCFStringRef = NULL;
					CFStringRef	usageKeyCFStringRef;
					CFStringRef	productCFStringRef;
					CFStringRef	usageCFStringRef;
					
					if ( CFDictionaryGetValueIfPresent( productCFDictionaryRef, kNameKeyCFStringRef, ( const void** ) &productCFStringRef ) ) {
						//CFShow( productCFStringRef );
					}
					usageKeyCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%ld:%ld" ), inUsagePage, inUsage );
					
					if ( CFDictionaryGetValueIfPresent( productCFDictionaryRef, usageKeyCFStringRef, ( const void** ) &usageCFStringRef ) ) {
#if VERBOSE_ELEMENT_NAMES
						fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@ %@ %@" ),
																	vendorCFStringRef, productCFStringRef, usageCFStringRef );
#else
						fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@" ), usageCFStringRef );
#endif // VERBOSE_ELEMENT_NAMES
						// CFShow( usageCFStringRef );
					}
#if FAKE_MISSING_NAMES
					else {
						fullCFStringRef = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@ %@ #%@" ), vendorCFStringRef, productCFStringRef, usageKeyCFStringRef );
					}
#endif // FAKE_MISSING_NAMES
					if ( fullCFStringRef ) {
						// CFShow( fullCFStringRef );
						results = CFStringGetCString( fullCFStringRef, outCStr, CFStringGetLength( fullCFStringRef ) * sizeof( UniChar ) + 1, kCFStringEncodingMacRoman );
						CFRelease( fullCFStringRef );
					}
					CFRelease( usageKeyCFStringRef );
				}
				CFRelease( productKeyCFStringRef );
			}
			CFRelease( vendorKeyCFStringRef );
		}
		//++CFRelease( gUsageCFPropertyListRef );	// Leak this!
	}
	return results;
}	// hu_XMLSearchForElementNameByUsage



/*************************************************************************
*
* HIDPrintElement( stream, inElement )
*
* Purpose:  printf out all of an elements information
*
* Inputs:   stream      - FILE* to which to write output
*           inElement	- the element
*
* Returns:  int			- number of char's output
*/
int HIDPrintElement( FILE* stream, const hu_element_t* inElement )
{
	int results;
	int count;
	
	fprintf( stream, "\n" );
	
	if ( gDepth != inElement->depth )
		fprintf( stream, "%d", gDepth );
	for ( count = 0;count < inElement->depth;count++ )
		fprintf( stream, " | " );
	
#if 0	// this is verbose
	results = fprintf( stream, "-HIDPrintElement = {name: \"%s\", t: 0x%.2lX, u:%ld:%ld, c: %ld, min/max: %ld/%ld, " \
					  "scaled: %ld/%ld, size: %ld, rel: %s, wrap: %s, nonLinear: %s, preferred: %s, nullState: %s, "  \
					  "units: %ld, exp: %ld, cal: %ld/%ld, user: %ld/%ld, depth: %ld}.",
					  inElement->name, 						// name of element( c string )
					  inElement->type, 						// the type defined by IOHIDElementType in IOHIDKeys.h
					  inElement->usagePage, 					// usage page from IOUSBHIDParser.h which defines general usage
					  inElement->usage, 						// usage within above page from IOUSBHIDParser.h which defines specific usage
					  ( long ) inElement->cookie, 			// unique value( within device of specific vendorID and productID ) which identifies element, will NOT change
					  inElement->min, 						// reported min value possible
					  inElement->max, 						// reported max value possible
					  inElement->scaledMin, 					// reported scaled min value possible
					  inElement->scaledMax, 					// reported scaled max value possible
					  inElement->size, 						// size in bits of data return from element
					  inElement->relative ? "YES" : "NO", 	// are reports relative to last report( deltas )
					  inElement->wrapping ? "YES" : "NO", 	// does element wrap around( one value higher than max is min )
					  inElement->nonLinear ? "YES" : "NO", 	// are the values reported non-linear relative to element movement
					  inElement->preferredState ? "YES" : "NO", // does element have a preferred state( such as a button )
					  inElement->nullState ? "YES" : "NO", 	// does element have null state
					  inElement->units, 						// units value is reported in( not used very often )
					  inElement->unitExp, 					// exponent for units( also not used very often )
					  inElement->minReport, 					// min returned value( for calibrate call )
					  inElement->maxReport, 					// max returned value
					  inElement->userMin, 					// user set min to scale to( for scale call )
					  inElement->userMax, 					// user set max
					  inElement->depth
					  );
#else	// this is brief
	results = fprintf( stream, "-HIDPrintElement = {t: 0x%lX, u:%ld:%ld, c: %ld, name: \"%s\", d: %ld}.",
					  inElement->type, 				// the type defined by IOHIDElementType in IOHIDKeys.h
					  inElement->usagePage, 			// usage page from IOUSBHIDParser.h which defines general usage
					  inElement->usage, 				// usage within above page from IOUSBHIDParser.h which defines specific usage
					  ( long ) inElement->cookie, 		// unique value( within device of specific vendorID and productID ) which identifies element, will NOT change
					  inElement->name, 				// name of element( c string )
					  inElement->depth
					  );
#endif
	fflush( stream );
	return results;
}

/*************************************************************************
*
* HIDIsValidDevice( inSearchDevice )
*
* Purpose:  validate this device
*
* Inputs:   inSearchDevice   - the device
*
* Returns:  Boolean			- TRUE if we find the device in our( internal ) device list
*/

Boolean HIDIsValidDevice( const hu_device_t* inSearchDevice )
{
	if (inSearchDevice == NULL)
		return FALSE;
	
	hu_device_t* tDevice = gDeviceList;
	
	while ( tDevice ) {
		if ( tDevice == inSearchDevice )
			return TRUE;
		tDevice = tDevice->pNext;
	}
	return FALSE;
}

/*************************************************************************
*
* HIDIsValidElement( inSearchDevice, inSearchElement )
*
* Purpose:  validate this element
*
* Inputs:   inSearchDevice   - the device
*			inSearchElement  - the element
*
* Returns:  Boolean			- TRUE if this is a valid element pointer for this device
*/
Boolean HIDIsValidElement( const hu_device_t* inSearchDevice, const hu_element_t* inSearchElement )
{
	if ( HIDIsValidDevice( inSearchDevice ) ) {
		hu_element_t* tElement = HIDGetFirstDeviceElement( inSearchDevice, kHIDElementTypeAll );
		while ( tElement ) {
			if ( tElement == inSearchElement )
				return TRUE;
			tElement = HIDGetNextDeviceElement( tElement, kHIDElementTypeAll );
		}
	}
	return FALSE;
}

/*************************************************************************
*
* HIDSetHotPlugCallback( inHotPlugCallbackProcPtr )
*
* Purpose:  set's client hot plug callback routine
*
* Inputs:   inHotPlugCallbackProcPtr   - the routine to be called when a device is plug in
*
* Returns:  SInt32		- error code ( if any )
*/
OSStatus HIDSetHotPlugCallback(HotPlugCallbackProcPtr inAddCallbackPtr,
							   HotUnplugCallbackProcPtr inRemovalCallbackPtr)
{
#if USE_HOTPLUGGING
	gHotPlugAddCallbackPtr = inAddCallbackPtr;
	gHotPlugRemovalCallbackPtr = inRemovalCallbackPtr;
#endif
	return noErr;
}

#pragma mark - Hotplug / Notifications Callbacks

#if USE_HOTPLUGGING
/*************************************************************************
* Purpose:  This routine is the callback for our 
*           IOServiceAddMatchingNotification in hu_CreateMultiTypeDeviceList.
*           Called when a new device is plugged in.
* Inputs:   inDeviceListHead    - the address of our device list
*			inIODeviceIterator  - IO device iterator
*/
static void hu_HotPlugAddNotification( void *inDeviceListHead, 
                                       io_iterator_t inIODeviceIter )
{
	OSStatus status = noErr;
	if ( noErr == status )
		hu_AddDevices((hu_device_t**)inDeviceListHead, inIODeviceIter, TRUE);
}
#endif // USE_HOTPLUGGING

#if USE_NOTIFICATIONS
/*************************************************************************
* Purpose:  callback for kIOGeneralInterest notifications
* Notes:	notification installed( IOServiceAddInterestNotification ) in hu_AddDevices
*			This routine will get called whenever any kIOGeneralInterest 
*           notification happens. We are interested in the
*           kIOMessageServiceIsTerminated message so that's what we look for. 
*			Other messages are defined in IOMessage.h.
* Inputs:   inRefCon		- refcon passed when the notification was installed.
*			inService		- IOService whose state has changed.
*			inMessageType	- A inMessageType enum, defined by IOKit/IOMessage.h
*                               or by the IOService's family.
*			inMsgArg        - An arg for the message, dependent on inMessageType.
*/
static void hu_RemovalNotification(void *inRefCon, 
                                   io_service_t inService, 
                                   natural_t inMessageType, 
                                   void *inMsgArg)
{
	if ( kIOMessageServiceIsTerminated == inMessageType ) 
	{
		hu_device_t* tDevice = ( hu_device_t* ) inRefCon;
		
		fprintf(stderr, "libndofdev: device 0x%08x \"%s\" removed.\n", 
		        inService, tDevice->product );

		// give a chance to client to do cleanup with the removed device
		if (gHotPlugRemovalCallbackPtr && tDevice) 
			gHotPlugRemovalCallbackPtr(tDevice);
		
		// Free the data we're no longer using now that the device is going away
		hu_DisposeDevice( tDevice );
	}
}
#else
/*************************************************************************
* Purpose:  callback for device removal notifications
*
* Notes:	removal notification installed( setRemovalCallback ) in hu_AddDevices
*			This routine will get called whenever a device is removed( unplugged ).
*
* Inputs:   target			- pointer to your data, often a pointer to an object.
*			inResult		- Completion result of desired operation.
*			inRefCon		- pointer to more data.
*			inSender		- Interface instance sending the completion routine.
*/
static void hu_RemovalCallbackFunction( void* target, IOReturn inResult, void* inRefCon, void* inSender )
{
	hu_DisposeDevice( ( hu_device_t* ) target );
}
#endif // USE_NOTIFICATIONS



/*****************************************************/
#pragma mark - local ( static ) function implementations
/*****************************************************/

/*************************************************************************
*
* hu_MapChar( c )
*
* Purpose:  Maps bad chars to good chars for html/printing ASCII
*
* Inputs:   c				- the( bad? ) characater
*
* Returns:  char			- the good characater
*/

static char hu_MapChar( char c )
{
	unsigned char uc = ( unsigned char ) c;
	
	switch( uc ) {
		case '/': return '-'; // use dash instead of slash
			
		case 0x7F: return ' ';
		case 0x80: return 'A';
		case 0x81: return 'A';
		case 0x82: return 'C';
		case 0x83: return 'E';
		case 0x84: return 'N';
		case 0x85: return 'O';
		case 0x86: return 'U';
		case 0x87: return 'a';
		case 0x88: return 'a';
		case 0x89: return 'a';
		case 0x8A: return 'a';
		case 0x8B: return 'a';
		case 0x8C: return 'a';
		case 0x8D: return 'c';
		case 0x8E: return 'e';
		case 0x8F: return 'e';
		case 0x90: return ' ';
		case 0x91: return ' '; // ? '
		case 0x92: return ' '; // ? '
		case 0x93: return ' '; // ? "
		case 0x94: return ' '; // ? "
		case 0x95: return ' ';
		case 0x96: return ' ';
		case 0x97: return ' ';
		case 0x98: return ' ';
		case 0x99: return ' ';
		case 0x9A: return ' ';
		case 0x9B: return 0x27;
		case 0x9C: return 0x22;
		case 0x9D: return ' ';
		case 0x9E: return ' ';
		case 0x9F: return ' ';
		case 0xA0: return ' ';
		case 0xA1: return ' ';
		case 0xA2: return ' ';
		case 0xA3: return ' ';
		case 0xA4: return ' ';
		case 0xA5: return ' ';
		case 0xA6: return ' ';
		case 0xA7: return ' ';
		case 0xA8: return ' ';
		case 0xA9: return ' ';
		case 0xAA: return ' ';
		case 0xAB: return ' ';
		case 0xAC: return ' ';
		case 0xAD: return ' ';
		case 0xAE: return ' ';
		case 0xAF: return ' ';
		case 0xB0: return ' ';
		case 0xB1: return ' ';
		case 0xB2: return ' ';
		case 0xB3: return ' ';
		case 0xB4: return ' ';
		case 0xB5: return ' ';
		case 0xB6: return ' ';
		case 0xB7: return ' ';
		case 0xB8: return ' ';
		case 0xB9: return ' ';
		case 0xBA: return ' ';
		case 0xBB: return ' ';
		case 0xBC: return ' ';
		case 0xBD: return ' ';
		case 0xBE: return ' ';
		case 0xBF: return ' ';
		case 0xC0: return ' ';
		case 0xC1: return ' ';
		case 0xC2: return ' ';
		case 0xC3: return ' ';
		case 0xC4: return ' ';
		case 0xC5: return ' ';
		case 0xC6: return ' ';
		case 0xC7: return ' ';
		case 0xC8: return ' ';
		case 0xC9: return ' ';
		case 0xCA: return ' ';
		case 0xCB: return 'A';
		case 0xCC: return 'A';
		case 0xCD: return 'O';
		case 0xCE: return ' ';
		case 0xCF: return ' ';
		case 0xD0: return '-';
		case 0xD1: return '-';
		case 0xD2: return 0x22;
		case 0xD3: return 0x22;
		case 0xD4: return 0x27;
		case 0xD5: return 0x27;
		case 0xD6: return '-'; // use dash instead of slash
		case 0xD7: return ' ';
		case 0xD8: return 'y';
		case 0xD9: return 'Y';
		case 0xDA: return '-'; // use dash instead of slash
		case 0xDB: return ' ';
		case 0xDC: return '<';
		case 0xDD: return '>';
		case 0xDE: return ' ';
		case 0xDF: return ' ';
		case 0xE0: return ' ';
		case 0xE1: return ' ';
		case 0xE2: return ',';
		case 0xE3: return ',';
		case 0xE4: return ' ';
		case 0xE5: return 'A';
		case 0xE6: return 'E';
		case 0xE7: return 'A';
		case 0xE8: return 'E';
		case 0xE9: return 'E';
		case 0xEA: return 'I';
		case 0xEB: return 'I';
		case 0xEC: return 'I';
		case 0xED: return 'I';
		case 0xEE: return 'O';
		case 0xEF: return 'O';
		case 0xF0: return ' ';
		case 0xF1: return 'O';
		case 0xF2: return 'U';
		case 0xF3: return 'U';
		case 0xF4: return 'U';
		case 0xF5: return '|';
		case 0xF6: return ' ';
		case 0xF7: return ' ';
		case 0xF8: return ' ';
		case 0xF9: return ' ';
		case 0xFA: return '.';
		case 0xFB: return ' ';
		case 0xFC: return ' ';
		case 0xFD: return 0x22;
		case 0xFE: return ' ';
		case 0xFF: return ' ';
	}
	return c;
}

/*************************************************************************
*
* hu_CleanString( inCStr )
*
* Purpose:  ensures the string only contains printable ASCII characters
* Notes:	input is null terminated( C ) string, change is made in place
*
* Inputs:   inCStr	- the address of the C string
*
* Returns:  nothing
*/

static void hu_CleanString( char* inCStr )
{
	char* charIt = inCStr;
	while ( *charIt ) {
		*charIt = hu_MapChar( *charIt );
		charIt++;
	}
}

/*************************************************************************
*
* hu_GetElementInfo( inElementCFDictRef, inElement )
*
* Purpose:  extracts element information from an elements CF dictionary
*			into a element data structure
* Notes:	only called by hu_AddElement
*
* Inputs:   inElementCFDictRef   - the elements dictionary
*			inElement			- pointer to elements data structure
*
* Returns:  nothing
*/

static void hu_GetElementInfo( CFTypeRef inElementCFDictRef, hu_element_t* inElement )
{
	long number;
	CFTypeRef tCFTypeRef;
	
	// type, inUsagePage, usage already stored
	
	// get the cookie
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementCookieKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->cookie = (void*)(size_t)( IOHIDElementCookie ) number;
	} else {
		inElement->cookie = ( IOHIDElementCookie ) 0;
	}
	
	// get the min
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementMinKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->min = number;
	} else {
		inElement->min = 0;
	}
	
	inElement->maxReport = inElement->min;
	inElement->userMin = kDefaultUserMin;
	
	// get the max
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementMaxKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->max = number;
	} else {
		inElement->max = 0;
	}
	
	inElement->minReport = inElement->max;
	inElement->userMax = kDefaultUserMax;
	
	// get the scaled min
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementScaledMinKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->scaledMin = number;
	} else {
		inElement->scaledMin = 0;
	}
	
	// get the scaled max
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementScaledMaxKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->scaledMax = number;
	} else {
		inElement->scaledMax = 0;
	}
	
	// get the size
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementSizeKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->size = number;
	} else {
		inElement->size = 0;
	}
	
	// get the "is relative" boolean
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementIsRelativeKey ) );
	if ( tCFTypeRef ) {
		inElement->relative = CFBooleanGetValue( tCFTypeRef );
	} else {
		inElement->relative = 0;
	}
	
	// get the "is wrapping" boolean
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementIsWrappingKey ) );
	if ( tCFTypeRef ) {
		inElement->wrapping = CFBooleanGetValue( tCFTypeRef );
	} else {
		inElement->wrapping = FALSE;
	}
	
	// get the "is non linear" boolean
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementIsNonLinearKey ) );
	if ( tCFTypeRef ) {
		inElement->nonLinear = CFBooleanGetValue( tCFTypeRef );
	} else {
		inElement->nonLinear = FALSE;
	}
	
	// get the "Has Preferred State" boolean
#ifdef kIOHIDElementHasPreferredStateKey
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementHasPreferredStateKey ) );
#else // Mac OS X 10.0 has spelling error
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementHasPreferedStateKey ) );
#endif
	if ( tCFTypeRef ) {
		inElement->preferredState = CFBooleanGetValue( tCFTypeRef );
	} else {
		inElement->preferredState = FALSE;
	}
	
	// get the "Has Null State" boolean
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementHasNullStateKey ) );
	if ( tCFTypeRef ) {
		inElement->nullState = CFBooleanGetValue( tCFTypeRef );
	} else {
		inElement->nullState = FALSE;
	}
	
	// get the units
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementUnitKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->units = number;
	} else {
		inElement->units = 0;
	}
	
	// get the units exponent
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementUnitExponentKey ) );
	if ( tCFTypeRef && CFNumberGetValue( tCFTypeRef, kCFNumberLongType, &number ) ) {
		inElement->unitExp = number;
	} else {
		inElement->unitExp = 0;
	}
	
	// get the name
	tCFTypeRef = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementNameKey ) );
	if ( tCFTypeRef ) {
		if ( !CFStringGetCString( tCFTypeRef, inElement->name, 256, kCFStringEncodingUTF8 ) ) {
			HIDReportError( "hu_GetElementInfo: CFStringGetCString error retrieving inElement->name." );
		}
	}
	if ( !*inElement->name ) {
		// set name from vendor id, product id & usage info look up
		if ( !HIDGetElementNameFromVendorProductUsage( gCurrentDevice->vendorID, gCurrentDevice->productID, inElement->usagePage, inElement->usage, inElement->name ) ) {
			// set name from vendor id/product id look up
			HIDGetElementNameFromVendorProductCookie( gCurrentDevice->vendorID, gCurrentDevice->productID, ( long ) inElement->cookie, inElement->name );
			if ( !*inElement->name ) { // if no name
				HIDGetUsageName( inElement->usagePage, inElement->usage, inElement->name );
				if ( !*inElement->name ) // if not usage
					sprintf( inElement->name, "Element" );
			}
		}
	}
}

/*************************************************************************
*
* hu_AddElement( inElementCFDictRef, inCurrentElement )
*
* Purpose:  examines CF dictionary vlaue in device element hierarchy to determine
*			if it is element of interest or a collection of more elements.
*			if element of interest allocate storage, add to list and retrieve element specific info.
*			if collection then pass on to hu_GetCollectionElements to deconstruct into additional individual elements.
* Notes:	only called by hu_GetElementsCFArrayHandler
*
* Inputs:   inElementCFDictRef   - the elements dictionary
*			inCurrentElement	- pointer to address of current elements structure
*
* Returns:  nothing
*/

static void hu_AddElement( CFTypeRef inElementCFDictRef, hu_element_t **inCurrentElement )
{
	hu_device_t*	tDevice			= gCurrentDevice;
	hu_element_t*   tElement		= NULL;
	CFTypeRef		tElementCFDictRefType  = CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementTypeKey ) );
	CFTypeRef		refUsagePage	= CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementUsagePageKey ) );
	CFTypeRef		refUsage		= CFDictionaryGetValue( inElementCFDictRef, CFSTR( kIOHIDElementUsageKey ) );
	long			elementType = 0, usagePage = 0, usage = 0;
	
    if ( tElementCFDictRefType ) {
		CFNumberGetValue( tElementCFDictRefType, kCFNumberLongType, &elementType );
    }
    if ( refUsagePage ) {
 		CFNumberGetValue( refUsagePage, kCFNumberLongType, &usagePage );
    }
    if ( refUsage ) {
		CFNumberGetValue( refUsage, kCFNumberLongType, &usage );
    }
    if ( !tDevice ) {
		return;
    }
	if ( elementType ) {
		// look at types of interest
		if ( elementType != kIOHIDElementTypeCollection ) {
			if ( usagePage && usage ) {	// if valid usage and page
				switch( usagePage ) {	// only interested in kHIDPage_GenericDesktop and kHIDPage_Button usage pages
					case kHIDPage_GenericDesktop: {
						switch( usage ) {	// look at usage to determine function
							case kHIDUsage_GD_X:
							case kHIDUsage_GD_Y:
							case kHIDUsage_GD_Z:
							case kHIDUsage_GD_Rx:
							case kHIDUsage_GD_Ry:
							case kHIDUsage_GD_Rz:
								tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
								if ( tElement ) tDevice->axis++;
									break;
							case kHIDUsage_GD_Slider:
								tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
								if ( tElement ) tDevice->sliders++;
									break;
							case kHIDUsage_GD_Dial:
								tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
								if ( tElement ) tDevice->dials++;
									break;
							case kHIDUsage_GD_Wheel:
								tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
								if ( tElement ) tDevice->wheels++;
									break;
							case kHIDUsage_GD_Hatswitch:
								tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
								if ( tElement ) tDevice->hats++;
									break;
							default:
								tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
								break;
						}
					}
						break;
					case kHIDPage_Button:
						tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
						if ( tElement ) tDevice->buttons++;
							break;
					default:
						// just add a generic element
						tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
						break;
				}
			}
#if 0		// ignore these errors
			else {
				HIDReportError( "hu_AddElement: CFNumberGetValue error when getting value for refUsage or refUsagePage." );
			}
#endif
		} else {	// collection
			tElement = ( hu_element_t* ) malloc( sizeof( hu_element_t ) );
		}
	} else {
		HIDReportError( "hu_AddElement: CFNumberGetValue error when getting value for tElementCFDictRefType." );
	}
	
	if ( tElement ) {	// add to list
		
		// this code builds a binary tree based on the collection hierarchy of inherent in the device element layout
		// it preserves the structure of the elements as collections have children and elements are siblings to each other
		
		// clear record
		bzero( tElement, sizeof( hu_element_t ) );
		
		// get element info
		tElement->type = elementType;
		tElement->usagePage = usagePage;
		tElement->usage = usage;
		tElement->depth = 0;		// assume root object
		
		// extract element information from the CF dictionary into a element data structure
		hu_GetElementInfo( inElementCFDictRef, tElement );
		
		// count elements
		tDevice->totalElements++;
		
		switch( tElement->type ) {
			case kIOHIDElementTypeInput_Misc:
			case kIOHIDElementTypeInput_Button:
			case kIOHIDElementTypeInput_Axis:
			case kIOHIDElementTypeInput_ScanCodes:
				tDevice->inputs++;
				break;
			case kIOHIDElementTypeOutput:
				tDevice->outputs++;
				break;
			case kIOHIDElementTypeFeature:
				tDevice->features++;
				break;
			case kIOHIDElementTypeCollection:
				tDevice->collections++;
				break;
			default:
				HIDReportErrorNum( "\nhu_AddElement: Unknown element type: %d", tElement->type );
				break;
		}
		
		if ( !*inCurrentElement ) {	// if at list head
			tDevice->pListElements = tElement; // add current element
			*inCurrentElement = tElement; // set current element to element we just added
		} else {	// have exsiting structure
			if ( gAddAsChild ) {	// if the previous element was a collection, let's add this as a child of the previous
									// this iteration should not be needed but there maybe some untested degenerate case which this code will ensure works
				while ( ( *inCurrentElement )->pChild ) {	// step down tree until free child node found
					*inCurrentElement = ( *inCurrentElement )->pChild;
				}
				
				( *inCurrentElement )->pChild = tElement; // insert there
				tElement->depth = ( *inCurrentElement )->depth + 1;
			} else { // add as sibling
					 // this iteration should not be needed but there maybe some untested degenerate case which this code will ensure works
				while ( ( *inCurrentElement )->pSibling ) {	// step down tree until free sibling node found
					*inCurrentElement = ( *inCurrentElement )->pSibling;
				}
				( *inCurrentElement )->pSibling = tElement; // insert there
				tElement->depth = ( *inCurrentElement )->depth;
			}
			tElement->pPrevious = *inCurrentElement; // point to previous
			*inCurrentElement = tElement; // set current to our collection
		}
		
		// if a type that is normally an axis and has a preferred state
		tElement->hasCenter = FALSE;
		
		if ( ( tElement->preferredState ) && ( kHIDPage_GenericDesktop == tElement->usagePage ) ) {
			switch( tElement->usage ) {
				case kHIDUsage_GD_X:
				case kHIDUsage_GD_Y:
				case kHIDUsage_GD_Z:
				case kHIDUsage_GD_Rx:
				case kHIDUsage_GD_Ry:
				case kHIDUsage_GD_Rz:
				case kHIDUsage_GD_Slider:
				case kHIDUsage_GD_Dial:
				case kHIDUsage_GD_Wheel:
				case kHIDUsage_GD_Hatswitch:
					tElement->hasCenter = TRUE; // respect center
					tElement->initialCenter = 0x80000000; // HIDGetElementValue( tDevice, tElement );
			}
		}
		
#ifdef LOG_ELEMENTS
		HIDPrintElement( LOG_ELEMENTS, tElement );
#endif // LOG_ELEMENTS
		
		if ( kIOHIDElementTypeCollection == elementType ) { // if this element is a collection of other elements
			gAddAsChild = TRUE; // add next set as children to this element
			gDepth++;
			hu_GetCollectionElements( inElementCFDictRef, &tElement ); // recursively process the collection
			gDepth--;
		}
		gAddAsChild = FALSE; // add next as this elements sibling( when return from a collection or with non-collections )
	}
#if 0		// ignore these errors
	else {
		HIDReportError( "hu_AddElement: no element added." );
	}
#endif
}

/*************************************************************************
*
* hu_GetElementsCFArrayHandler( value, parameter )
*
* Purpose:  the CFArrayApplierFunction called by the CFArrayApplyFunction in hu_GetElements
*
* Inputs:   value			- the value from the array
*			parameter		- the context passed to CFArrayApplyFunction( in our case, inCurrentElement )
*
* Returns:  nothing
*/

static void hu_GetElementsCFArrayHandler( const void* value, void* parameter )
{
	if ( CFGetTypeID( value ) == CFDictionaryGetTypeID( ) ) {
		hu_AddElement( ( CFTypeRef ) value, ( hu_element_t ** ) parameter );
	}
}

/*************************************************************************
*
* hu_GetElements( inElementCFDictRef, outCurrentElement )
*
* Purpose:  handles retrieval of element information from arrays of elements in device IO registry information
*
* Notes:	Only called by hu_GetCollectionElements
*
* Inputs:   inElementCFDictRef   - the I/O registry dictionary of all elements for a device
*			outCurrentElement	- adress where to store new elements
*
* Returns:  nothing
*/

static void hu_GetElements( CFTypeRef inElementCFDictRef, hu_element_t **outCurrentElement )
{
	CFTypeID tCFTypeID = CFGetTypeID( inElementCFDictRef );
	if ( tCFTypeID == CFArrayGetTypeID( ) ) {	// if element is an array
		CFRange range = {0, CFArrayGetCount( inElementCFDictRef )};
		// CountElementsCFArrayHandler called for each array member
		CFArrayApplyFunction( inElementCFDictRef, range, hu_GetElementsCFArrayHandler, outCurrentElement );
	}
}

/*************************************************************************
*
* hu_GetCollectionElements( deviceProperties, outCurrentCollection )
*
* Purpose:  handles extracting element information from element collection CF types
*
* Notes:	used from top level element decoding and hierarchy deconstruction to flatten device element list.
*			Called by hu_AddElement and hu_BuildDevice.
*
* Inputs:   deviceProperties	- the I/O registry dictionary for the device
*			outCurrentCollection  - adress where to store new elements
*
* Returns:  nothing
*/

static void hu_GetCollectionElements( CFDictionaryRef deviceProperties, hu_element_t **outCurrentCollection )
{
	CFTypeRef inElementCFDictRef = CFDictionaryGetValue( deviceProperties, CFSTR( kIOHIDElementKey ) );
	if ( inElementCFDictRef ) {
		hu_GetElements( inElementCFDictRef, outCurrentCollection );
	} else {
		HIDReportError( "hu_GetCollectionElements: CFDictionaryGetValue error when creating CFTypeRef for kIOHIDElementKey." );
	}
}

/*************************************************************************
*
* hu_TopLevelElementHandler( value, parameter )
*
* Purpose:  use top level element usage page and usage to discern device usage page and usage setting appropriate values in device record
*
* Notes:	callback for CFArrayApplyFunction in hu_GetDeviceInfo
*
* Inputs:   value			- the value from the array( should be a CFDictionaryRef )
*			parameter		- the context passed to CFArrayApplyFunction( in our case, NULL )
*
* Returns:  nothing
*/

static void hu_TopLevelElementHandler( const void* value, void* parameter )
{
	CFTypeRef refCF = 0;
	if ( value &&  parameter ) {
		if ( CFGetTypeID( value ) == CFDictionaryGetTypeID( ) ) {
			refCF = CFDictionaryGetValue( value, CFSTR( kIOHIDElementUsagePageKey ) );
			if ( !CFNumberGetValue( refCF, kCFNumberLongType, & ( ( hu_device_t* ) parameter )->usagePage ) ) {
				HIDReportError( "hu_TopLevelElementHandler: CFNumberGetValue error retrieving inDevice->usagePage." );
			}
			refCF = CFDictionaryGetValue( value, CFSTR( kIOHIDElementUsageKey ) );
			if ( !CFNumberGetValue( refCF, kCFNumberLongType, & ( ( hu_device_t* ) parameter )->usage ) ) {
				HIDReportError( "hu_TopLevelElementHandler: CFNumberGetValue error retrieving inDevice->usage." );
			}
		}
	}
}

/*************************************************************************
*
* hu_GetDeviceInfo( inHIDDevice, inDeviceCFDictionaryRef, inDevice )
*
* Purpose:  extracts device info from dictionary records in IO registry
*
* Notes:	only called by hu_BuildDevice
*
* Inputs:   inHIDDevice				- our HID object
*			inDeviceCFDictionaryRef  - dictionary properties for this device
*			inDevice					- address where to store device properties
*
* Returns:  nothing
*/

static void hu_GetDeviceInfo( io_object_t inHIDDevice, CFDictionaryRef inDeviceCFDictionaryRef, hu_device_t* inDevice )
{
	CFMutableDictionaryRef usbProperties = nil;
	io_registry_entry_t parent1, parent2;
	kern_return_t rc;

	// Mac OS X currently is not mirroring all USB properties to HID page so need to look at USB device page also
	// get dictionary for usb properties: step up two levels and get CF dictionary for USB properties
	// try to get parent1
	rc = IORegistryEntryGetParentEntry( inHIDDevice, kIOServicePlane, &parent1 );
	if ( KERN_SUCCESS == rc ) {
		// got parent1, try for parent2
		rc = IORegistryEntryGetParentEntry( parent1, kIOServicePlane, &parent2 );
		// either way, release parent1
		if ( kIOReturnSuccess != IOObjectRelease( parent1 ) ) {
			HIDReportError( "hu_GetDeviceInfo: IOObjectRelease error with parent1." );
		}
		if ( KERN_SUCCESS == rc ) {
			// got parent2, try for usbProperties
			rc = IORegistryEntryCreateCFProperties( parent2, &usbProperties, kCFAllocatorDefault, kNilOptions );
			// either way, release parent2
			if ( kIOReturnSuccess != IOObjectRelease( parent2 ) ) {
				HIDReportError( "hu_GetDeviceInfo: IOObjectRelease error with parent2." );
			}
		}
	}
	if ( KERN_SUCCESS == rc )
	{
		// IORegistryEntryCreateCFProperties() succeeded
		if ( usbProperties != nil ) {
			CFTypeRef refCF = 0;
			// get device info
			// try hid dictionary first, if fail then go to usb dictionary
			
			// get transport
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDTransportKey ) );
			if ( refCF ) {
				if ( !CFStringGetCString( refCF, inDevice->transport, 256, kCFStringEncodingUTF8 ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFStringGetCString error retrieving inDevice->transport." );
				}
				hu_CleanString( inDevice->transport );
			}
			
			// get vendorID
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDVendorIDKey ) );
			if ( !refCF ) {
				refCF = CFDictionaryGetValue( usbProperties, CFSTR( "idVendor" ) );
			}
			if ( refCF ) {
				if ( !CFNumberGetValue( refCF, kCFNumberLongType, &inDevice->vendorID ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFNumberGetValue error retrieving inDevice->vendorID." );
				}
			}
			
			// get product ID
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDProductIDKey ) );
			if ( !refCF ) {
				refCF = CFDictionaryGetValue( usbProperties, CFSTR( "idProduct" ) );
			}
			if ( refCF ) {
				if ( !CFNumberGetValue( refCF, kCFNumberLongType, &inDevice->productID ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFNumberGetValue error retrieving inDevice->productID." );
				}
			}
			
			// get product version
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDVersionNumberKey ) );
			if ( refCF ) {
				if ( !CFNumberGetValue( refCF, kCFNumberLongType, &inDevice->version ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFNumberGetValue error retrieving inDevice->version." );
				}
			}
			
			// get manufacturer name
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDManufacturerKey ) );
			if ( !refCF ) {
				refCF = CFDictionaryGetValue( usbProperties, CFSTR( "USB Vendor Name" ) );
			}
			if ( refCF ) {
				if ( !CFStringGetCString( refCF, inDevice->manufacturer, 256, kCFStringEncodingUTF8 ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFStringGetCString error retrieving inDevice->manufacturer." );
				}
				hu_CleanString( inDevice->manufacturer );
			}
			
			// get product name
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDProductKey ) );
			if ( !refCF ) {
				refCF = CFDictionaryGetValue( usbProperties, CFSTR( "USB Product Name" ) );
			}
			if ( refCF ) {
				if ( !CFStringGetCString( refCF, inDevice->product, 256, kCFStringEncodingUTF8 ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFStringGetCString error retrieving inDevice->product." );
				}
				hu_CleanString( inDevice->product );
			}
			
			// get serial
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDSerialNumberKey ) );
			if ( refCF ) {
				if ( !CFStringGetCString( refCF, inDevice->serial, 256, kCFStringEncodingUTF8 ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFStringGetCString error retrieving inDevice->serial." );
				}
				hu_CleanString( inDevice->serial );
			}
			
			// get location ID
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDLocationIDKey ) );
			if ( !refCF ) {
				refCF = CFDictionaryGetValue( usbProperties, CFSTR( "locationID" ) );
			}
			if ( refCF ) {
				if ( !CFNumberGetValue( refCF, kCFNumberLongType, &inDevice->locID ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFNumberGetValue error retrieving inDevice->locID." );
				}
			}
			
			// get usage page and usage
			refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDPrimaryUsagePageKey ) );
			if ( refCF ) {
				if ( !CFNumberGetValue( refCF, kCFNumberLongType, &inDevice->usagePage ) ) {
					HIDReportError( "hu_GetDeviceInfo: CFNumberGetValue error retrieving inDevice->usagePage." );
				}
				refCF = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDPrimaryUsageKey ) );
				if ( refCF )
					if ( !CFNumberGetValue( refCF, kCFNumberLongType, &inDevice->usage ) ) {
						HIDReportError( "hu_GetDeviceInfo: CFNumberGetValue error retrieving inDevice->usage." );
					}
			}
			if ( !refCF ) { // get top level element HID usage page or usage
							// use top level element instead
				CFTypeRef refCFTopElement = 0;
				refCFTopElement = CFDictionaryGetValue( inDeviceCFDictionaryRef, CFSTR( kIOHIDElementKey ) );
				{
					// refCFTopElement points to an array of element dictionaries
					CFRange range = {0, CFArrayGetCount( refCFTopElement )};
					CFArrayApplyFunction( refCFTopElement, range, hu_TopLevelElementHandler, NULL );
				}
			}
			CFRelease(usbProperties);
		} else {
			HIDReportError( "hu_GetDeviceInfo: IORegistryEntryCreateCFProperties failed to create usbProperties." );
		}
	}
}

/*************************************************************************
*
* hu_AddDevice( inDeviceListHead, inNewDevice )
*
* Purpose:  adds device to linked list of devices passed in
*
* Notes:	handles NULL lists properly
*
* Inputs:   inDeviceListHead - the head of the device list
*			inNewDevice		- the new device
*
* Returns:  hu_device_t**	- address where it was added to list
*/

static hu_device_t **hu_AddDevice( hu_device_t **inDeviceListHead, hu_device_t* inNewDevice )
{
	hu_device_t **result = NULL;
	
	if ( !*inDeviceListHead ) {
		result = inDeviceListHead;
	} else {
		hu_device_t *previousDevice = NULL, *tDevice = *inDeviceListHead;
		while ( tDevice ) {
			previousDevice = tDevice;
			tDevice = previousDevice->pNext;
		}
		result = &previousDevice->pNext;
	}
    
    // insert at end of list
	inNewDevice->pNext = NULL;
	*result = inNewDevice;
    
	return result;
}

/*************************************************************************
*
* hu_MoveDevice( inDeviceListHead, inNewDevice, inOldListDeviceHead )
*
* Purpose:  moves a device from one list to another
*
* Notes:	handles NULL lists properly
*
* Inputs:   inDeviceListHead - the head of the( new ) device list
*			inNewDevice		- the device
*			inOldListDeviceHead  - the head of the old device list
*
* Returns:  hu_device_t*	- next device in old list( for properly iterating )
*/

static hu_device_t* hu_MoveDevice( hu_device_t **inDeviceListHead, hu_device_t* inNewDevice, hu_device_t **inOldListDeviceHead )
{
	hu_device_t* tDeviceNext = NULL;
	if ( !inNewDevice || !inOldListDeviceHead || !inDeviceListHead ) { // handle NULL pointers
		HIDReportError( "hu_MoveDevice: NULL input error." );
		return tDeviceNext;
	}
	
	// remove from old
	if ( inNewDevice == *inOldListDeviceHead ) { // replacing head
		*inOldListDeviceHead = inNewDevice->pNext;
		tDeviceNext = *inOldListDeviceHead;
	} else {
		hu_device_t *previousDevice = NULL, *tDevice = *inOldListDeviceHead;
		while ( tDevice && ( tDevice != inNewDevice ) ) { // step through list until match or end
			previousDevice = tDevice;
			tDevice = previousDevice->pNext;
		}
		if ( tDevice == inNewDevice ) { // if there was a match
			previousDevice->pNext = tDevice->pNext; // skip this device
			tDeviceNext = tDevice->pNext;
		} else {
			HIDReportError( "hu_MoveDevice: device not found when moving." );
		}
	}
	
	// add to new list
	hu_AddDevice( inDeviceListHead, inNewDevice );
	return tDeviceNext; // return next device
}

/*************************************************************************
*
* hu_BuildDevice( inHIDDevice )
*
* Purpose:  given a IO device object build a flat device record including device info and all elements
*
* Notes:	handles NULL lists properly
*
* Inputs:   inHIDDevice		- the I/O device object
*
* Returns:  hu_device_t*	- the address of the new device record
*/

static hu_device_t* hu_BuildDevice( io_object_t inHIDDevice )
{
	hu_device_t* tDevice = ( hu_device_t* ) malloc( sizeof( hu_device_t ) );
	
	if ( tDevice ) {
		// clear our device struct
		bzero( tDevice, sizeof( hu_device_t ) );
		
		// get dictionary for the device properties
		CFMutableDictionaryRef deviceCFDictRef = 0;
		kern_return_t result = IORegistryEntryCreateCFProperties( inHIDDevice, &deviceCFDictRef, kCFAllocatorDefault, kNilOptions );
		
		if ( ( KERN_SUCCESS == result ) && deviceCFDictRef ) {
			// create device interface
			result = HIDCreateOpenDeviceInterface( inHIDDevice, tDevice );
			if ( kIOReturnSuccess != result )
				HIDReportErrorNum( "\nhu_BuildDevice: HIDCreateOpenDeviceInterface failed.", result );
			
			// extract all the device info from the device dictionary into our device struct
			//( inHIDDevice is used to find parents in registry tree )
			hu_GetDeviceInfo( inHIDDevice, deviceCFDictRef, tDevice );
			
			// set current device for use in getting elements
			gCurrentDevice = tDevice;
			
			// Add all elements
			hu_element_t* currentElement = NULL;
			hu_GetCollectionElements( deviceCFDictRef, &currentElement );
			
			gCurrentDevice = NULL;
			CFRelease( deviceCFDictRef );
		} else {
			HIDReportErrorNum( "\nhu_BuildDevice: IORegistryEntryCreateCFProperties error when creating deviceProperties.", result );
		}
	} else {
		HIDReportError( "malloc error when allocating hu_device_t*." );
	}
	return tDevice;
}

/*************************************************************************
*
* hu_CreateSingleTypeDeviceList( inHIDObjectIterator )
*
* Purpose:  build flat linked list of devices from hid object iterator
*
* Notes:	Only called by hu_CreateMultiTypeDeviceList
*
* Inputs:   inHIDObjectIterator	- the HID object iterator
*
* Returns:  hu_device_t*		- the address of the new device list head
*/

static hu_device_t* hu_CreateSingleTypeDeviceList( io_iterator_t inHIDObjectIterator )
{
	hu_device_t* deviceListHead = NULL;
	
	hu_AddDevices(&deviceListHead, inHIDObjectIterator, FALSE);
	return deviceListHead;
}

/*************************************************************************
*
* hu_CreateMultiTypeDeviceList( pUsagePages, pUsages, inNumDeviceTypes )
*
* Purpose:  build flat linked list of devices from list of usages and usagePages
*
* Notes:	Only called by HIDBuildMultiDeviceList & HIDUpdateDeviceList
*
* Inputs:   pUsagePages		- inNumDeviceTypes sized array of matching usage pages
*			pUsages			- inNumDeviceTypes sized array of matching usages
*			inNumDeviceTypes - number of usage pages & usages
*
* Returns:  hu_device_t*		- the address of the new device list head
*/

static hu_device_t* hu_CreateMultiTypeDeviceList( UInt32 *pUsagePages, UInt32 *pUsages, UInt32 inNumDeviceTypes )
{
	io_iterator_t hidObjectIterator = 0;
	hu_device_t* newDeviceList = NULL; // build new list
	UInt32 i;
	
	if ( !pUsages || !pUsagePages || !inNumDeviceTypes ) {
		HIDReportError( "hu_CreateMultiTypeDeviceList: NULL pUsages, pUsagePages or inNumDeviceTypes." );
	} else {
		for ( i = 0; i < inNumDeviceTypes; i++ ) {	// for all usage and usage page types
			hu_device_t* deviceList = NULL;
			CFMutableDictionaryRef hidMatchingCFDictRef;
			IOReturn result = kIOReturnSuccess;	// assume success( optimist! )
			
			// Set up matching dictionary to search the I/O Registry for HID devices we are interested in. Dictionary reference is NULL if error.
			hidMatchingCFDictRef = hu_SetUpMatchingDictionary( pUsagePages[i], pUsages[i] );
			if ( !hidMatchingCFDictRef )
				HIDReportError( "hu_SetUpMatchingDictionary: Couldn’t create a matching dictionary." );
			
			// BUG FIX! one reference is consumed by IOServiceGetMatchingServices
			CFRetain( hidMatchingCFDictRef );
			
			// create an IO object iterator
			result = IOServiceGetMatchingServices( kIOMasterPortDefault, hidMatchingCFDictRef, &hidObjectIterator );
			if ( kIOReturnSuccess != result ) {
				HIDReportErrorNum( "\nhu_CreateMultiTypeDeviceList: Failed to create IO object iterator, error:", result );
			} else if ( 0 == hidObjectIterator ) { // likely no HID devices which matched selection criteria are connected
				HIDReportError( "hu_CreateMultiTypeDeviceList: Could not find any matching devices, thus iterator creation failed." );
			}
			
			if ( hidObjectIterator ) {
#if USE_HOTPLUGGING
				CFRunLoopSourceRef		runLoopSource;
				IOReturn result = kIOReturnSuccess;	// assume success( optimist! )
				
				// To get async notifications we need to create a notification 
				// port, get its run loop event source and add it to the 
				// current run loop. If we don't add it to the run loop we 
				// won't get notified.
				gNotifyPort = IONotificationPortCreate( kIOMasterPortDefault );
				runLoopSource = IONotificationPortGetRunLoopSource( gNotifyPort );
				gRunLoop = CFRunLoopGetCurrent( );
				CFRunLoopAddSource( gRunLoop, runLoopSource, kCFRunLoopDefaultMode );
				
				// BUG FIX! one reference is consumed by IOServiceAddMatchingNotification
				CFRetain( hidMatchingCFDictRef );
				
				// Now set up a notification to be called when a device is
				// first matched by I/O Kit.
#if TARGET_RT_MAC_CFM
				result = IOServiceAddMatchingNotification(gNotifyPort, 
														  kIOFirstMatchNotification, 
														  hidMatchingCFDictRef,
														// Notes: This is a MachO function ptr. 
														// If you're using CFM you have to call 
														// MachOFunctionPointerForCFMFunctionPointer.
														  MachOFunctionPointerForCFMFunctionPointer( hu_HotPlugAddNotification ), 	// callback
														  &gDeviceList, 
														  &hidObjectIterator );
#else
				result = IOServiceAddMatchingNotification(gNotifyPort, 
														  kIOFirstMatchNotification, 
														  hidMatchingCFDictRef,
														  hu_HotPlugAddNotification,// callback
														  &gDeviceList, // refCon
														  &hidObjectIterator);
#endif // TARGET_RT_MAC_CFM
#endif // USE_HOTPLUGGING
				// add all existing devices
				deviceList = hu_CreateSingleTypeDeviceList( hidObjectIterator ); // build device list
				
			}
			if ( deviceList ) // if there are devices to merge
				hu_MergeDeviceList( &newDeviceList, &deviceList ); // merge into new list
			while ( deviceList ) // dump what is left of source list
				deviceList = hu_DisposeDevice( deviceList ); // dispose current device & return next device, will set deviceList to NULL
			
			// BUG FIX! we weren't releasing this
			CFRelease( hidMatchingCFDictRef );
		}
	}
	return newDeviceList;
}

/*************************************************************************
*
* hu_FindDeviceInList( inDeviceList, inFindDevice )
*
* Purpose:  given a device list and a device find if device is in list
*
* Notes:	called by hu_CompareUpdateDeviceList & hu_MergeDeviceList
*
* Inputs:   inDeviceList - the device list
*			inFindDevice - the device
*
* Returns:  Boolean		- TRUE if device in list
*/
static Boolean hu_FindDeviceInList( hu_device_t* inDeviceList, hu_device_t* inFindDevice )
{
	Boolean found = FALSE; // not found
	hu_device_t* tDevice = inDeviceList; // not really needed but is clearer this way
	while ( tDevice && !found ) {	// while we still have device to look at and have not found the target device
		if ( ( tDevice->vendorID == inFindDevice->vendorID ) && // if we match same vendor, product & location
			 ( tDevice->productID == inFindDevice->productID ) && // this is not quite right for same tyes plugged into the same location but different physical devices
			 ( tDevice->locID == inFindDevice->locID ) &&      // since this is a corner and impossible to detect without serial numbers case we will ignore it
			 ( tDevice->usage == inFindDevice->usage ) )
		{
			found = TRUE; // found device
		}
		tDevice = tDevice->pNext; // step to next device
	}
	return found;
}

/*************************************************************************
*
* hu_MergeDeviceList( inNewDeviceList, inDeviceList )
*
* Purpose:  merges two devicelist into single *inNewDeviceList
*
* Notes:	inNewDeviceList may have head device modified( such as if it is NULL ) thus pointer to pointer to device.
*			devices are matched on vendorID, productID, locID by hu_FindDeviceInList.
*			device record in inNewDeviceList maintained.
*
* Inputs:   inNewDeviceList  - the new list
*			inDeviceList		- the device list to add
*
* Returns:  nothing
*/

static void hu_MergeDeviceList( hu_device_t **inNewDeviceList, hu_device_t **inDeviceList )
{
	hu_device_t* tDevice = *inDeviceList;
	while ( tDevice ) { // for all the devices in old list
		Boolean present = hu_FindDeviceInList( *inNewDeviceList, tDevice ); // ensure they are in new list
		if ( !present ) { // not found in new list
			tDevice = hu_MoveDevice( inNewDeviceList, tDevice, inDeviceList ); // move to new list and get next
		} else { // found in new list( so don't do anything
			tDevice = tDevice->pNext; // just step to next device
		}
	}
}

/*************************************************************************
* Purpose:  given a IO device iterator, iterate it and for each device object do:
*			1. Create some private data to relate to each device.
*			2. Submit an IOServiceAddInterestNotification of type kIOGeneralInterest for this device,
*			using the refCon field to store a pointer to our data. When we get called with
*			this interest notification, we can grab the refCon and access our private data.
* Inputs:   inDeviceListHead    - the address of our device list
*			inIODeviceIterator  - IO device iterator
*           inHotplugFlag       - is this call generated by a hotplug event?
*/
static void hu_AddDevices(hu_device_t **inDeviceListHead, 
                          io_iterator_t inIODeviceIterator,
                          Boolean inHotplugFlag)
{
	IOReturn result = kIOReturnSuccess;	// assume success( optimist! )
	io_object_t ioHIDDeviceObject = 0;
	
	while ( 0 != (ioHIDDeviceObject = IOIteratorNext( inIODeviceIterator ) ) ) {
		hu_device_t **newDeviceAt = NULL;
		hu_device_t* newDevice = hu_BuildDevice( ioHIDDeviceObject );
		if ( newDevice ) {
#ifdef LOG_DEVICES
#if 0 // verbose
			fprintf( LOG_DEVICES, "hu_AddDevices: newDevice = {t: \"%s\", v: %ld, p: %ld, v: %ld, m: \"%s\", " \
					"p: \"%s\", l: %ld, u: %4.4lX:%4.4lX, #e: %ld, #f: %ld, #i: %ld, #o: %ld, " \
					"#c: %ld, #a: %ld, #b: %ld, #h: %ld, #s: %ld, #d: %ld, #w: %ld}.\n",
					newDevice->transport,
					newDevice->vendorID,
					newDevice->productID,
					newDevice->version,
					newDevice->manufacturer,
					newDevice->product,
					newDevice->locID,
					newDevice->usagePage,
					newDevice->usage,
					newDevice->totalElements,
					newDevice->features,
					newDevice->inputs,
					newDevice->outputs,
					newDevice->collections,
					newDevice->axis,
					newDevice->buttons,
					newDevice->hats,
					newDevice->sliders,
					newDevice->dials,
					newDevice->wheels
					);
			fflush( LOG_DEVICES );
#else	// otherwise output brief description
			fprintf( LOG_DEVICES, "hu_AddDevices: newDevice = {m: \"%s\" p: \"%s\", vid: %ld, pid: %ld, loc: %08lX, axes:  %ld, usage: %4.4lX:%4.4lX}.\n",
					newDevice->manufacturer,
					newDevice->product,
					newDevice->vendorID,
					newDevice->productID,
					newDevice->locID,
					newDevice->axis,					
					newDevice->usagePage,
					newDevice->usage
					);
			fflush( LOG_DEVICES );
#endif
#endif // LOG_DEVICES
						
			newDeviceAt = hu_AddDevice(inDeviceListHead, newDevice);
            
            if (inHotplugFlag && gHotPlugAddCallbackPtr)
                gHotPlugAddCallbackPtr(newDevice);
                
#if USE_NOTIFICATIONS
			// Register for an interest notification of this device being 
            // removed. Use a reference to our private data as the refCon 
            // which will be passed to the notification callback.
#if TARGET_RT_MAC_CFM
			result = IOServiceAddInterestNotification(gNotifyPort, 		// notifyPort
													  ioHIDDeviceObject, 	// service
													  kIOGeneralInterest, 	// interestType
																		// Notes: This is a MachO 
                                                      // function pointer. If you're using CFM you have 
                                                      // to call MachOFunctionPointerForCFMFunctionPointer.
													  MachOFunctionPointerForCFMFunctionPointer(hu_RemovalNotification),
													  newDevice, 			// refCon
													  (io_object_t*)&newDevice->notification);
#else
			result = IOServiceAddInterestNotification(gNotifyPort, 
													  ioHIDDeviceObject,  // service
													  kIOGeneralInterest, // interestType
													  hu_RemovalNotification, // callback
													  newDevice, 		// refCon
													  (io_object_t*)&newDevice->notification);	// notification
#endif // TARGET_RT_MAC_CFM
			if ( KERN_SUCCESS != result )
				HIDReportErrorNum( "\nhu_AddDevices: IOServiceAddInterestNotification error: x0%08lX.", result );
#else
#if TARGET_RT_MAC_CFM
			// Notes: This is a MachO function pointer. If you're using CFM 
			// you have to call MachOFunctionPointerForCFMFunctionPointer.
			result = ( *( IOHIDDeviceInterface** )newDevice->interface )->
				setRemovalCallback(newDevice->interface, 
					MachOFunctionPointerForCFMFunctionPointer(hu_RemovalCallbackFunction), 
								   newDeviceAt, 0);
#else
			result = ( *( IOHIDDeviceInterface** )newDevice->interface )->
				setRemovalCallback(newDevice->interface, 
								   hu_RemovalCallbackFunction, 
								   newDeviceAt, 0 );
#endif // TARGET_RT_MAC_CFM
#endif // USE_NOTIFICATIONS
		}
		
		// release the device object, it is no longer needed
		result = IOObjectRelease( ioHIDDeviceObject );
		if ( KERN_SUCCESS != result )
			HIDReportErrorNum( "\nhu_AddDevices: IOObjectRelease error with ioHIDDeviceObject.", result );
	}
}

/*************************************************************************
*
* hu_SetUpMatchingDictionary( inUsagePage, inUsage )
*
* Purpose:  builds a matching dictionary based on usage page and usage
*
* Notes:	Only called by hu_CreateMultiTypeDeviceList
*
* Inputs:   inUsagePage				- usage page
*			inUsage					- usages
*
* Returns:  CFMutableDictionaryRef  - the matching dictionary
*/

static CFMutableDictionaryRef hu_SetUpMatchingDictionary( UInt32 inUsagePage, UInt32 inUsage )
{
	CFNumberRef refUsage = NULL, refUsagePage = NULL;
	CFMutableDictionaryRef refHIDMatchDictionary = NULL;
	
	// Set up a matching dictionary to search I/O Registry by class name
	// for all HID class devices.  IOServiceAddMatchingNotification will consume
	// this dictionary reference, so there is no need to release it later on.
	refHIDMatchDictionary = IOServiceMatching( kIOHIDDeviceKey );
	if ( refHIDMatchDictionary ) {
		if ( inUsagePage ) {
			// Add key for device type( joystick, in this case ) to refine the matching dictionary.
			refUsagePage = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &inUsagePage );
			CFDictionarySetValue( refHIDMatchDictionary, CFSTR( kIOHIDPrimaryUsagePageKey ), refUsagePage );
			CFRelease( refUsagePage );
			
			// note: the usage is only valid if the usage page is also defined
			if ( inUsage ) {
				refUsage = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &inUsage );
				CFDictionarySetValue( refHIDMatchDictionary, CFSTR( kIOHIDPrimaryUsageKey ), refUsage );
				CFRelease( refUsage );
			}
		}
	} else if ( !refHIDMatchDictionary ) {
		HIDReportError( "hu_SetUpMatchingDictionary: Failed to get HID CFMutableDictionaryRef via IOServiceMatching." );
	}
	return refHIDMatchDictionary;
}

/*************************************************************************
*
* hu_DisposeDeviceElements( inElement )
*
* Purpose:  disposes of the element list associated with a device and the 
*			memory associated with the list
*
* Notes:	uses recursion to dispose both children and siblings.
*
* Inputs:   inElement	- the element
*
* Returns:  nothing
*/
static void hu_DisposeDeviceElements( hu_element_t* inElement )
{
	if ( inElement ) {
		hu_DisposeDeviceElements( inElement->pChild );
		hu_DisposeDeviceElements( inElement->pSibling );
		free( inElement );
	}
}

/*************************************************************************
*
* hu_DisposeReleaseQueue( inDevice )
*
* Purpose:  disposes and releases queue, sets queue to NULL
*
* Inputs:   inDevice					- the device
*
* Returns:  IOReturn 				- error code ( if any )
*/
static IOReturn hu_DisposeReleaseQueue( hu_device_t* inDevice )
{
	IOReturn result = kIOReturnSuccess;	// assume success( optimist! )
	
	if ( HIDIsValidDevice( inDevice ) ) {	// need valid device
		if ( inDevice->queue ) {	// and queue
									// stop queue
			result = ( *( IOHIDQueueInterface** ) inDevice->queue )->stop( inDevice->queue );
			if ( kIOReturnSuccess != result ) {
				HIDReportErrorNum( "\nhu_DisposeReleaseQueue - Failed to stop queue.", result );
			}
			// dispose of queue
			result = ( *( IOHIDQueueInterface** ) inDevice->queue )->dispose( inDevice->queue );
			if ( kIOReturnSuccess != result ) {
				HIDReportErrorNum( "\nhu_DisposeReleaseQueue - Failed to dipose queue.", result );
			}
			// release the queue
			result = ( *( IOHIDQueueInterface** ) inDevice->queue )->Release( inDevice->queue );
			if ( kIOReturnSuccess != result ) {
				HIDReportErrorNum( "\nhu_DisposeReleaseQueue - Failed to release queue.", result );
			}
			
			inDevice->queue = NULL;
#if USE_ASYNC_EVENTS
			if ( inDevice->queueRunLoopSource ) {
				if ( CFRunLoopContainsSource( CFRunLoopGetCurrent( ), inDevice->queueRunLoopSource, kCFRunLoopDefaultMode ) )
					CFRunLoopRemoveSource( CFRunLoopGetCurrent( ), inDevice->queueRunLoopSource, kCFRunLoopDefaultMode );
				CFRelease( inDevice->queueRunLoopSource );
				inDevice->queueRunLoopSource = NULL;
			}
#endif // USE_ASYNC_EVENTS
		} else {
			HIDReportError( "\nhu_DisposeReleaseQueue - no queue." );
		}
	} else {
		HIDReportError( "\nhu_DisposeReleaseQueue - Invalid device." );
	}
	return result;
}

/*************************************************************************
* HIDDequeueDevice( inDevice )
*
* Purpose: removes all device elements from queue
*
* Notes:	releases queue and closes device interface
*			does not release device interfaces,
*			application must call HIDReleaseDeviceList on exit
*
* Inputs:   inDevice		- the device
*
* Returns:  UInt32		- error code ( if any )
*/
unsigned long HIDDequeueDevice( hu_device_t* inDevice )
{
	IOReturn result = kIOReturnSuccess;	// assume success( optimist! )

	if ( HIDIsValidDevice( inDevice ) ) {
		if ( ( inDevice->interface ) && ( inDevice->queue ) ) {
			// iterate through elements and if queued, remove
			hu_element_t* inElement = HIDGetFirstDeviceElement( inDevice, kHIDElementTypeIO );
			while ( inElement ) {
				if ( ( *( IOHIDQueueInterface** ) inDevice->queue )->hasElement( inDevice->queue, (IOHIDElementCookie)inElement->cookie ) ) {
					result = ( *( IOHIDQueueInterface** ) inDevice->queue )->removeElement( inDevice->queue, (IOHIDElementCookie)inElement->cookie );
					if ( kIOReturnSuccess != result ) {
						HIDReportErrorNum( "\nHIDDequeueDevice - Failed to remove element from queue.", result );
					}
				}
				inElement = HIDGetNextDeviceElement( inElement, kHIDElementTypeIO );
			}
		}
		// ensure queue is disposed and released
		// interface will be closed and released on call to HIDReleaseDeviceList
		if ( inDevice->queue ) {
			result = hu_DisposeReleaseQueue( inDevice );
			if ( kIOReturnSuccess != result ) {
				HIDReportErrorNum( "\nremoveElement - Failed to dispose and release queue.", result );
			}
		}
	} else {
		HIDReportError( "\nHIDDequeueDevice - Invalid device." );
		result = kIOReturnBadArgument;
	}
	return result;
}

/*************************************************************************
*
* HIDCloseReleaseInterface( inDevice )
*
* Purpose: Closes and releases interface to device
*
* Notes:	should be done prior to exting application
*			will have no affect if device or interface do not exist
*			application will "own" the device if interface is not closed
*			device may have to be plug and re-plugged in different location
*			to get it working again without a restart )
*
* Inputs:   inDevice		- the device
*
* Returns:  UInt32		- error code ( if any )
*/
unsigned long HIDCloseReleaseInterface( hu_device_t* inDevice )
{
	IOReturn result = kIOReturnSuccess;	// assume success( optimist! )
	
	if ( HIDIsValidDevice( inDevice ) && inDevice->interface ) {
		// close the interface
		result = ( *( IOHIDDeviceInterface** ) inDevice->interface )->close( inDevice->interface );
		if ( kIOReturnNotOpen == result ) {
			// do nothing as device was not opened, thus can't be closed
		} else if ( kIOReturnSuccess != result ) {
			HIDReportErrorNum( "\nHIDCloseReleaseInterface - Failed to close IOHIDDeviceInterface.", result );
		}
		//release the interface
		result = ( *( IOHIDDeviceInterface** ) inDevice->interface )->Release( inDevice->interface );
		if ( kIOReturnSuccess != result ) {
			HIDReportErrorNum( "\nHIDCloseReleaseInterface - Failed to release interface.", result );
		}
		inDevice->interface = NULL;
	}
	return result;
}

/*************************************************************************
*
* hu_DisposeDevice( inDevice )
*
* Purpose:  disposes of a single device,
*
* Notes:	closes and releases the device interface,
*			frees memory for the device and all its elements
*			sets device pointer to NULL
*			all your device no longer belong to us...
*			... you do not 'own' the device anymore.
*
* Inputs:   inDevice			- the device
*
* Returns:  hu_device_t*	- pointer to next device
*/

static hu_device_t* hu_DisposeDevice( hu_device_t* inDevice )
{
	kern_return_t result = KERN_SUCCESS;	// assume success( optimist! )
	hu_device_t* tDeviceNext = NULL;
	
	if ( HIDIsValidDevice( inDevice ) ) {
		// save next device prior to disposing of this device
		tDeviceNext = inDevice->pNext;
		
		result = HIDDequeueDevice( inDevice );
#if 0		// ignore these errors
		if ( kIOReturnSuccess != result )
			HIDReportErrorNum( "\nhu_DisposeDevice: HIDDequeueDevice error: 0x%08lX.", result );
#endif
		
		hu_DisposeDeviceElements( inDevice->pListElements );
		inDevice->pListElements = NULL;
		
		result = HIDCloseReleaseInterface( inDevice ); // function sanity checks interface value( now application does not own device )
		if ( kIOReturnSuccess != result )
		 	HIDReportErrorNum( "\nhu_DisposeDevice: HIDCloseReleaseInterface error: 0x%08lX.", result );
		
#if USE_NOTIFICATIONS
		if ( inDevice->interface ) {
			// NOT( *inDevice->interface )->Release( inDevice->interface );
			result = IODestroyPlugInInterface( inDevice->interface );
			if ( kIOReturnSuccess != result )
				HIDReportErrorNum( "\nhu_DisposeDevice: IODestroyPlugInInterface error: 0x%08lX.", result );
		}
		
		if ( inDevice->notification ) {
			result = IOObjectRelease( ( io_object_t ) inDevice->notification );
			if ( kIOReturnSuccess != result )
				HIDReportErrorNum( "\nhu_DisposeDevice: IOObjectRelease error: 0x%08lX.", result );
		}
#endif // USE_NOTIFICATIONS
		
		// remove this device from the device list
		if ( gDeviceList == inDevice ) {	// head of list?
			gDeviceList = tDeviceNext;
		} else {
			hu_device_t* tDeviceTemp = tDeviceNext = gDeviceList;	// we're going to return this if we don't find ourselfs in the list
			while ( tDeviceTemp ) {
				if ( tDeviceTemp->pNext == inDevice ) { // found us!
														// take us out of linked list
					tDeviceTemp->pNext = tDeviceNext = inDevice->pNext;
					break;
				}
				tDeviceTemp = tDeviceTemp->pNext;
			}
		}
		free( inDevice );
	}
	
	// update device count
	gNumDevices = hu_CountCurrentDevices( );
	return tDeviceNext;
}

/*************************************************************************
*
* hu_CountCurrentDevices( void )
*
* Purpose:  count number of devices in global device list( gDeviceList )
*
* Inputs:   none
*
* Returns:  UInt32  - the count
*/

static UInt32 hu_CountCurrentDevices( void )
{
	hu_device_t* tDevice = gDeviceList;
	UInt32 count = 0;
	while ( tDevice ) {
		count++;
		tDevice = tDevice->pNext;
	}
	return count;
}

/*************************************************************************
*
* hu_MatchElementTypeMask( inIOHIDElementType, inTypeMask )
*
* Purpose:  matches type masks passed in to actual element types( which are not set up to be used as a mask )
*
* Inputs:   inIOHIDElementType   - the element type
*			inTypeMask			- the element type mask
*
* Returns:  Boolean				- TRUE if they match
*/

static Boolean hu_MatchElementTypeMask( IOHIDElementType inIOHIDElementType, HIDElementTypeMask inTypeMask )
{
	Boolean result = FALSE;	// assume failure ( pessimist! )
	if ( inTypeMask & kHIDElementTypeInput ) {
		if (	( kIOHIDElementTypeInput_Misc == inIOHIDElementType  ) ||
				( kIOHIDElementTypeInput_Button == inIOHIDElementType  ) ||
				( kIOHIDElementTypeInput_Axis == inIOHIDElementType ) ||
				( kIOHIDElementTypeInput_ScanCodes == inIOHIDElementType ) )
		{
			result = TRUE;
		}
	}
	
	if ( inTypeMask & kHIDElementTypeOutput ) {
		if ( kIOHIDElementTypeOutput == inIOHIDElementType ) {
			result = TRUE;
		}
	}
	
	if ( inTypeMask & kHIDElementTypeFeature ) {
		if ( kIOHIDElementTypeFeature == inIOHIDElementType ) {
			result = TRUE;
		}
	}
	
	if ( inTypeMask & kHIDElementTypeCollection ) {
		if ( kIOHIDElementTypeCollection == inIOHIDElementType ) {
			result = TRUE;
		}
	}
	return result;
}

/*************************************************************************
*
* hu_GetDeviceElement( inElement, inTypeMask )
*
* Purpose:  resurcively search for element of type( mask )
*
* Notes:	called( multiple times ) by HIDGetNextDeviceElement
*
* Inputs:   inElement			- the current element
*			inTypeMask			- the element type mask
*
* Returns:  hu_element_t		- the element of type( mask )
*/
static hu_element_t* hu_GetDeviceElement( hu_element_t* inElement, HIDElementTypeMask inTypeMask )
{
	hu_element_t* result = inElement;	// return the element passed in...
	if ( inElement ) {
		// unless it's doesn't match the type we're looking for
		if ( !hu_MatchElementTypeMask( inElement->type, inTypeMask ) ) {
			// in which case we return the next one
			result = HIDGetNextDeviceElement( inElement, inTypeMask );
		}
	}
	return result;
}

/*************************************************************************
*
* HIDGetUsageName( inUsagePage, inUsage, outCStrName )
*
* Purpose:  return a C string for a given usage page & usage( see IOUSBHIDParser.h )
* Notes:	returns usage page and usage values in string form for unknown values
*
* Inputs:   inUsagePage	- the usage page
*			inUsage		- the usage
*			outCStrName   - address where to store usage string
*
* Returns:  outCStrName	- the usage string
*/

void HIDGetUsageName( long inUsagePage, long inUsage, char* outCStrName )
{
	if ( hu_XMLGetUsageName( inUsagePage, inUsage, outCStrName ) )
		return;
	
	switch( inUsagePage ) {
		case kHIDPage_Undefined: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Undefined Page, Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_GenericDesktop: {
			switch( inUsage ) {
				case kHIDUsage_GD_Pointer: sprintf( outCStrName, "Pointer" ); break;
				case kHIDUsage_GD_Mouse: sprintf( outCStrName, "Mouse" ); break;
				case kHIDUsage_GD_Joystick: sprintf( outCStrName, "Joystick" ); break;
				case kHIDUsage_GD_GamePad: sprintf( outCStrName, "GamePad" ); break;
				case kHIDUsage_GD_Keyboard: sprintf( outCStrName, "Keyboard" ); break;
				case kHIDUsage_GD_Keypad: sprintf( outCStrName, "Keypad" ); break;
				case kHIDUsage_GD_MultiAxisController: sprintf( outCStrName, "Multi-Axis Controller" ); break;
					
				case kHIDUsage_GD_X: sprintf( outCStrName, "X-Axis" ); break;
				case kHIDUsage_GD_Y: sprintf( outCStrName, "Y-Axis" ); break;
				case kHIDUsage_GD_Z: sprintf( outCStrName, "Z-Axis" ); break;
				case kHIDUsage_GD_Rx: sprintf( outCStrName, "X-Rotation" ); break;
				case kHIDUsage_GD_Ry: sprintf( outCStrName, "Y-Rotation" ); break;
				case kHIDUsage_GD_Rz: sprintf( outCStrName, "Z-Rotation" ); break;
				case kHIDUsage_GD_Slider: sprintf( outCStrName, "Slider" ); break;
				case kHIDUsage_GD_Dial: sprintf( outCStrName, "Dial" ); break;
				case kHIDUsage_GD_Wheel: sprintf( outCStrName, "Wheel" ); break;
				case kHIDUsage_GD_Hatswitch: sprintf( outCStrName, "Hatswitch" ); break;
				case kHIDUsage_GD_CountedBuffer: sprintf( outCStrName, "Counted Buffer" ); break;
				case kHIDUsage_GD_ByteCount: sprintf( outCStrName, "Byte Count" ); break;
				case kHIDUsage_GD_MotionWakeup: sprintf( outCStrName, "Motion Wakeup" ); break;
				case kHIDUsage_GD_Start: sprintf( outCStrName, "Start" ); break;
				case kHIDUsage_GD_Select: sprintf( outCStrName, "Select" ); break;
					
				case kHIDUsage_GD_Vx: sprintf( outCStrName, "X-Velocity" ); break;
				case kHIDUsage_GD_Vy: sprintf( outCStrName, "Y-Velocity" ); break;
				case kHIDUsage_GD_Vz: sprintf( outCStrName, "Z-Velocity" ); break;
				case kHIDUsage_GD_Vbrx: sprintf( outCStrName, "X-Rotation Velocity" ); break;
				case kHIDUsage_GD_Vbry: sprintf( outCStrName, "Y-Rotation Velocity" ); break;
				case kHIDUsage_GD_Vbrz: sprintf( outCStrName, "Z-Rotation Velocity" ); break;
				case kHIDUsage_GD_Vno: sprintf( outCStrName, "Vno" ); break;
					
				case kHIDUsage_GD_SystemControl: sprintf( outCStrName, "System Control" ); break;
				case kHIDUsage_GD_SystemPowerDown: sprintf( outCStrName, "System Power Down" ); break;
				case kHIDUsage_GD_SystemSleep: sprintf( outCStrName, "System Sleep" ); break;
				case kHIDUsage_GD_SystemWakeUp: sprintf( outCStrName, "System Wake Up" ); break;
				case kHIDUsage_GD_SystemContextMenu: sprintf( outCStrName, "System Context Menu" ); break;
				case kHIDUsage_GD_SystemMainMenu: sprintf( outCStrName, "System Main Menu" ); break;
				case kHIDUsage_GD_SystemAppMenu: sprintf( outCStrName, "System App Menu" ); break;
				case kHIDUsage_GD_SystemMenuHelp: sprintf( outCStrName, "System Menu Help" ); break;
				case kHIDUsage_GD_SystemMenuExit: sprintf( outCStrName, "System Menu Exit" ); break;
				case kHIDUsage_GD_SystemMenu: sprintf( outCStrName, "System Menu" ); break;
				case kHIDUsage_GD_SystemMenuRight: sprintf( outCStrName, "System Menu Right" ); break;
				case kHIDUsage_GD_SystemMenuLeft: sprintf( outCStrName, "System Menu Left" ); break;
				case kHIDUsage_GD_SystemMenuUp: sprintf( outCStrName, "System Menu Up" ); break;
				case kHIDUsage_GD_SystemMenuDown: sprintf( outCStrName, "System Menu Down" ); break;
					
				case kHIDUsage_GD_DPadUp: sprintf( outCStrName, "DPad Up" ); break;
				case kHIDUsage_GD_DPadDown: sprintf( outCStrName, "DPad Down" ); break;
				case kHIDUsage_GD_DPadRight: sprintf( outCStrName, "DPad Right" ); break;
				case kHIDUsage_GD_DPadLeft: sprintf( outCStrName, "DPad Left" ); break;
					
				case kHIDUsage_GD_Reserved: sprintf( outCStrName, "Reserved" ); break;
					
				default: sprintf( outCStrName, "Generic Desktop Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Simulation: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Simulation Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_VR: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "VR Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Sport: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Sport Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Game: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Game Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_KeyboardOrKeypad: {
			switch( inUsage ) {
				case kHIDUsage_KeyboardErrorRollOver: sprintf( outCStrName, "Error Roll Over" ); break;
				case kHIDUsage_KeyboardPOSTFail: sprintf( outCStrName, "POST Fail" ); break;
				case kHIDUsage_KeyboardErrorUndefined: sprintf( outCStrName, "Error Undefined" ); break;
				case kHIDUsage_KeyboardA: sprintf( outCStrName, "A" ); break;
				case kHIDUsage_KeyboardB: sprintf( outCStrName, "B" ); break;
				case kHIDUsage_KeyboardC: sprintf( outCStrName, "C" ); break;
				case kHIDUsage_KeyboardD: sprintf( outCStrName, "D" ); break;
				case kHIDUsage_KeyboardE: sprintf( outCStrName, "E" ); break;
				case kHIDUsage_KeyboardF: sprintf( outCStrName, "F" ); break;
				case kHIDUsage_KeyboardG: sprintf( outCStrName, "G" ); break;
				case kHIDUsage_KeyboardH: sprintf( outCStrName, "H" ); break;
				case kHIDUsage_KeyboardI: sprintf( outCStrName, "I" ); break;
				case kHIDUsage_KeyboardJ: sprintf( outCStrName, "J" ); break;
				case kHIDUsage_KeyboardK: sprintf( outCStrName, "K" ); break;
				case kHIDUsage_KeyboardL: sprintf( outCStrName, "L" ); break;
				case kHIDUsage_KeyboardM: sprintf( outCStrName, "M" ); break;
				case kHIDUsage_KeyboardN: sprintf( outCStrName, "N" ); break;
				case kHIDUsage_KeyboardO: sprintf( outCStrName, "O" ); break;
				case kHIDUsage_KeyboardP: sprintf( outCStrName, "P" ); break;
				case kHIDUsage_KeyboardQ: sprintf( outCStrName, "Q" ); break;
				case kHIDUsage_KeyboardR: sprintf( outCStrName, "R" ); break;
				case kHIDUsage_KeyboardS: sprintf( outCStrName, "S" ); break;
				case kHIDUsage_KeyboardT: sprintf( outCStrName, "T" ); break;
				case kHIDUsage_KeyboardU: sprintf( outCStrName, "U" ); break;
				case kHIDUsage_KeyboardV: sprintf( outCStrName, "V" ); break;
				case kHIDUsage_KeyboardW: sprintf( outCStrName, "W" ); break;
				case kHIDUsage_KeyboardX: sprintf( outCStrName, "X" ); break;
				case kHIDUsage_KeyboardY: sprintf( outCStrName, "Y" ); break;
				case kHIDUsage_KeyboardZ: sprintf( outCStrName, "Z" ); break;
				case kHIDUsage_Keyboard1: sprintf( outCStrName, "1" ); break;
				case kHIDUsage_Keyboard2: sprintf( outCStrName, "2" ); break;
				case kHIDUsage_Keyboard3: sprintf( outCStrName, "3" ); break;
				case kHIDUsage_Keyboard4: sprintf( outCStrName, "4" ); break;
				case kHIDUsage_Keyboard5: sprintf( outCStrName, "5" ); break;
				case kHIDUsage_Keyboard6: sprintf( outCStrName, "6" ); break;
				case kHIDUsage_Keyboard7: sprintf( outCStrName, "7" ); break;
				case kHIDUsage_Keyboard8: sprintf( outCStrName, "8" ); break;
				case kHIDUsage_Keyboard9: sprintf( outCStrName, "9" ); break;
				case kHIDUsage_Keyboard0: sprintf( outCStrName, "0" ); break;
				case kHIDUsage_KeyboardReturnOrEnter: sprintf( outCStrName, "Return" ); break;
				case kHIDUsage_KeyboardEscape: sprintf( outCStrName, "Escape" ); break;
				case kHIDUsage_KeyboardDeleteOrBackspace: sprintf( outCStrName, "Delete" ); break;
				case kHIDUsage_KeyboardTab: sprintf( outCStrName, "Tab" ); break;
				case kHIDUsage_KeyboardSpacebar: sprintf( outCStrName, "Spacebar" ); break;
				case kHIDUsage_KeyboardHyphen: sprintf( outCStrName, "Dash" ); break;
				case kHIDUsage_KeyboardEqualSign: sprintf( outCStrName, "Equal" ); break;
				case kHIDUsage_KeyboardOpenBracket: sprintf( outCStrName, "Left Square Bracket" ); break;
				case kHIDUsage_KeyboardCloseBracket: sprintf( outCStrName, "Right Square Bracket" ); break;
				case kHIDUsage_KeyboardBackslash: sprintf( outCStrName, "Slash" ); break;
				case kHIDUsage_KeyboardNonUSPound: sprintf( outCStrName, "Non-US #" ); break;
				case kHIDUsage_KeyboardSemicolon: sprintf( outCStrName, "Semi-Colon" ); break;
				case kHIDUsage_KeyboardQuote: sprintf( outCStrName, "Single Quote" ); break;
				case kHIDUsage_KeyboardGraveAccentAndTilde: sprintf( outCStrName, "Grave Accent" ); break;
				case kHIDUsage_KeyboardComma: sprintf( outCStrName, "Comma" ); break;
				case kHIDUsage_KeyboardPeriod: sprintf( outCStrName, "Period" ); break;
				case kHIDUsage_KeyboardSlash: sprintf( outCStrName, "Slash" ); break;
				case kHIDUsage_KeyboardCapsLock: sprintf( outCStrName, "Caps Lock" ); break;
				case kHIDUsage_KeyboardF1: sprintf( outCStrName, "F1" ); break;
				case kHIDUsage_KeyboardF2: sprintf( outCStrName, "F2" ); break;
				case kHIDUsage_KeyboardF3: sprintf( outCStrName, "F3" ); break;
				case kHIDUsage_KeyboardF4: sprintf( outCStrName, "F4" ); break;
				case kHIDUsage_KeyboardF5: sprintf( outCStrName, "F5" ); break;
				case kHIDUsage_KeyboardF6: sprintf( outCStrName, "F6" ); break;
				case kHIDUsage_KeyboardF7: sprintf( outCStrName, "F7" ); break;
				case kHIDUsage_KeyboardF8: sprintf( outCStrName, "F8" ); break;
				case kHIDUsage_KeyboardF9: sprintf( outCStrName, "F9" ); break;
				case kHIDUsage_KeyboardF10: sprintf( outCStrName, "F10" ); break;
				case kHIDUsage_KeyboardF11: sprintf( outCStrName, "F11" ); break;
				case kHIDUsage_KeyboardF12: sprintf( outCStrName, "F12" ); break;
				case kHIDUsage_KeyboardPrintScreen: sprintf( outCStrName, "Print Screen" ); break;
				case kHIDUsage_KeyboardScrollLock: sprintf( outCStrName, "Scroll Lock" ); break;
				case kHIDUsage_KeyboardPause: sprintf( outCStrName, "Pause" ); break;
				case kHIDUsage_KeyboardInsert: sprintf( outCStrName, "Insert" ); break;
				case kHIDUsage_KeyboardHome: sprintf( outCStrName, "Home" ); break;
				case kHIDUsage_KeyboardPageUp: sprintf( outCStrName, "Page Up" ); break;
				case kHIDUsage_KeyboardDeleteForward: sprintf( outCStrName, "Delete Forward" ); break;
				case kHIDUsage_KeyboardEnd: sprintf( outCStrName, "End" ); break;
				case kHIDUsage_KeyboardPageDown: sprintf( outCStrName, "Page Down" ); break;
				case kHIDUsage_KeyboardRightArrow: sprintf( outCStrName, "Right Arrow" ); break;
				case kHIDUsage_KeyboardLeftArrow: sprintf( outCStrName, "Left Arrow" ); break;
				case kHIDUsage_KeyboardDownArrow: sprintf( outCStrName, "Down Arrow" ); break;
				case kHIDUsage_KeyboardUpArrow: sprintf( outCStrName, "Up Arrow" ); break;
				case kHIDUsage_KeypadNumLock: sprintf( outCStrName, "Keypad NumLock" ); break;
				case kHIDUsage_KeypadSlash: sprintf( outCStrName, "Keypad Slash" ); break;
				case kHIDUsage_KeypadAsterisk: sprintf( outCStrName, "Keypad Asterisk" ); break;
				case kHIDUsage_KeypadHyphen: sprintf( outCStrName, "Keypad Dash" ); break;
				case kHIDUsage_KeypadPlus: sprintf( outCStrName, "Keypad Plus" ); break;
				case kHIDUsage_KeypadEnter: sprintf( outCStrName, "Keypad Enter" ); break;
				case kHIDUsage_Keypad1: sprintf( outCStrName, "Keypad 1" ); break;
				case kHIDUsage_Keypad2: sprintf( outCStrName, "Keypad 2" ); break;
				case kHIDUsage_Keypad3: sprintf( outCStrName, "Keypad 3" ); break;
				case kHIDUsage_Keypad4: sprintf( outCStrName, "Keypad 4" ); break;
				case kHIDUsage_Keypad5: sprintf( outCStrName, "Keypad 5" ); break;
				case kHIDUsage_Keypad6: sprintf( outCStrName, "Keypad 6" ); break;
				case kHIDUsage_Keypad7: sprintf( outCStrName, "Keypad 7" ); break;
				case kHIDUsage_Keypad8: sprintf( outCStrName, "Keypad 8" ); break;
				case kHIDUsage_Keypad9: sprintf( outCStrName, "Keypad 9" ); break;
				case kHIDUsage_Keypad0: sprintf( outCStrName, "Keypad 0" ); break;
				case kHIDUsage_KeypadPeriod: sprintf( outCStrName, "Keypad Period" ); break;
				case kHIDUsage_KeyboardNonUSBackslash: sprintf( outCStrName, "Non-US Backslash" ); break;
				case kHIDUsage_KeyboardApplication: sprintf( outCStrName, "Application" ); break;
				case kHIDUsage_KeyboardPower: sprintf( outCStrName, "Power" ); break;
				case kHIDUsage_KeypadEqualSign: sprintf( outCStrName, "Keypad Equal" ); break;
				case kHIDUsage_KeyboardF13: sprintf( outCStrName, "F13" ); break;
				case kHIDUsage_KeyboardF14: sprintf( outCStrName, "F14" ); break;
				case kHIDUsage_KeyboardF15: sprintf( outCStrName, "F15" ); break;
				case kHIDUsage_KeyboardF16: sprintf( outCStrName, "F16" ); break;
				case kHIDUsage_KeyboardF17: sprintf( outCStrName, "F17" ); break;
				case kHIDUsage_KeyboardF18: sprintf( outCStrName, "F18" ); break;
				case kHIDUsage_KeyboardF19: sprintf( outCStrName, "F19" ); break;
				case kHIDUsage_KeyboardF20: sprintf( outCStrName, "F20" ); break;
				case kHIDUsage_KeyboardF21: sprintf( outCStrName, "F21" ); break;
				case kHIDUsage_KeyboardF22: sprintf( outCStrName, "F22" ); break;
				case kHIDUsage_KeyboardF23: sprintf( outCStrName, "F23" ); break;
				case kHIDUsage_KeyboardF24: sprintf( outCStrName, "F24" ); break;
				case kHIDUsage_KeyboardExecute: sprintf( outCStrName, "Execute" ); break;
				case kHIDUsage_KeyboardHelp: sprintf( outCStrName, "Help" ); break;
				case kHIDUsage_KeyboardMenu: sprintf( outCStrName, "Menu" ); break;
				case kHIDUsage_KeyboardSelect: sprintf( outCStrName, "Select" ); break;
				case kHIDUsage_KeyboardStop: sprintf( outCStrName, "Stop" ); break;
				case kHIDUsage_KeyboardAgain: sprintf( outCStrName, "Again" ); break;
				case kHIDUsage_KeyboardUndo: sprintf( outCStrName, "Undo" ); break;
				case kHIDUsage_KeyboardCut: sprintf( outCStrName, "Cut" ); break;
				case kHIDUsage_KeyboardCopy: sprintf( outCStrName, "Copy" ); break;
				case kHIDUsage_KeyboardPaste: sprintf( outCStrName, "Paste" ); break;
				case kHIDUsage_KeyboardFind: sprintf( outCStrName, "Find" ); break;
				case kHIDUsage_KeyboardMute: sprintf( outCStrName, "Mute" ); break;
				case kHIDUsage_KeyboardVolumeUp: sprintf( outCStrName, "Volume Up" ); break;
				case kHIDUsage_KeyboardVolumeDown: sprintf( outCStrName, "Volume Down" ); break;
				case kHIDUsage_KeyboardLockingCapsLock: sprintf( outCStrName, "Locking Caps Lock" ); break;
				case kHIDUsage_KeyboardLockingNumLock: sprintf( outCStrName, "Locking Num Lock" ); break;
				case kHIDUsage_KeyboardLockingScrollLock: sprintf( outCStrName, "Locking Scroll Lock" ); break;
				case kHIDUsage_KeypadComma: sprintf( outCStrName, "Keypad Comma" ); break;
				case kHIDUsage_KeypadEqualSignAS400: sprintf( outCStrName, "Keypad Equal Sign for AS-400" ); break;
				case kHIDUsage_KeyboardInternational1: sprintf( outCStrName, "International1" ); break;
				case kHIDUsage_KeyboardInternational2: sprintf( outCStrName, "International2" ); break;
				case kHIDUsage_KeyboardInternational3: sprintf( outCStrName, "International3" ); break;
				case kHIDUsage_KeyboardInternational4: sprintf( outCStrName, "International4" ); break;
				case kHIDUsage_KeyboardInternational5: sprintf( outCStrName, "International5" ); break;
				case kHIDUsage_KeyboardInternational6: sprintf( outCStrName, "International6" ); break;
				case kHIDUsage_KeyboardInternational7: sprintf( outCStrName, "International7" ); break;
				case kHIDUsage_KeyboardInternational8: sprintf( outCStrName, "International8" ); break;
				case kHIDUsage_KeyboardInternational9: sprintf( outCStrName, "International9" ); break;
				case kHIDUsage_KeyboardLANG1: sprintf( outCStrName, "LANG1" ); break;
				case kHIDUsage_KeyboardLANG2: sprintf( outCStrName, "LANG2" ); break;
				case kHIDUsage_KeyboardLANG3: sprintf( outCStrName, "LANG3" ); break;
				case kHIDUsage_KeyboardLANG4: sprintf( outCStrName, "LANG4" ); break;
				case kHIDUsage_KeyboardLANG5: sprintf( outCStrName, "LANG5" ); break;
				case kHIDUsage_KeyboardLANG6: sprintf( outCStrName, "LANG6" ); break;
				case kHIDUsage_KeyboardLANG7: sprintf( outCStrName, "LANG7" ); break;
				case kHIDUsage_KeyboardLANG8: sprintf( outCStrName, "LANG8" ); break;
				case kHIDUsage_KeyboardLANG9: sprintf( outCStrName, "LANG9" ); break;
				case kHIDUsage_KeyboardAlternateErase: sprintf( outCStrName, "Alternate Erase" ); break;
				case kHIDUsage_KeyboardSysReqOrAttention: sprintf( outCStrName, "SysReq or Attention" ); break;
				case kHIDUsage_KeyboardCancel: sprintf( outCStrName, "Cancel" ); break;
				case kHIDUsage_KeyboardClear: sprintf( outCStrName, "Clear" ); break;
				case kHIDUsage_KeyboardPrior: sprintf( outCStrName, "Prior" ); break;
				case kHIDUsage_KeyboardReturn: sprintf( outCStrName, "Return" ); break;
				case kHIDUsage_KeyboardSeparator: sprintf( outCStrName, "Separator" ); break;
				case kHIDUsage_KeyboardOut: sprintf( outCStrName, "Out" ); break;
				case kHIDUsage_KeyboardOper: sprintf( outCStrName, "Oper" ); break;
				case kHIDUsage_KeyboardClearOrAgain: sprintf( outCStrName, "Clear or Again" ); break;
				case kHIDUsage_KeyboardCrSelOrProps: sprintf( outCStrName, "CrSel or Props" ); break;
				case kHIDUsage_KeyboardExSel: sprintf( outCStrName, "ExSel" ); break;
				case kHIDUsage_KeyboardLeftControl: sprintf( outCStrName, "Left Control" ); break;
				case kHIDUsage_KeyboardLeftShift: sprintf( outCStrName, "Left Shift" ); break;
				case kHIDUsage_KeyboardLeftAlt: sprintf( outCStrName, "Left Alt" ); break;
				case kHIDUsage_KeyboardLeftGUI: sprintf( outCStrName, "Left GUI" ); break;
				case kHIDUsage_KeyboardRightControl: sprintf( outCStrName, "Right Control" ); break;
				case kHIDUsage_KeyboardRightShift: sprintf( outCStrName, "Right Shift" ); break;
				case kHIDUsage_KeyboardRightAlt: sprintf( outCStrName, "Right Alt" ); break;
				case kHIDUsage_KeyboardRightGUI: sprintf( outCStrName, "Right GUI" ); break;
				case kHIDUsage_Keyboard_Reserved: sprintf( outCStrName, "Reserved" ); break;
				default: sprintf( outCStrName, "Keyboard Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_LEDs: {
			switch( inUsage ) {
				// some LED usages
				case kHIDUsage_LED_IndicatorRed: sprintf( outCStrName, "Red LED" ); break;
				case kHIDUsage_LED_IndicatorGreen: sprintf( outCStrName, "Green LED" ); break;
				case kHIDUsage_LED_IndicatorAmber: sprintf( outCStrName, "Amber LED" ); break;
				case kHIDUsage_LED_GenericIndicator: sprintf( outCStrName, "Generic LED" ); break;
				case kHIDUsage_LED_SystemSuspend: sprintf( outCStrName, "System Suspend LED" ); break;
				case kHIDUsage_LED_ExternalPowerConnected: sprintf( outCStrName, "External Power LED" ); break;
				default: sprintf( outCStrName, "LED Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Button: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Button #%ld", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Ordinal: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Ordinal Instance %lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Telephony: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Telephony Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Consumer: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Consumer Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Digitizer: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Digitizer Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_PID: {
			if ( ( ( inUsage >= 0x02 ) && ( inUsage <= 0x1F ) ) || ( ( inUsage >= 0x29 ) && ( inUsage <= 0x2F ) ) ||
				 ( ( inUsage >= 0x35 ) && ( inUsage <= 0x3F ) ) || ( ( inUsage >= 0x44 ) && ( inUsage <= 0x4F ) ) ||
				 ( inUsage == 0x8A ) || ( inUsage == 0x93 ) || ( ( inUsage >= 0x9D ) && ( inUsage <= 0x9E ) ) ||
				 ( ( inUsage >= 0xA1 ) && ( inUsage <= 0xA3 ) ) || ( ( inUsage >= 0xAD ) && ( inUsage <= 0xFFFF ) ) ) {
				sprintf( outCStrName, "PID Reserved" );
			} else {
				switch( inUsage ) {
					case 0x00: sprintf( outCStrName, "PID Undefined Usage" ); break;
					case kHIDUsage_PID_PhysicalInterfaceDevice: sprintf( outCStrName, "Physical Interface Device" ); break;
					case kHIDUsage_PID_Normal: sprintf( outCStrName, "Normal Force" ); break;
						
					case kHIDUsage_PID_SetEffectReport: sprintf( outCStrName, "Set Effect Report" ); break;
					case kHIDUsage_PID_EffectBlockIndex: sprintf( outCStrName, "Effect Block Index" ); break;
					case kHIDUsage_PID_ParamBlockOffset: sprintf( outCStrName, "Parameter Block Offset" ); break;
					case kHIDUsage_PID_ROM_Flag: sprintf( outCStrName, "ROM Flag" ); break;
						
					case kHIDUsage_PID_EffectType: sprintf( outCStrName, "Effect Type" ); break;
					case kHIDUsage_PID_ET_ConstantForce: sprintf( outCStrName, "Effect Type Constant Force" ); break;
					case kHIDUsage_PID_ET_Ramp: sprintf( outCStrName, "Effect Type Ramp" ); break;
					case kHIDUsage_PID_ET_CustomForceData: sprintf( outCStrName, "Effect Type Custom Force Data" ); break;
					case kHIDUsage_PID_ET_Square: sprintf( outCStrName, "Effect Type Square" ); break;
					case kHIDUsage_PID_ET_Sine: sprintf( outCStrName, "Effect Type Sine" ); break;
					case kHIDUsage_PID_ET_Triangle: sprintf( outCStrName, "Effect Type Triangle" ); break;
					case kHIDUsage_PID_ET_SawtoothUp: sprintf( outCStrName, "Effect Type Sawtooth Up" ); break;
					case kHIDUsage_PID_ET_SawtoothDown: sprintf( outCStrName, "Effect Type Sawtooth Down" ); break;
					case kHIDUsage_PID_ET_Spring: sprintf( outCStrName, "Effect Type Spring" ); break;
					case kHIDUsage_PID_ET_Damper: sprintf( outCStrName, "Effect Type Damper" ); break;
					case kHIDUsage_PID_ET_Inertia: sprintf( outCStrName, "Effect Type Inertia" ); break;
					case kHIDUsage_PID_ET_Friction: sprintf( outCStrName, "Effect Type Friction" ); break;
					case kHIDUsage_PID_Duration: sprintf( outCStrName, "Effect Duration" ); break;
					case kHIDUsage_PID_SamplePeriod: sprintf( outCStrName, "Effect Sample Period" ); break;
					case kHIDUsage_PID_Gain: sprintf( outCStrName, "Effect Gain" ); break;
					case kHIDUsage_PID_TriggerButton: sprintf( outCStrName, "Effect Trigger Button" ); break;
					case kHIDUsage_PID_TriggerRepeatInterval: sprintf( outCStrName, "Effect Trigger Repeat Interval" ); break;
						
					case kHIDUsage_PID_AxesEnable: sprintf( outCStrName, "Axis Enable" ); break;
					case kHIDUsage_PID_DirectionEnable: sprintf( outCStrName, "Direction Enable" ); break;
						
					case kHIDUsage_PID_Direction: sprintf( outCStrName, "Direction" ); break;
						
					case kHIDUsage_PID_TypeSpecificBlockOffset: sprintf( outCStrName, "Type Specific Block Offset" ); break;
						
					case kHIDUsage_PID_BlockType: sprintf( outCStrName, "Block Type" ); break;
						
					case kHIDUsage_PID_SetEnvelopeReport: sprintf( outCStrName, "Set Envelope Report" ); break;
					case kHIDUsage_PID_AttackLevel: sprintf( outCStrName, "Envelope Attack Level" ); break;
					case kHIDUsage_PID_AttackTime: sprintf( outCStrName, "Envelope Attack Time" ); break;
					case kHIDUsage_PID_FadeLevel: sprintf( outCStrName, "Envelope Fade Level" ); break;
					case kHIDUsage_PID_FadeTime: sprintf( outCStrName, "Envelope Fade Time" ); break;
						
					case kHIDUsage_PID_SetConditionReport: sprintf( outCStrName, "Set Condition Report" ); break;
					case kHIDUsage_PID_CP_Offset: sprintf( outCStrName, "Condition CP Offset" ); break;
					case kHIDUsage_PID_PositiveCoefficient: sprintf( outCStrName, "Condition Positive Coefficient" ); break;
					case kHIDUsage_PID_NegativeCoefficient: sprintf( outCStrName, "Condition Negative Coefficient" ); break;
					case kHIDUsage_PID_PositiveSaturation: sprintf( outCStrName, "Condition Positive Saturation" ); break;
					case kHIDUsage_PID_NegativeSaturation: sprintf( outCStrName, "Condition Negative Saturation" ); break;
					case kHIDUsage_PID_DeadBand: sprintf( outCStrName, "Condition Dead Band" ); break;
						
					case kHIDUsage_PID_DownloadForceSample: sprintf( outCStrName, "Download Force Sample" ); break;
					case kHIDUsage_PID_IsochCustomForceEnable: sprintf( outCStrName, "Isoch Custom Force Enable" ); break;
						
					case kHIDUsage_PID_CustomForceDataReport: sprintf( outCStrName, "Custom Force Data Report" ); break;
					case kHIDUsage_PID_CustomForceData: sprintf( outCStrName, "Custom Force Data" ); break;
						
					case kHIDUsage_PID_CustomForceVendorDefinedData: sprintf( outCStrName, "Custom Force Vendor Defined Data" ); break;
					case kHIDUsage_PID_SetCustomForceReport: sprintf( outCStrName, "Set Custom Force Report" ); break;
					case kHIDUsage_PID_CustomForceDataOffset: sprintf( outCStrName, "Custom Force Data Offset" ); break;
					case kHIDUsage_PID_SampleCount: sprintf( outCStrName, "Custom Force Sample Count" ); break;
						
					case kHIDUsage_PID_SetPeriodicReport: sprintf( outCStrName, "Set Periodic Report" ); break;
					case kHIDUsage_PID_Offset: sprintf( outCStrName, "Periodic Offset" ); break;
					case kHIDUsage_PID_Magnitude: sprintf( outCStrName, "Periodic Magnitude" ); break;
					case kHIDUsage_PID_Phase: sprintf( outCStrName, "Periodic Phase" ); break;
					case kHIDUsage_PID_Period: sprintf( outCStrName, "Periodic Period" ); break;
						
					case kHIDUsage_PID_SetConstantForceReport: sprintf( outCStrName, "Set Constant Force Report" ); break;
						
					case kHIDUsage_PID_SetRampForceReport: sprintf( outCStrName, "Set Ramp Force Report" ); break;
					case kHIDUsage_PID_RampStart: sprintf( outCStrName, "Ramp Start" ); break;
					case kHIDUsage_PID_RampEnd: sprintf( outCStrName, "Ramp End" ); break;
						
					case kHIDUsage_PID_EffectOperationReport: sprintf( outCStrName, "Effect Operation Report" ); break;
						
					case kHIDUsage_PID_EffectOperation: sprintf( outCStrName, "Effect Operation" ); break;
					case kHIDUsage_PID_OpEffectStart: sprintf( outCStrName, "Op Effect Start" ); break;
					case kHIDUsage_PID_OpEffectStartSolo: sprintf( outCStrName, "Op Effect Start Solo" ); break;
					case kHIDUsage_PID_OpEffectStop: sprintf( outCStrName, "Op Effect Stop" ); break;
					case kHIDUsage_PID_LoopCount: sprintf( outCStrName, "Op Effect Loop Count" ); break;
						
					case kHIDUsage_PID_DeviceGainReport: sprintf( outCStrName, "Device Gain Report" ); break;
					case kHIDUsage_PID_DeviceGain: sprintf( outCStrName, "Device Gain" ); break;
						
					case kHIDUsage_PID_PoolReport: sprintf( outCStrName, "PID Pool Report" ); break;
					case kHIDUsage_PID_RAM_PoolSize: sprintf( outCStrName, "RAM Pool Size" ); break;
					case kHIDUsage_PID_ROM_PoolSize: sprintf( outCStrName, "ROM Pool Size" ); break;
					case kHIDUsage_PID_ROM_EffectBlockCount: sprintf( outCStrName, "ROM Effect Block Count" ); break;
					case kHIDUsage_PID_SimultaneousEffectsMax: sprintf( outCStrName, "Simultaneous Effects Max" ); break;
					case kHIDUsage_PID_PoolAlignment: sprintf( outCStrName, "Pool Alignment" ); break;
						
					case kHIDUsage_PID_PoolMoveReport: sprintf( outCStrName, "PID Pool Move Report" ); break;
					case kHIDUsage_PID_MoveSource: sprintf( outCStrName, "Move Source" ); break;
					case kHIDUsage_PID_MoveDestination: sprintf( outCStrName, "Move Destination" ); break;
					case kHIDUsage_PID_MoveLength: sprintf( outCStrName, "Move Length" ); break;
						
					case kHIDUsage_PID_BlockLoadReport: sprintf( outCStrName, "PID Block Load Report" ); break;
						
					case kHIDUsage_PID_BlockLoadStatus: sprintf( outCStrName, "Block Load Status" ); break;
					case kHIDUsage_PID_BlockLoadSuccess: sprintf( outCStrName, "Block Load Success" ); break;
					case kHIDUsage_PID_BlockLoadFull: sprintf( outCStrName, "Block Load Full" ); break;
					case kHIDUsage_PID_BlockLoadError: sprintf( outCStrName, "Block Load Error" ); break;
					case kHIDUsage_PID_BlockHandle: sprintf( outCStrName, "Block Handle" ); break;
						
					case kHIDUsage_PID_BlockFreeReport: sprintf( outCStrName, "PID Block Free Report" ); break;
						
					case kHIDUsage_PID_TypeSpecificBlockHandle: sprintf( outCStrName, "Type Specific Block Handle" ); break;
						
					case kHIDUsage_PID_StateReport: sprintf( outCStrName, "PID State Report" ); break;
					case kHIDUsage_PID_EffectPlaying: sprintf( outCStrName, "Effect Playing" ); break;
						
					case kHIDUsage_PID_DeviceControlReport: sprintf( outCStrName, "PID Device Control Report" ); break;
						
					case kHIDUsage_PID_DeviceControl: sprintf( outCStrName, "PID Device Control" ); break;
					case kHIDUsage_PID_DC_EnableActuators: sprintf( outCStrName, "Device Control Enable Actuators" ); break;
					case kHIDUsage_PID_DC_DisableActuators: sprintf( outCStrName, "Device Control Disable Actuators" ); break;
					case kHIDUsage_PID_DC_StopAllEffects: sprintf( outCStrName, "Device Control Stop All Effects" ); break;
					case kHIDUsage_PID_DC_DeviceReset: sprintf( outCStrName, "Device Control Reset" ); break;
					case kHIDUsage_PID_DC_DevicePause: sprintf( outCStrName, "Device Control Pause" ); break;
					case kHIDUsage_PID_DC_DeviceContinue: sprintf( outCStrName, "Device Control Continue" ); break;
					case kHIDUsage_PID_DevicePaused: sprintf( outCStrName, "Device Paused" ); break;
					case kHIDUsage_PID_ActuatorsEnabled: sprintf( outCStrName, "Actuators Enabled" ); break;
					case kHIDUsage_PID_SafetySwitch: sprintf( outCStrName, "Safety Switch" ); break;
					case kHIDUsage_PID_ActuatorOverrideSwitch: sprintf( outCStrName, "Actuator Override Switch" ); break;
					case kHIDUsage_PID_ActuatorPower: sprintf( outCStrName, "Actuator Power" ); break;
					case kHIDUsage_PID_StartDelay: sprintf( outCStrName, "Start Delay" ); break;
						
					case kHIDUsage_PID_ParameterBlockSize: sprintf( outCStrName, "Parameter Block Size" ); break;
					case kHIDUsage_PID_DeviceManagedPool: sprintf( outCStrName, "Device Managed Pool" ); break;
					case kHIDUsage_PID_SharedParameterBlocks: sprintf( outCStrName, "Shared Parameter Blocks" ); break;
						
					case kHIDUsage_PID_CreateNewEffectReport: sprintf( outCStrName, "Create New Effect Report" ); break;
					case kHIDUsage_PID_RAM_PoolAvailable: sprintf( outCStrName, "RAM Pool Available" ); break;
					default: sprintf( outCStrName, "PID Usage 0x%lx", inUsage ); break;
				}
			}
			break;
		}
		case kHIDPage_Unicode: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Unicode Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_PowerDevice: {
			if ( ( ( inUsage >= 0x06 ) && ( inUsage <= 0x0F ) ) || ( ( inUsage >= 0x26 ) && ( inUsage <= 0x2F ) ) ||
				 ( ( inUsage >= 0x39 ) && ( inUsage <= 0x3F ) ) || ( ( inUsage >= 0x48 ) && ( inUsage <= 0x4F ) ) ||
				 ( ( inUsage >= 0x58 ) && ( inUsage <= 0x5F ) ) || ( inUsage == 0x6A ) ||
				 ( ( inUsage >= 0x74 ) && ( inUsage <= 0xFC ) ) ) {
				sprintf( outCStrName, "Power Device Reserved" );
			} else {
				switch( inUsage ) {
					case kHIDUsage_PD_Undefined: sprintf( outCStrName, "Power Device Undefined Usage" ); break;
					case kHIDUsage_PD_iName: sprintf( outCStrName, "Power Device Name Index" ); break;
					case kHIDUsage_PD_PresentStatus: sprintf( outCStrName, "Power Device Present Status" ); break;
					case kHIDUsage_PD_ChangedStatus: sprintf( outCStrName, "Power Device Changed Status" ); break;
					case kHIDUsage_PD_UPS: sprintf( outCStrName, "Uninterruptible Power Supply" ); break;
					case kHIDUsage_PD_PowerSupply: sprintf( outCStrName, "Power Supply" ); break;
						
					case kHIDUsage_PD_BatterySystem: sprintf( outCStrName, "Battery System Power Module" ); break;
					case kHIDUsage_PD_BatterySystemID: sprintf( outCStrName, "Battery System ID" ); break;
					case kHIDUsage_PD_Battery: sprintf( outCStrName, "Battery" ); break;
					case kHIDUsage_PD_BatteryID: sprintf( outCStrName, "Battery ID" ); break;
					case kHIDUsage_PD_Charger: sprintf( outCStrName, "Charger" ); break;
					case kHIDUsage_PD_ChargerID: sprintf( outCStrName, "Charger ID" ); break;
					case kHIDUsage_PD_PowerConverter: sprintf( outCStrName, "Power Converter Power Module" ); break;
					case kHIDUsage_PD_PowerConverterID: sprintf( outCStrName, "Power Converter ID" ); break;
					case kHIDUsage_PD_OutletSystem: sprintf( outCStrName, "Outlet System power module" ); break;
					case kHIDUsage_PD_OutletSystemID: sprintf( outCStrName, "Outlet System ID" ); break;
					case kHIDUsage_PD_Input: sprintf( outCStrName, "Power Device Input" ); break;
					case kHIDUsage_PD_InputID: sprintf( outCStrName, "Power Device Input ID" ); break;
					case kHIDUsage_PD_Output: sprintf( outCStrName, "Power Device Output" ); break;
					case kHIDUsage_PD_OutputID: sprintf( outCStrName, "Power Device Output ID" ); break;
					case kHIDUsage_PD_Flow: sprintf( outCStrName, "Power Device Flow" ); break;
					case kHIDUsage_PD_FlowID: sprintf( outCStrName, "Power Device Flow ID" ); break;
					case kHIDUsage_PD_Outlet: sprintf( outCStrName, "Power Device Outlet" ); break;
					case kHIDUsage_PD_OutletID: sprintf( outCStrName, "Power Device Outlet ID" ); break;
					case kHIDUsage_PD_Gang: sprintf( outCStrName, "Power Device Gang" ); break;
					case kHIDUsage_PD_GangID: sprintf( outCStrName, "Power Device Gang ID" ); break;
					case kHIDUsage_PD_PowerSummary: sprintf( outCStrName, "Power Device Power Summary" ); break;
					case kHIDUsage_PD_PowerSummaryID: sprintf( outCStrName, "Power Device Power Summary ID" ); break;
						
					case kHIDUsage_PD_Voltage: sprintf( outCStrName, "Power Device Voltage" ); break;
					case kHIDUsage_PD_Current: sprintf( outCStrName, "Power Device Current" ); break;
					case kHIDUsage_PD_Frequency: sprintf( outCStrName, "Power Device Frequency" ); break;
					case kHIDUsage_PD_ApparentPower: sprintf( outCStrName, "Power Device Apparent Power" ); break;
					case kHIDUsage_PD_ActivePower: sprintf( outCStrName, "Power Device RMS Power" ); break;
					case kHIDUsage_PD_PercentLoad: sprintf( outCStrName, "Power Device Percent Load" ); break;
					case kHIDUsage_PD_Temperature: sprintf( outCStrName, "Power Device Temperature" ); break;
					case kHIDUsage_PD_Humidity: sprintf( outCStrName, "Power Device Humidity" ); break;
					case kHIDUsage_PD_BadCount: sprintf( outCStrName, "Power Device Bad Condition Count" ); break;
						
					case kHIDUsage_PD_ConfigVoltage: sprintf( outCStrName, "Power Device Nominal Voltage" ); break;
					case kHIDUsage_PD_ConfigCurrent: sprintf( outCStrName, "Power Device Nominal Current" ); break;
					case kHIDUsage_PD_ConfigFrequency: sprintf( outCStrName, "Power Device Nominal Frequency" ); break;
					case kHIDUsage_PD_ConfigApparentPower: sprintf( outCStrName, "Power Device Nominal Apparent Power" ); break;
					case kHIDUsage_PD_ConfigActivePower: sprintf( outCStrName, "Power Device Nominal RMS Power" ); break;
					case kHIDUsage_PD_ConfigPercentLoad: sprintf( outCStrName, "Power Device Nominal Percent Load" ); break;
					case kHIDUsage_PD_ConfigTemperature: sprintf( outCStrName, "Power Device Nominal Temperature" ); break;
						
					case kHIDUsage_PD_ConfigHumidity: sprintf( outCStrName, "Power Device Nominal Humidity" ); break;
					case kHIDUsage_PD_SwitchOnControl: sprintf( outCStrName, "Power Device Switch On Control" ); break;
					case kHIDUsage_PD_SwitchOffControl: sprintf( outCStrName, "Power Device Switch Off Control" ); break;
					case kHIDUsage_PD_ToggleControl: sprintf( outCStrName, "Power Device Toogle Sequence Control" ); break;
					case kHIDUsage_PD_LowVoltageTransfer: sprintf( outCStrName, "Power Device Min Transfer Voltage" ); break;
					case kHIDUsage_PD_HighVoltageTransfer: sprintf( outCStrName, "Power Device Max Transfer Voltage" ); break;
					case kHIDUsage_PD_DelayBeforeReboot: sprintf( outCStrName, "Power Device Delay Before Reboot" ); break;
					case kHIDUsage_PD_DelayBeforeStartup: sprintf( outCStrName, "Power Device Delay Before Startup" ); break;
					case kHIDUsage_PD_DelayBeforeShutdown: sprintf( outCStrName, "Power Device Delay Before Shutdown" ); break;
					case kHIDUsage_PD_Test: sprintf( outCStrName, "Power Device Test Request/Result" ); break;
					case kHIDUsage_PD_ModuleReset: sprintf( outCStrName, "Power Device Reset Request/Result" ); break;
					case kHIDUsage_PD_AudibleAlarmControl: sprintf( outCStrName, "Power Device Audible Alarm Control" ); break;
						
					case kHIDUsage_PD_Present: sprintf( outCStrName, "Power Device Present" ); break;
					case kHIDUsage_PD_Good: sprintf( outCStrName, "Power Device Good" ); break;
					case kHIDUsage_PD_InternalFailure: sprintf( outCStrName, "Power Device Internal Failure" ); break;
					case kHIDUsage_PD_VoltageOutOfRange: sprintf( outCStrName, "Power Device Voltage Out Of Range" ); break;
					case kHIDUsage_PD_FrequencyOutOfRange: sprintf( outCStrName, "Power Device Frequency Out Of Range" ); break;
					case kHIDUsage_PD_Overload: sprintf( outCStrName, "Power Device Overload" ); break;
					case kHIDUsage_PD_OverCharged: sprintf( outCStrName, "Power Device Over Charged" ); break;
					case kHIDUsage_PD_OverTemperature: sprintf( outCStrName, "Power Device Over Temperature" ); break;
					case kHIDUsage_PD_ShutdownRequested: sprintf( outCStrName, "Power Device Shutdown Requested" ); break;
						
					case kHIDUsage_PD_ShutdownImminent: sprintf( outCStrName, "Power Device Shutdown Imminent" ); break;
					case kHIDUsage_PD_SwitchOnOff: sprintf( outCStrName, "Power Device On/Off Switch Status" ); break;
					case kHIDUsage_PD_Switchable: sprintf( outCStrName, "Power Device Switchable" ); break;
					case kHIDUsage_PD_Used: sprintf( outCStrName, "Power Device Used" ); break;
					case kHIDUsage_PD_Boost: sprintf( outCStrName, "Power Device Boosted" ); break;
					case kHIDUsage_PD_Buck: sprintf( outCStrName, "Power Device Bucked" ); break;
					case kHIDUsage_PD_Initialized: sprintf( outCStrName, "Power Device Initialized" ); break;
					case kHIDUsage_PD_Tested: sprintf( outCStrName, "Power Device Tested" ); break;
					case kHIDUsage_PD_AwaitingPower: sprintf( outCStrName, "Power Device Awaiting Power" ); break;
					case kHIDUsage_PD_CommunicationLost: sprintf( outCStrName, "Power Device Communication Lost" ); break;
						
					case kHIDUsage_PD_iManufacturer: sprintf( outCStrName, "Power Device Manufacturer String Index" ); break;
					case kHIDUsage_PD_iProduct: sprintf( outCStrName, "Power Device Product String Index" ); break;
					case kHIDUsage_PD_iserialNumber: sprintf( outCStrName, "Power Device Serial Number String Index" ); break;
					default: sprintf( outCStrName, "Power Device Usage 0x%lx", inUsage ); break;
				}
			}
			break;
		}
		case kHIDPage_BatterySystem: {
			if ( ( ( inUsage >= 0x0A ) && ( inUsage <= 0x0F ) ) || ( ( inUsage >= 0x1E ) && ( inUsage <= 0x27 ) ) ||
				 ( ( inUsage >= 0x30 ) && ( inUsage <= 0x3F ) ) || ( ( inUsage >= 0x4C ) && ( inUsage <= 0x5F ) ) ||
				 ( ( inUsage >= 0x6C ) && ( inUsage <= 0x7F ) ) || ( ( inUsage >= 0x90 ) && ( inUsage <= 0xBF ) ) ||
				 ( ( inUsage >= 0xC3 ) && ( inUsage <= 0xCF ) ) || ( ( inUsage >= 0xDD ) && ( inUsage <= 0xEF ) ) ||
				 ( ( inUsage >= 0xF2 ) && ( inUsage <= 0xFF ) ) ) {
				sprintf( outCStrName, "Power Device Reserved" );
			} else {
				switch( inUsage ) {
					case kHIDUsage_BS_Undefined: sprintf( outCStrName, "Battery System Undefined" ); break;
					case kHIDUsage_BS_SMBBatteryMode: sprintf( outCStrName, "SMB Mode" ); break;
					case kHIDUsage_BS_SMBBatteryStatus: sprintf( outCStrName, "SMB Status" ); break;
					case kHIDUsage_BS_SMBAlarmWarning: sprintf( outCStrName, "SMB Alarm Warning" ); break;
					case kHIDUsage_BS_SMBChargerMode: sprintf( outCStrName, "SMB Charger Mode" ); break;
					case kHIDUsage_BS_SMBChargerStatus: sprintf( outCStrName, "SMB Charger Status" ); break;
					case kHIDUsage_BS_SMBChargerSpecInfo: sprintf( outCStrName, "SMB Charger Extended Status" ); break;
					case kHIDUsage_BS_SMBSelectorState: sprintf( outCStrName, "SMB Selector State" ); break;
					case kHIDUsage_BS_SMBSelectorPresets: sprintf( outCStrName, "SMB Selector Presets" ); break;
					case kHIDUsage_BS_SMBSelectorInfo: sprintf( outCStrName, "SMB Selector Info" ); break;
					case kHIDUsage_BS_OptionalMfgFunction1: sprintf( outCStrName, "Battery System Optional SMB Mfg Function 1" ); break;
					case kHIDUsage_BS_OptionalMfgFunction2: sprintf( outCStrName, "Battery System Optional SMB Mfg Function 2" ); break;
					case kHIDUsage_BS_OptionalMfgFunction3: sprintf( outCStrName, "Battery System Optional SMB Mfg Function 3" ); break;
					case kHIDUsage_BS_OptionalMfgFunction4: sprintf( outCStrName, "Battery System Optional SMB Mfg Function 4" ); break;
					case kHIDUsage_BS_OptionalMfgFunction5: sprintf( outCStrName, "Battery System Optional SMB Mfg Function 5" ); break;
					case kHIDUsage_BS_ConnectionToSMBus: sprintf( outCStrName, "Battery System Connection To System Management Bus" ); break;
					case kHIDUsage_BS_OutputConnection: sprintf( outCStrName, "Battery System Output Connection Status" ); break;
					case kHIDUsage_BS_ChargerConnection: sprintf( outCStrName, "Battery System Charger Connection" ); break;
					case kHIDUsage_BS_BatteryInsertion: sprintf( outCStrName, "Battery System Battery Insertion" ); break;
					case kHIDUsage_BS_Usenext: sprintf( outCStrName, "Battery System Use Next" ); break;
					case kHIDUsage_BS_OKToUse: sprintf( outCStrName, "Battery System OK To Use" ); break;
					case kHIDUsage_BS_BatterySupported: sprintf( outCStrName, "Battery System Battery Supported" ); break;
					case kHIDUsage_BS_SelectorRevision: sprintf( outCStrName, "Battery System Selector Revision" ); break;
					case kHIDUsage_BS_ChargingIndicator: sprintf( outCStrName, "Battery System Charging Indicator" ); break;
					case kHIDUsage_BS_ManufacturerAccess: sprintf( outCStrName, "Battery System Manufacturer Access" ); break;
					case kHIDUsage_BS_RemainingCapacityLimit: sprintf( outCStrName, "Battery System Remaining Capacity Limit" ); break;
					case kHIDUsage_BS_RemainingTimeLimit: sprintf( outCStrName, "Battery System Remaining Time Limit" ); break;
					case kHIDUsage_BS_AtRate: sprintf( outCStrName, "Battery System At Rate..." ); break;
					case kHIDUsage_BS_CapacityMode: sprintf( outCStrName, "Battery System Capacity Mode" ); break;
					case kHIDUsage_BS_BroadcastToCharger: sprintf( outCStrName, "Battery System Broadcast To Charger" ); break;
					case kHIDUsage_BS_PrimaryBattery: sprintf( outCStrName, "Battery System Primary Battery" ); break;
					case kHIDUsage_BS_ChargeController: sprintf( outCStrName, "Battery System Charge Controller" ); break;
					case kHIDUsage_BS_TerminateCharge: sprintf( outCStrName, "Battery System Terminate Charge" ); break;
					case kHIDUsage_BS_TerminateDischarge: sprintf( outCStrName, "Battery System Terminate Discharge" ); break;
					case kHIDUsage_BS_BelowRemainingCapacityLimit: sprintf( outCStrName, "Battery System Below Remaining Capacity Limit" ); break;
					case kHIDUsage_BS_RemainingTimeLimitExpired: sprintf( outCStrName, "Battery System Remaining Time Limit Expired" ); break;
					case kHIDUsage_BS_Charging: sprintf( outCStrName, "Battery System Charging" ); break;
					case kHIDUsage_BS_Discharging: sprintf( outCStrName, "Battery System Discharging" ); break;
					case kHIDUsage_BS_FullyCharged: sprintf( outCStrName, "Battery System Fully Charged" ); break;
					case kHIDUsage_BS_FullyDischarged: sprintf( outCStrName, "Battery System Fully Discharged" ); break;
					case kHIDUsage_BS_ConditioningFlag: sprintf( outCStrName, "Battery System Conditioning Flag" ); break;
					case kHIDUsage_BS_AtRateOK: sprintf( outCStrName, "Battery System At Rate OK" ); break;
					case kHIDUsage_BS_SMBErrorCode: sprintf( outCStrName, "Battery System SMB Error Code" ); break;
					case kHIDUsage_BS_NeedReplacement: sprintf( outCStrName, "Battery System Need Replacement" ); break;
					case kHIDUsage_BS_AtRateTimeToFull: sprintf( outCStrName, "Battery System At Rate Time To Full" ); break;
					case kHIDUsage_BS_AtRateTimeToEmpty: sprintf( outCStrName, "Battery System At Rate Time To Empty" ); break;
					case kHIDUsage_BS_AverageCurrent: sprintf( outCStrName, "Battery System Average Current" ); break;
					case kHIDUsage_BS_Maxerror: sprintf( outCStrName, "Battery System Max Error" ); break;
					case kHIDUsage_BS_RelativeStateOfCharge: sprintf( outCStrName, "Battery System Relative State Of Charge" ); break;
					case kHIDUsage_BS_AbsoluteStateOfCharge: sprintf( outCStrName, "Battery System Absolute State Of Charge" ); break;
					case kHIDUsage_BS_RemainingCapacity: sprintf( outCStrName, "Battery System Remaining Capacity" ); break;
					case kHIDUsage_BS_FullChargeCapacity: sprintf( outCStrName, "Battery System Full Charge Capacity" ); break;
					case kHIDUsage_BS_RunTimeToEmpty: sprintf( outCStrName, "Battery System Run Time To Empty" ); break;
					case kHIDUsage_BS_AverageTimeToEmpty: sprintf( outCStrName, "Battery System Average Time To Empty" ); break;
					case kHIDUsage_BS_AverageTimeToFull: sprintf( outCStrName, "Battery System Average Time To Full" ); break;
					case kHIDUsage_BS_CycleCount: sprintf( outCStrName, "Battery System Cycle Count" ); break;
					case kHIDUsage_BS_BattPackModelLevel: sprintf( outCStrName, "Battery System Batt Pack Model Level" ); break;
					case kHIDUsage_BS_InternalChargeController: sprintf( outCStrName, "Battery System Internal Charge Controller" ); break;
					case kHIDUsage_BS_PrimaryBatterySupport: sprintf( outCStrName, "Battery System Primary Battery Support" ); break;
					case kHIDUsage_BS_DesignCapacity: sprintf( outCStrName, "Battery System Design Capacity" ); break;
					case kHIDUsage_BS_SpecificationInfo: sprintf( outCStrName, "Battery System Specification Info" ); break;
					case kHIDUsage_BS_ManufacturerDate: sprintf( outCStrName, "Battery System Manufacturer Date" ); break;
					case kHIDUsage_BS_SerialNumber: sprintf( outCStrName, "Battery System Serial Number" ); break;
					case kHIDUsage_BS_iManufacturerName: sprintf( outCStrName, "Battery System Manufacturer Name Index" ); break;
					case kHIDUsage_BS_iDevicename: sprintf( outCStrName, "Battery System Device Name Index" ); break;
					case kHIDUsage_BS_iDeviceChemistry: sprintf( outCStrName, "Battery System Device Chemistry Index" ); break;
					case kHIDUsage_BS_ManufacturerData: sprintf( outCStrName, "Battery System Manufacturer Data" ); break;
					case kHIDUsage_BS_Rechargable: sprintf( outCStrName, "Battery System Rechargable" ); break;
					case kHIDUsage_BS_WarningCapacityLimit: sprintf( outCStrName, "Battery System Warning Capacity Limit" ); break;
					case kHIDUsage_BS_CapacityGranularity1: sprintf( outCStrName, "Battery System Capacity Granularity 1" ); break;
					case kHIDUsage_BS_CapacityGranularity2: sprintf( outCStrName, "Battery System Capacity Granularity 2" ); break;
					case kHIDUsage_BS_iOEMInformation: sprintf( outCStrName, "Battery System OEM Information Index" ); break;
					case kHIDUsage_BS_InhibitCharge: sprintf( outCStrName, "Battery System Inhibit Charge" ); break;
					case kHIDUsage_BS_EnablePolling: sprintf( outCStrName, "Battery System Enable Polling" ); break;
					case kHIDUsage_BS_ResetToZero: sprintf( outCStrName, "Battery System Reset To Zero" ); break;
					case kHIDUsage_BS_ACPresent: sprintf( outCStrName, "Battery System AC Present" ); break;
					case kHIDUsage_BS_BatteryPresent: sprintf( outCStrName, "Battery System Battery Present" ); break;
					case kHIDUsage_BS_PowerFail: sprintf( outCStrName, "Battery System Power Fail" ); break;
					case kHIDUsage_BS_AlarmInhibited: sprintf( outCStrName, "Battery System Alarm Inhibited" ); break;
					case kHIDUsage_BS_ThermistorUnderRange: sprintf( outCStrName, "Battery System Thermistor Under Range" ); break;
					case kHIDUsage_BS_ThermistorHot: sprintf( outCStrName, "Battery System Thermistor Hot" ); break;
					case kHIDUsage_BS_ThermistorCold: sprintf( outCStrName, "Battery System Thermistor Cold" ); break;
					case kHIDUsage_BS_ThermistorOverRange: sprintf( outCStrName, "Battery System Thermistor Over Range" ); break;
					case kHIDUsage_BS_VoltageOutOfRange: sprintf( outCStrName, "Battery System Voltage Out Of Range" ); break;
					case kHIDUsage_BS_CurrentOutOfRange: sprintf( outCStrName, "Battery System Current Out Of Range" ); break;
					case kHIDUsage_BS_CurrentNotRegulated: sprintf( outCStrName, "Battery System Current Not Regulated" ); break;
					case kHIDUsage_BS_VoltageNotRegulated: sprintf( outCStrName, "Battery System Voltage Not Regulated" ); break;
					case kHIDUsage_BS_MasterMode: sprintf( outCStrName, "Battery System Master Mode" ); break;
					case kHIDUsage_BS_ChargerSelectorSupport: sprintf( outCStrName, "Battery System Charger Support Selector" ); break;
					case kHIDUsage_BS_ChargerSpec: sprintf( outCStrName, "attery System Charger Specification" ); break;
					case kHIDUsage_BS_Level2: sprintf( outCStrName, "Battery System Charger Level 2" ); break;
					case kHIDUsage_BS_Level3: sprintf( outCStrName, "Battery System Charger Level 3" ); break;
					default: sprintf( outCStrName, "Battery System Usage 0x%lx", inUsage ); break;
				}
			}
			break;
		}
		case kHIDPage_AlphanumericDisplay: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Alphanumeric Display Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_BarCodeScanner: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Bar Code Scanner Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Scale: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Scale Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_CameraControl: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Camera Control Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		case kHIDPage_Arcade: {
			switch( inUsage ) {
				default: sprintf( outCStrName, "Arcade Usage 0x%lx", inUsage ); break;
			}
			break;
		}
		default: {
			if ( inUsagePage >= kHIDPage_VendorDefinedStart ) {
				sprintf( outCStrName, "Vendor Defined Usage 0x%lx", inUsage );
			} else {
				sprintf( outCStrName, "Page: 0x%lx, Usage: 0x%lx", inUsagePage, inUsage );
			}
			break;
		}
	}
}	// HIDGetUsageName
