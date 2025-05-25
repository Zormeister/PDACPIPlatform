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

#ifndef _PDACPI_PLATFORMEXPERT_H_
#define _PDACPI_PLATFORMEXPERT_H_

#include <IOKit/acpi/IOACPIPlatformExpert.h>
#include <IOKit/rtc/IORTCController.h>

class PDACPIPlatformExpert : public IOACPIPlatformExpert {
    OSDeclareDefaultStructors(PDACPIPlatformExpert);

public:
    /* IOService overrides */
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    
    virtual OSObject *copyProperty(const char *property) const override;
    
    /* IOACPIPlatformExpert overrides */
    virtual const OSData *getACPITableData(const char *name, UInt32 TableIndex) override;
    
    virtual IOReturn registerAddressSpaceHandler(
                                   IOACPIPlatformDevice *device,
                                   IOACPIAddressSpaceID spaceID,
                                   IOACPIAddressSpaceHandler handler,
                                   void * context,
                                   IOOptionBits options) override;
    
    virtual void unregisterAddressSpaceHandler(
                                   IOACPIPlatformDevice *device,
                                   IOACPIAddressSpaceID spaceID,
                                   IOACPIAddressSpaceHandler handler,
                                   IOOptionBits options) override;

    /* internal functions */
private:
    bool initializeACPICA(void);
    void performACPIPowerOff(void);
    bool catalogACPITables(void);
    bool fetchPCIData(void);
    UInt32 getACPITableCount(const char *name);
    void createCPUNubs(void); /* walk MADT and enumerate the CPU devices/objects available. */
    
private:
    OSDictionary *m_tableDict;
    
    /* PIO == ACPIPE, MMIO == ACPIPE, PCI CFG == ACPIPE, we have handlers for all of these. */
    IOACPIAddressSpaceHandler m_ecSpaceHandler;
    void *m_ecSpaceContext;
    IOACPIAddressSpaceHandler m_smbusSpaceHandler;
    void *m_smbusSpaceContext;
    IORTC *m_localRTC;
    IOLock *m_rtcLock;
};

#endif
