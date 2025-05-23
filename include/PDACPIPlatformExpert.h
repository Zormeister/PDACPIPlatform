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
//#include "acpi.h"
#include "../include/acpica/acpi.h"
#include "acpica/acpi_fadt.h"

#define super IOService
OSDefineMetaClassAndStructors(PDACPIPlatformExpert, IOService)

// Initialize ACPICA
extern "C" ACPI_STATUS AcpiOsInitialize();
extern "C" ACPI_STATUS AcpiInitializeSubsystem();
extern "C" ACPI_STATUS AcpiInitializeTables(void* tableArray, uint32_t tableCount, uint8_t allowResize);
extern "C" ACPI_STATUS AcpiLoadTables();
extern "C" ACPI_STATUS AcpiEnableSubsystem(uint32_t flags);
extern "C" ACPI_STATUS AcpiInitializeObjects(uint32_t flags);

bool PDACPIPlatformExpert::start(IOService* provider)
{
    IOLog("PDACPIPlatformExpert::start - Initializing ACPICA\n");

    if (!super::start(provider))
        return false;

    // Initialize ACPICA OS Layer
    if (ACPI_FAILURE(AcpiOsInitialize())) {
        IOLog("[ERROR] ACPICA OS Initialization failed.\n");
        return false;
    }

    if (ACPI_FAILURE(AcpiInitializeSubsystem())) {
        IOLog("[ERROR] ACPICA Subsystem Initialization failed.\n");
        return false;
    }

    if (ACPI_FAILURE(AcpiInitializeTables(NULL, 32, FALSE))) {
        IOLog("[ERROR] ACPICA Table Initialization failed.\n");
        return false;
    }

    if (ACPI_FAILURE(AcpiLoadTables())) {
        IOLog("[ERROR] ACPICA Table Loading failed.\n");
        return false;
    }

    if (ACPI_FAILURE(AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION))) {
        IOLog("[ERROR] ACPICA Subsystem Enable failed.\n");
        return false;
    }

    if (ACPI_FAILURE(AcpiInitializeObjects(ACPI_FULL_INITIALIZATION))) {
        IOLog("[ERROR] ACPICA Object Initialization failed.\n");
        return false;
    }

    IOLog("[SUCCESS] ACPICA Initialized successfully.\n");

    registerService();
    return true;
}

void PDACPIPlatformExpert::stop(IOService* provider)
{
    IOLog("PDACPIPlatformExpert::stop\n");
    super::stop(provider);
}

void PDACPIPlatformExpert::handleSleepRequest()
{
    IOLog("PDACPIPlatformExpert::handleSleepRequest - Entering Sleep\n");
    AcpiEnterSleepStatePrep(3); // S3 (Sleep)
    AcpiEnterSleepState(3);
}

void PDACPIPlatformExpert::handleWakeRequest()
{
    IOLog("PDACPIPlatformExpert::handleWakeRequest - Exiting Sleep\n");
    AcpiLeaveSleepState(3);
}

void PDACPIPlatformExpert::performACPIPowerOff()
{
    IOLog("PDACPIPlatformExpert::performACPIPowerOff - Shutting down\n");
    AcpiEnterSleepStatePrep(5); // S5 (Power Off)
    AcpiEnterSleepState(5);
}

void PDACPIPlatformExpert::evaluateExample()
{
    ACPI_OBJECT obj;
    ACPI_BUFFER buffer = { sizeof(obj), &obj };

    if (ACPI_FAILURE(AcpiEvaluateObject(NULL, (char*)"_SB.PCI0.SBRG.SIO", NULL, &buffer)))
    {
        IOLog("[ERROR] ACPI Evaluation of _SB.PCI0.SBRG.SIO failed.\n");
    }
    else
    {
        IOLog("[SUCCESS] ACPI Evaluation of _SB.PCI0.SBRG.SIO succeeded: Value = %llu\n", obj.Integer.Value);
    }
}
