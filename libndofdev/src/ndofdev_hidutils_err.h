/*
 @file ndofdev_hidutils_err.h
 @brief This is a slightly modified subset of the original "HID Utilities" 
        sample code from Apple. 
 
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

#ifndef HID_Error_Handler_h
#define HID_Error_Handler_h

#define kVerboseErrors 1

/*****************************************************/
#pragma mark - includes & imports
#include <Carbon/Carbon.h>

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
#pragma mark - exported function prototypes
    
// Error reporter, can be set to report however the application desires
extern void HIDReportError( const char* inErrorCStr );

// Error with numeric code reporter, can be set to report however the application desires
extern void HIDReportErrorNum( const char* inErrorCStr, long inErrorNum );
    
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

#endif /* HID_Error_Handler_h */
