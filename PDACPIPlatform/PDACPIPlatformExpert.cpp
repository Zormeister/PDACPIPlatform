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

#if __has_include(<IOKit/pci/IOPCIPrivate.h>)
#include <IOKit/pci/IOPCIPrivate.h>
#else
extern IOReturn IOPCIPlatformInitialize(void);
#endif

extern "C" {
#include "acpica/acpi.h" // For ACPICA APIs
#include "acpica/acstruct.h"
#include "acpica/aclocal.h"
#include "acpica/acglobal.h"
}

#define super IOACPIPlatformExpert
OSDefineMetaClassAndStructors(PDACPIPlatformExpert, IOACPIPlatformExpert)

ACPI_TABLE_MADT *gAPICTable;

/* AcpiOsLayer.cpp */
extern ACPI_MCFG_ALLOCATION *gPCIDataFromMCFG;
extern size_t gPCIMCFGEntryCount;

bool PDACPIPlatformExpert::initializeACPICA()
{
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

/* this is so IOPCIFamily gets our ACPI tables. */
OSObject *PDACPIPlatformExpert::copyProperty(const char *property) const
{
    if (strncmp(property, "ACPI Tables", strlen(property)) == 0) {
        return this->m_tableDict->copyCollection();
    }
    
    return super::copyProperty(property);
}

bool PDACPIPlatformExpert::fetchPCIData()
{
    const OSData *table = this->getACPITableData("MCFG", 0);
    
    if (!table) {
        IOLog("ACPI: No MCFG table found in the ACPI table collection.\n");
    }
    
    ACPI_TABLE_MCFG *mcfg = (ACPI_TABLE_MCFG *)table->getBytesNoCopy();

    gPCIMCFGEntryCount = (mcfg->Header.Length - sizeof(ACPI_TABLE_MCFG)) / sizeof(ACPI_MCFG_ALLOCATION);
    gPCIDataFromMCFG = (ACPI_MCFG_ALLOCATION *)(table->getBytesNoCopy() + sizeof(ACPI_TABLE_MCFG));
    
    /* While we're here; kindly tell IOPCIFamily to initialize MMIO mapping services. */
    IOPCIPlatformInitialize();
    
    return true;
}

/* This was based off of osbsdtbl.c's shenanigans */
struct AcpiTableMap {
    char Signature[4];
    UInt8 instance;
    ACPI_TABLE_HEADER *Tbl; /* well we've already mapped the damn thing so, might as well reuse the pointer. */
};

bool PDACPIPlatformExpert::catalogACPITables()
{
    char name[32];
    ACPI_TABLE_HEADER *Table;
    UInt32 tables = AcpiGbl_RootTableList.CurrentTableCount;
    this->m_tableDict = OSDictionary::withCapacity(tables + 1);

    AcpiTableMap *tmp = IOMalloc(sizeof(AcpiTableMap) * tables);

    /* ZORMEISTER: God this is such a hack... */
    for (UInt32 i = 0; i < tables; i++) {
        AcpiGetTableByIndex(i, &Table);
        for (UInt32 j = i; 0 > j; t--) { /* ZORMEISTER: walk backwards from our current position. */
            if (strncmp(tmp[j].Signature, Table->Signature, 4) == 0) {
                if (tmp[j].instance == 0) {
                    tmp[j].instance++;
                }

                tmp[i].instance = tmp[j].instance + 1;
            }
        }
    }

    /* ZORMEISTER: now do it again. */

    for (UInt32 k = 0; k < tables; k++) {
        /* Allocate an OSData using the ACPI table length. */
        OSData *data = OSData::withBytesNoCopy(tmp[k].Tbl, tmp[k].Tbl->Length);
        memset(name, 0, 32); /* clear out the stack variable */
        if (tmp[k].instance > 0) {
            snprintf(name, 32, "%4.4s-%u", tmp[k].Tbl->Signature, tmp[k].instance);
        } else {
            snprintf(name, 32, "%4.4s", tmp[k].Tbl->Signature);
        }

        this->m_tableDict->setObject(name, data);
        OSSafeReleaseNULL(data);
    }
    
    IOFree(tmp, sizeof(AcpiTableMap) * tables);

    return true;
}

const OSData *PDACPIPlatformExpert::getACPITableData(const char *name, UInt32 TableIndex)
{
    char tbl[32];

    if (TableIndex > 0) {
        snprintf(tbl, 32, "%4.4s-%u", name, TableIndex);
    } else {
        snprintf(tbl, 32, "%4.4s", name);
    }

    OSObject *obj = this->m_tableDict->getObject(name);
    if (obj) {
        return OSDynamicCast(OSData, obj);
    }
    
    return nullptr;
}


bool PDACPIPlatformExpert::start(IOService *provider)
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

void PDACPIPlatformExpert::stop(IOService *provider)
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
                                                         IOOptionBits)
{
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
                                                IOOptionBits options)
{
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
