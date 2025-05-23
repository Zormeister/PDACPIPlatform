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

#include "../include/PDACPICPU.h"
#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(PDACPICPU, IOService)

bool PDACPICPU::start(IOService* provider)
{
    IOLog("PDACPICPU::start\n");
    if (!super::start(provider))
        return false;

    currentPState = 0;
    pStateArray = OSArray::withCapacity(4);
    cStateArray = OSArray::withCapacity(4);

    // Simulate C/P states for now
    // In future, parse _PSS/_CST ACPI objects

    setProperty("PStateCount", (uint64_t)pStateArray->getCount(), 64);
    setProperty("CStateCount", (uint64_t)cStateArray->getCount(), 64);

    registerService();
    return true;
}

void PDACPICPU::enterC1()
{
    asm volatile("hlt");
}

void PDACPICPU::enterCState(uint32_t cstateType)
{
    switch (cstateType)
    {
        case 1:
            enterC1();
            break;
        default:
            break;
    }
}

bool PDACPICPU::switchToPState(uint32_t index)
{
    if (index == currentPState)
        return true;

    IOLog("PDACPICPU: Switching to P-State %u\n", index);
    currentPState = index;
    return true;
}

uint32_t PDACPICPU::getBestCStateForLatency(uint32_t maxAllowedLatencyUs)
{
    // Simulated: always return C1
    return 1;
}
