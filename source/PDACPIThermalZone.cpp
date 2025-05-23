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

#include "../include/PDACPIThermalZone.h"
#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(PDACPIThermalZone, IOService)

bool PDACPIThermalZone::initWithHandle(ACPI_HANDLE handle)
{
    if (!super::init()) return false;
    acpiHandle = handle;
    return true;
}

bool PDACPIThermalZone::start(IOService* provider)
{
    IOLog("PDACPIThermalZone::start\n");
    if (!super::start(provider))
        return false;

    readTripPoints();
    thermalPollingEnabled = true;
    registerService();
    return true;
}

void PDACPIThermalZone::stop(IOService* provider)
{
    IOLog("PDACPIThermalZone::stop\n");
    super::stop(provider);
}

bool PDACPIThermalZone::readTripPoints()
{
    passiveTripTemperature = 0;
    criticalTripTemperature = 0;
    ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    if (ACPI_SUCCESS(AcpiEvaluateObject(acpiHandle, (char*)"_PSV", NULL, &buffer))) {
        ACPI_OBJECT* obj = (ACPI_OBJECT*)buffer.Pointer;
        if (obj && obj->Type == ACPI_TYPE_INTEGER)
            passiveTripTemperature = obj->Integer.Value;
        AcpiOsFree(buffer.Pointer);
    }

    if (ACPI_SUCCESS(AcpiEvaluateObject(acpiHandle, (char*)"_CRT", NULL, &buffer))) {
        ACPI_OBJECT* obj = (ACPI_OBJECT*)buffer.Pointer;
        if (obj && obj->Type == ACPI_TYPE_INTEGER)
            criticalTripTemperature = obj->Integer.Value;
        AcpiOsFree(buffer.Pointer);
    }

    return true;
}

bool PDACPIThermalZone::readCurrentTemperature()
{
    ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    if (ACPI_FAILURE(AcpiEvaluateObject(acpiHandle, (char*)"_TMP", NULL, &buffer))) {
        currentTemperature = 450; // simulate 45.0C
        return true;
    }

    ACPI_OBJECT* obj = (ACPI_OBJECT*)buffer.Pointer;
    if (obj && obj->Type == ACPI_TYPE_INTEGER)
        currentTemperature = obj->Integer.Value;
    AcpiOsFree(buffer.Pointer);
    return true;
}

void PDACPIThermalZone::pollThermalZone()
{
    if (!thermalPollingEnabled || !readCurrentTemperature())
        return;

    IOLog("PDACPIThermalZone: Temperature = %u\n", currentTemperature);

    if (criticalTripTemperature && currentTemperature >= criticalTripTemperature)
        IOLog("PDACPIThermalZone: CRITICAL temp! Should shutdown.\n");
    else if (passiveTripTemperature && currentTemperature >= passiveTripTemperature)
        IOLog("PDACPIThermalZone: PASSIVE trip reached.\n");
}
