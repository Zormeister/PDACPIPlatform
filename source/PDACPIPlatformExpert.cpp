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

#include "../include/PDACPIPlatformExpert.h"
#include <IOKit/IOLib.h>
#include "acpi_fadt.h"

#define super IOService
OSDefineMetaClassAndStructors(PDACPIPlatformExpert, IOService)

bool PDACPIPlatformExpert::start(IOService* provider)
{
    IOLog("PDACPIPlatformExpert::start\n");
    if (!super::start(provider))
        return false;

    pmRootDomain = OSDynamicCast(IOPMrootDomain,
        IOService::waitForService(IOService::serviceMatching("IOPMrootDomain")));

    if (pmRootDomain)
        IOLog("Found IOPMrootDomain\n");

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
    if (pmRootDomain)
        pmRootDomain->sleepSystem(this);
}

void PDACPIPlatformExpert::handleWakeRequest()
{
    if (pmRootDomain)
        pmRootDomain->wakeSystem(this);
}

extern "C" ACPI_TABLE_FADT* getFADT();
extern "C" void outw(uint16_t port, uint16_t val);
extern "C" void IOSleep(uint32_t ms);

void PDACPIPlatformExpert::performACPIPowerOff()
{
    ACPI_TABLE_FADT* fadt = getFADT();
    if (!fadt || fadt->PM1aCntBlock == 0)
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

    outw(fadt->PM1aCntBlock, value);
    if (fadt->PM1bCntBlock)
        outw(fadt->PM1bCntBlock, value);

    IOSleep(10000);
    while (1) asm volatile("hlt");
}
