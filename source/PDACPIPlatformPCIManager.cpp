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

#include "../include/PDACPIPlatformPCIManager.h"
#include <IOKit/IOLib.h>
#include <IOKit/IORegistryIterator.h>
#include <IOKit/IORegistryEntry.h>

#define super IOService
OSDefineMetaClassAndStructors(PDACPIPlatformPCIManager, IOService)

bool PDACPIPlatformPCIManager::start(IOService* provider)
{
    IOLog("PDACPIPlatformPCIManager::start\n");
    if (!super::start(provider))
        return false;

    matchAndLogPCIDevices();
    registerService();
    return true;
}

void PDACPIPlatformPCIManager::stop(IOService* provider)
{
    IOLog("PDACPIPlatformPCIManager::stop\n");
    super::stop(provider);
}

void PDACPIPlatformPCIManager::matchAndLogPCIDevices()
{
    IOLog("PDACPIPlatformPCIManager::matchAndLogPCIDevices\n");

    IORegistryIterator* iter = IORegistryIterator::iterateOver(this->getProvider(), gIOACPIPlane, kIORegistryIterateRecursively);
    if (!iter) return;

    IORegistryEntry* entry;
    while ((entry = iter->getNextObject())) {
        OSNumber* classCode = OSDynamicCast(OSNumber, entry->getProperty("ClassCode"));
        OSNumber* subClass = OSDynamicCast(OSNumber, entry->getProperty("SubClass"));
        OSNumber* vendorID = OSDynamicCast(OSNumber, entry->getProperty("VendorID"));
        OSNumber* deviceID = OSDynamicCast(OSNumber, entry->getProperty("DeviceID"));

        if (classCode && vendorID && deviceID) {
            uint8_t cls = classCode->unsigned8BitValue();
            uint8_t subcls = subClass ? subClass->unsigned8BitValue() : 0;
            uint16_t ven = vendorID->unsigned16BitValue();
            uint16_t dev = deviceID->unsigned16BitValue();

            IOLog("PCI Device: Vendor=0x%04x Device=0x%04x Class=0x%02x SubClass=0x%02x\n",
                  ven, dev, cls, subcls);

            if (cls == 0x01 && subcls == 0x06)
                IOLog(" → Detected SATA Controller\n");
            else if (cls == 0x01 && subcls == 0x08)
                IOLog(" → Detected NVMe Controller\n");
            else if (cls == 0x0C && subcls == 0x03)
                IOLog(" → Detected USB Controller\n");
            else if (cls == 0x02)
                IOLog(" → Detected Network Controller\n");
            else if (cls == 0x03)
                IOLog(" → Detected Display Controller\n");
        }
    }

    iter->release();
}
