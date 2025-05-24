/*
*
* Copyright (c) 2007-Present The PureDarwin Project.
* All rights reserved.
*
* @PUREDARWIN_LICENSE_HEADER_START@
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
* IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* @PUREDARWIN_LICENSE_HEADER_END@
*
* PDACPIPlatform Open Source Version of Apple's AppleACPIPlatform
* Created by github.com/csekel (InSaneDarwin)
*
* This specific file was created by Zormeister
*/

#ifndef __ACDARWIN_H__
#define __ACDARWIN_H__

#if KERNEL

#include <mach/semaphore.h>
#include <IOKit/IOLib.h>

#define ACPI_USE_LOCAL_CACHE

#define ACPI_USE_GPE_POLLING

#define ACPI_SEMAPHORE semaphore_t
#define ACPI_SPINLOCK IOSimpleLock *

#define ACPI_USE_SYSTEM_CLIBRARY

#define ACPI_MSG_ERROR          "ACPI Error: "
#define ACPI_MSG_EXCEPTION      "ACPI Exception: "
#define ACPI_MSG_WARNING        "ACPI Warning: "
#define ACPI_MSG_INFO           "ACPI: "

#define ACPI_MSG_BIOS_ERROR     "ACPI BIOS Error (bug): "
#define ACPI_MSG_BIOS_WARNING   "ACPI BIOS Warning (bug): "

#include <stdint.h>

#define ACPI_DEBUG_OUTPUT
#define ACPI_DISASSEMBLER
#define ACPI_DEBUGGER

#define vsnprintf AcpiUtVsnprintf
#define snprintf AcpiUtSnprintf

#if __LP64__
#define ACPI_MACHINE_WIDTH 64
#else
#define ACPI_MACHINE_WIDTH 32
#endif

#define stderr NULL
#define stdout NULL

#define EOF -1

extern const unsigned char AcpiGbl_Ctypes[];

#define _ACPI_XA     0x00    /* extra alphabetic - not supported */
#define _ACPI_XS     0x40    /* extra space */
#define _ACPI_BB     0x00    /* BEL, BS, etc. - not supported */
#define _ACPI_CN     0x20    /* CR, FF, HT, NL, VT */
#define _ACPI_DI     0x04    /* '0'-'9' */
#define _ACPI_LO     0x02    /* 'a'-'z' */
#define _ACPI_PU     0x10    /* punctuation */
#define _ACPI_SP     0x08    /* space, tab, CR, LF, VT, FF */
#define _ACPI_UP     0x01    /* 'A'-'Z' */
#define _ACPI_XD     0x80    /* '0'-'9', 'A'-'F', 'a'-'f' */

#define isdigit(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_DI))
#define isspace(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_SP))
#define isxdigit(c) (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_XD))
#define isupper(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_UP))
#define islower(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_LO))
#define isprint(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_LO | _ACPI_UP | _ACPI_DI | _ACPI_XS | _ACPI_PU))
#define isalpha(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_LO | _ACPI_UP))

// HACK: kernel string.h is borked on at *least* 10.15.
#if defined(strcpy) && __has_builtin(__builtin___strcpy_chk)
#undef strcpy

#define strcpy(dest, src) __builtin___strcpy_chk(dest, src, XNU_BOS(dest, 1))
#endif

#endif

#endif /* __ACDARWIN_H__ */
