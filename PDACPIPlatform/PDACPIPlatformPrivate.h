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
#include "acpi.h"

#define PDACPIPLATFORMUC_TYPE 'pdac'

enum PDACPIUserClientMethods {
    kPDACPIPlatformUserClientGetTableByName = 1,
    kPDACPIPlatformUserClientGetTableByIndex = 2,
    kPDACPIPlatformUserClientGetTableByAddress = 3,
};

struct PDACPIUCGetTableByNameInput {
    char tableName[32];
};

struct PDACPIUCGetTableByIndexInput {
    size_t index;
};

struct PDACPIUCGetTableByAddressInput {
    ACPI_PHYSICAL_ADDRESS address;
};

struct PDACPIUCGetTableOutput {
    size_t tableLength;
    uint8_t table[0];
};

#endif /* _PDACPIPLATFORM_PRIVATE_H */
