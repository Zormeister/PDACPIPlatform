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

#include "../include/PDACPIPCIDevice.h"
#include <IOKit/IOLib.h>
#include "pci_config_access.h"

#define super IOService
OSDefineMetaClassAndStructors(PDACPIPCIDevice, IOService)

bool PDACPIPCIDevice::initWithHandle(ACPI_HANDLE handle)
{
    if (!super::init())
        return false;
    acpiHandle = handle;
    deviceNumber = 0;
    functionNumber = 0;
    return true;
}

IOService* PDACPIPCIDevice::probe(IOService* provider, SInt32* score)
{
    return super::probe(provider, score);
}

bool PDACPIPCIDevice::start(IOService* provider)
{
    IOLog("PDACPIPCIDevice::start\n");
    if (!super::start(provider))
        return false;

    ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    if (ACPI_SUCCESS(AcpiEvaluateObject(acpiHandle, (char*)"_ADR", NULL, &buffer))) {
        ACPI_OBJECT* obj = (ACPI_OBJECT*)buffer.Pointer;
        if (obj && obj->Type == ACPI_TYPE_INTEGER) {
            uint64_t adr = obj->Integer.Value;
            deviceNumber = (adr >> 16) & 0xFFFF;
            functionNumber = adr & 0xFFFF;

            setProperty("Device", deviceNumber, 32);
            setProperty("Function", functionNumber, 32);
        }
        AcpiOsFree(buffer.Pointer);
    }

#if defined(__x86_64__) || defined(__i386__)
    uint8_t busNumber = 0;
    uint32_t id = pciConfigRead32(busNumber, deviceNumber, functionNumber, 0x00);
    uint16_t vendorID = id & 0xFFFF;
    uint16_t deviceID = (id >> 16) & 0xFFFF;

    setProperty("VendorID", vendorID, 16);
    setProperty("DeviceID", deviceID, 16);

    uint32_t classReg = pciConfigRead32(busNumber, deviceNumber, functionNumber, 0x08);
    uint8_t classCode = (classReg >> 24) & 0xFF;
    uint8_t subclass = (classReg >> 16) & 0xFF;

    setProperty("ClassCode", classCode, 8);
    setProperty("SubClass", subclass, 8);
#endif

    registerService();
    return true;
}

void PDACPIPCIDevice::stop(IOService* provider)
{
    IOLog("PDACPIPCIDevice::stop\n");
    super::stop(provider);
}
