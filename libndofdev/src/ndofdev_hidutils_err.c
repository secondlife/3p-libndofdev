/*
 @file ndofdev_hidutils_err.c
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

#include <stdio.h>
#include "ndofdev_hidutils_err.h"
 
/*************************************************************************
* HIDReportErrorNum( inErrorCStr, inErrorNum )
* Purpose:  output's an error string & number
* Inputs:   inErrorCStr	- the error string
*			inErrorNum	- the error number
*/
void HIDReportErrorNum( const char* inErrorCStr, long inErrorNum )
{
#if kVerboseErrors
	char errMsgCStr [256];

	snprintf( errMsgCStr, sizeof(errMsgCStr), "%s #%ld( 0x%lx )",
             inErrorCStr, inErrorNum, inErrorNum );

	// out as debug string
	HIDReportError( errMsgCStr );
#else
#pragma unused( inErrorCStr, inErrorNum )
#endif // kVerboseErrors
}

/*************************************************************************
* HIDReportErrorNum( inErrorCStr )
* Purpose:  output's an error string
* Inputs:   inErrorCStr	- the error string
* Returns:  nothing
*/
void HIDReportError( const char* inErrorCStr )
{
#if kVerboseErrors
	char errMsgCStr [256];

    strncpy(errMsgCStr, inErrorCStr, sizeof(errMsgCStr));

	// out as debug string
	{
#if 1
        fputs( errMsgCStr, stderr );
#else
		Str255 strErr = "\p";
		CopyCStringToPascal( errMsgCStr, strErr );
		DebugStr( strErr );
#endif
	}
#else
#pragma unused( inErrorCStr )
#endif // kVerboseErrors
}
