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

extern "C" {
#include "acpica/acpi.h" // For ACPICA APIs
#include "acpica/acstruct.h"
#include "acpica/aclocal.h"
#include "acpica/acglobal.h"
}

#define super IOACPIPlatformExpert
OSDefineMetaClassAndStructors(PDACPIPlatformExpert, IOACPIPlatformExpert)

ACPI_TABLE_MADT *gAPICTable;

bool PDACPIPlatformExpert::initializeACPICA() {
    /* No need to init OSL seperately. AcpiInitializeSubsystem calls it as one of it's first calls. */

    ACPI_STATUS status = AcpiInitializeSubsystem();
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
    
    /* the system-type field is derived from the FADT, i think. */
    this->m_provider->setProperty("system-type", &AcpiGbl_FADT.PreferredProfile, 1);
    
    this->catalogACPITables();
    this->fetchPCIData();

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

bool PDACPIPlatformExpert::fetchPCIData() {
    /* Obtain PCI data from MCFG table */
    ACPI_STATUS status = AE_OK;
    ACPI_TABLE_HEADER *TableHeader = NULL;
    ACPI_TABLE_MCFG *mcfgTable = NULL;
    
    status = AcpiGetTable(ACPI_SIG_MCFG, 0, &TableHeader);
    if (ACPI_FAILURE(status)) {
        IOLog("ACPI: Failed to get MCFG table from ACPICA (%s)\n", AcpiFormatException(status));
        return false;
    }
    
    mcfgTable = (ACPI_TABLE_MCFG *)TableHeader;
    /* TODO: finish this */
}


UInt32 PDACPIPlatformExpert::getACPITableCount(const char *name) {
    UInt32 cnt = 0;
    while (cnt++) {
        ACPI_TABLE_HEADER *Tbl;
        ACPI_STATUS stat = AcpiGetTable((char *)name, cnt, &Tbl);
    }
}

bool PDACPIPlatformExpert::catalogACPITables() {
    ACPI_TABLE_HEADER *Table;
    UInt32 tables = AcpiGbl_RootTableList.CurrentTableCount;
    m_tableDict = OSDictionary::withCapacity(tables + 1);
    char name[32];
    
    for (UInt32 i = 0; i < tables; i++) {
        AcpiGetTableByIndex(i, &Table);
        // Now that we have our tables...
        OSData *data = OSData::withBytesNoCopy(Table, Table->Length);
        
        if (m_tableDict->getObject(Table->Signature)) {
            snprintf(name, 32, "%4.4s-%d", Table->Signature);
        }
    }
    
    return true;
}


bool PDACPIPlatformExpert::start(IOService* provider)
{
    IOLog("PDACPIPlatformExpert::start - Initializing ACPICA\n"); // Modified log

    if (!super::start(provider)) {
        IOLog("PDACPIPlatformExpert::start - super::start failed\n");
        return false;
    }
    
    this->m_provider = OSDynamicCast(IOPlatformExpertDevice, provider);
    
    /* Respond to certain boot arguemnts */
    PE_parse_boot_argn("acpi_layer", &AcpiDbgLayer, 4);
    PE_parse_boot_argn("acpi_level", &AcpiDbgLevel, 4);

    if (!this->initializeACPICA()) {
        panic("ACPI: ACPICA layer failed to initialize.\n");
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

IOReturn PDACPIPlatformExpert::registerAddressSpaceHandler(IOACPIPlatformDevice *,
                                                           IOACPIAddressSpaceID spaceID,
                                                           IOACPIAddressSpaceHandler Handler,
                                                           void *context, IOOptionBits options)
{
    /* We don't care about the specific device; we care about the handler itself */
    switch (spaceID) {
        case kIOACPIAddressSpaceIDEmbeddedController:
            this->m_ecSpaceHandler = Handler;
            this->m_ecSpaceContext = context;
            IOLog("PDACPIPlatformExpert::%s: Registered handler for the EC address space\n", __PRETTY_FUNCTION__);
            return kIOReturnSuccess;
        case kIOACPIAddressSpaceIDSMBus:
            this->m_smbusSpaceHandler = Handler;
            this->m_smbusSpaceContext = context;
            IOLog("PDACPIPlatformExpert::%s: Registered handler for the SMBus address space\n", __PRETTY_FUNCTION__);
            return kIOReturnSuccess;
        default:
            IOLog("PDACPIPlatformExpert::%s: Invalid attempt at registering an address space handler\n", __PRETTY_FUNCTION__);
            return kIOReturnInvalid;
    }
}

void PDACPIPlatformExpert::unregisterAddressSpaceHandler(IOACPIPlatformDevice *,
                                                         IOACPIAddressSpaceID spaceID,
                                                         IOACPIAddressSpaceHandler,
                                                         IOOptionBits) {
    /* Remove the specified handlers */
    switch (spaceID) {
        case kIOACPIAddressSpaceIDEmbeddedController:
            this->m_ecSpaceHandler = nullptr;
            this->m_ecSpaceContext = nullptr;
            IOLog("PDACPIPlatformExpert::%s: Removed handler for the EC address space\n", __PRETTY_FUNCTION__);
            return;
        case kIOACPIAddressSpaceIDSMBus:
            this->m_smbusSpaceHandler = nullptr;
            this->m_smbusSpaceContext = nullptr;
            IOLog("PDACPIPlatformExpert::%s: Removed handler for the SMBus address space\n", __PRETTY_FUNCTION__);
            return;
        default:
            IOLog("PDACPIPlatformExpert::%s: Invalid attempt at removing an address space handler\n", __PRETTY_FUNCTION__);
            break;
    }
}

IOReturn PDACPIPlatformExpert::readAddressSpace(UInt64 *value,
                                                IOACPIAddressSpaceID spaceID,
                                                IOACPIAddress address,
                                                UInt32 bitWidth,
                                                UInt32 bitOffset,
                                                IOOptionBits options) {
    switch (spaceID) {
        case kIOACPIAddressSpaceIDSystemMemory: {
            ACPI_STATUS status = AcpiOsReadMemory(address.addr64, value, bitWidth);
            if (ACPI_FAILURE(status)) {
                return kIOReturnError;
            } else {
                return kIOReturnSuccess;
            }
            break;
        }
        case kIOACPIAddressSpaceIDSystemIO: {
            ACPI_STATUS status = AcpiOsReadPort((ACPI_IO_ADDRESS)address.addr64, (UInt32 *)value, bitWidth);
            if (ACPI_FAILURE(status)) {
                return kIOReturnError;
            } else {
                return kIOReturnSuccess;
            }
            break;
        }
        case kIOACPIAddressSpaceIDEmbeddedController:
            if (this->m_ecSpaceHandler && this->m_ecSpaceContext) {
                return this->m_ecSpaceHandler(kIOACPIAddressSpaceOpRead, address, value, bitWidth, bitOffset, this->m_ecSpaceContext);
            }
        case kIOACPIAddressSpaceIDSMBus:
            if (this->m_smbusSpaceHandler && this->m_smbusSpaceContext) {
                return this->m_smbusSpaceHandler(kIOACPIAddressSpaceOpRead, address, value, bitWidth, bitOffset, this->m_smbusSpaceContext);
            }
        default:
            break;
    }
}
