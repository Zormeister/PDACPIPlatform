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
* PDACPIPlatform Open Source Version of Apples AppleACPIPlatform
* Created by github.com/csekel (InSaneDarwin)
*
*/

#include "PDACPIPlatformExpert.h"
#include <IOKit/IOLib.h>
#include "acpica/acpi.h" // For ACPICA APIs

#define super IOACPIPlatformExpert
OSDefineMetaClassAndStructors(PDACPIPlatformExpert, IOACPIPlatformExpert)

bool PDACPIPlatformExpert::initializeACPICA() {
    // Initialize ACPICA OS Layer
    ACPI_STATUS status = AcpiOsInitialize();
    if (ACPI_FAILURE(status)) {
        IOLog("PDACPIPlatformExpert::start - [ERROR] AcpiOsInitialize failed with status %s\n", AcpiFormatException(status));
        return false;
    }

    status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(status)) {
        IOLog("PDACPIPlatformExpert::start - [ERROR] AcpiInitializeSubsystem failed with status %s\n", AcpiFormatException(status));
        AcpiTerminate(); // Cleanup
        return false;
    }

    // For UEFI, passing NULL for InitialTableArray relies on AcpiOsGetRootPointer
    // to find the XSDT from the EFI System Table.
    // InitialTableCount (second param) is ignored by ACPICA when InitialTableArray (first param) is NULL.
    // AllowResize (third param) FALSE is typical.
    status = AcpiInitializeTables(NULL, 0, FALSE);
    if (ACPI_FAILURE(status)) {
        IOLog("PDACPIPlatformExpert::start - [ERROR] AcpiInitializeTables failed with status %s\n", AcpiFormatException(status));
        AcpiTerminate(); // Cleanup
        return false;
    }

    status = AcpiLoadTables();
    if (ACPI_FAILURE(status)) {
        IOLog("PDACPIPlatformExpert::start - [ERROR] AcpiLoadTables failed with status %s\n", AcpiFormatException(status));
        AcpiTerminate(); // Cleanup
        return false;
    }

    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        IOLog("PDACPIPlatformExpert::start - [ERROR] AcpiEnableSubsystem failed with status %s\n", AcpiFormatException(status));
        AcpiTerminate(); // Cleanup
        return false;
    }

    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        IOLog("PDACPIPlatformExpert::start - [ERROR] AcpiInitializeObjects failed with status %s\n", AcpiFormatException(status));
        AcpiTerminate(); // Cleanup
        return false;
    }
}

bool PDACPIPlatformExpert::start(IOService* provider)
{
    IOLog("PDACPIPlatformExpert::start - Initializing ACPICA\n"); // Modified log

    if (!super::start(provider)) {
        IOLog("PDACPIPlatformExpert::start - super::start failed\n");
        return false;
    }

    IOLog("PDACPIPlatformExpert::start - [SUCCESS] ACPICA Initialized successfully.\n");

    // The service should be registered after successful initialization.
    registerService();
    IOLog("PDACPIPlatformExpert::start - Service registered.\n");

    return true;
}

void PDACPIPlatformExpert::stop(IOService* provider)
{
    IOLog("PDACPIPlatformExpert::stop\n");
    super::stop(provider);
}

extern "C" ACPI_TABLE_FADT* getFADT();
extern "C" void outw(uint16_t port, uint16_t val);
extern "C" void IOSleep(uint32_t ms);

void PDACPIPlatformExpert::performACPIPowerOff()
{
    ACPI_TABLE_FADT* fadt = getFADT();
    if (!fadt || fadt->Pm1aControlBlock == 0)
        return;

    ACPI_OBJECT* s5Obj;
    ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    if (ACPI_FAILURE(AcpiEvaluateObject(NULL, (char*)"_S5", NULL, &buffer)))
        return;

    s5Obj = (ACPI_OBJECT*)buffer.Pointer;
    if (!s5Obj || s5Obj->Type != ACPI_TYPE_PACKAGE || s5Obj->Package.Count < 2)
        return;

    ACPI_OBJECT elem1 = s5Obj->Package.Elements[0];
    uint16_t slp_typ = elem1.Integer.Value & 0x7;
    uint16_t slp_en = 1 << 13;
    uint16_t value = (slp_typ << 10) | slp_en;

    outw(fadt->Pm1aControlBlock, value);
    if (fadt->Pm1bControlBlock)
        outw(fadt->Pm1bControlBlock, value);

    IOSleep(10000);
    while (1) asm volatile("hlt");
}
