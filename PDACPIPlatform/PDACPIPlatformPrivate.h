/*
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
 */

#ifndef _PDACPIPLATFORM_PRIVATE_H
#define _PDACPIPLATFORM_PRIVATE_H

#include <IOKit/IOLib.h>

#if __cplusplus
extern "C" {
#endif

#include "acpi.h"

#ifdef __cplusplus
};
#endif

#if KERNEL && PDACPI_BUILDING_PLATFORM

/* PDACPIPlatformExpert private definitions - not for use in userspace nor by any clients */

/* This is so we can store extra data in the future */
#define PDACPI_HANDLE_SIG 'pdah'

struct PDACPIHandle {
    UInt32 sig;
    ACPI_HANDLE fACPICAHandle; /* The ACPICA handle for the object/IOACPIPlatformDevice */
    
    /* Add any additional data as needed, like resources, etc. */
};

#else

#endif

#define PDACPIPLATFORMUC_TYPE 'pdac'

enum PDACPIUserClientMethods {
    kPDACPIPlatformUserClientGetTables = 1
};

struct PDACPITableDescriptor {
    char signature[8];              /* AcpiOsGetTableByName */
    size_t offset;                  /* Offset of the table into the collection */
    ACPI_PHYSICAL_ADDRESS address;  /* AcpiOsGetTableByAddress */
};

struct PDACPIUCGetTableOutput {
    size_t tableLength;
    uint8_t table[0];
};

#endif /* _PDACPIPLATFORM_PRIVATE_H */
