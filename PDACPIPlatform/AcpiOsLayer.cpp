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
* PDACPIPlatform Open Source Version of Apple's AppleACPIPlatform
* Created by github.com/csekel (InSaneDarwin)
*
* This specific file was created by Zormeister
*/

/* These are our functions so we can hook into IOKit properly. */

#include "acpica/acpi.h"
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOCommand.h>
#include <IOKit/IOCommandGate.h>
#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <pexpert/i386/efi.h>
#include <pexpert/i386/boot.h>

/* for some reason Xcode has disabled any and all forms of auto completion. */

extern "C" void *AcpiOsExtMapMemory(ACPI_PHYSICAL_ADDRESS, ACPI_SIZE);
extern "C" void AcpiOsExtUnmapMemory(void *);
extern "C" ACPI_STATUS AcpiOsExtInitialize(void);
extern "C" ACPI_PHYSICAL_ADDRESS AcpiOsExtGetRootPointer(void);
extern "C" ACPI_STATUS AcpiOsExtExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context);

IOWorkLoop *gAcpiOsThreadWorkLoop;
IOCommandGate *gAcpiOsThreadCommandGate;

/* PCI config space stuff. */
ACPI_MCFG_ALLOCATION gPCIFromPE;
ACPI_MCFG_ALLOCATION *gPCIDataFromMCFG;
size_t gPCIMCFGEntryCount;

IOLock *gAcpiOsExtMemoryMapLock;
OSSet *gAcpiOsExtMemoryMapSet;
OSCollectionIterator *gAcpiOsExtMemoryMapIterator;

struct _iocmdq_callback_data
{
    ACPI_OSD_EXEC_CALLBACK Callback;
    void *Context;
};

IOReturn AcpiOsThreadDispatch(OSObject *, void *field0, void *, void *, void *)
{
    _iocmdq_callback_data *d = (_iocmdq_callback_data *)field0;
    ml_set_interrupts_enabled(false); /* disable interrupts */
    d->Callback(d->Context);
    ml_set_interrupts_enabled(true); /* enable interrupts */

    /* we no longer have need for our callback data, and we have exited the cautious period without interrupts. */
    IOFree(d, sizeof(_iocmdq_callback_data));
    return kIOReturnSuccess;
}

ACPI_STATUS AcpiOsExtInitialize(void)
{
    /* Initialize local resources. */
    gAcpiOsExtMemoryMapLock = IOLockAlloc();
    gAcpiOsExtMemoryMapSet = OSSet::withCapacity(4); /* OSSet's can expand if need be, right? */
    gAcpiOsExtMemoryMapIterator = OSCollectionIterator::withCollection(gAcpiOsExtMemoryMapSet);

    /* init the execution system */
    gAcpiOsThreadWorkLoop = IOWorkLoop::workLoop();
    gAcpiOsThreadCommandGate = IOCommandGate::commandGate(NULL);
    gAcpiOsThreadCommandGate->setWorkLoop(gAcpiOsThreadWorkLoop);
    
    /* Fetch MCFG data from PE boot args, at least until PlatformExpert updates the data. */
    boot_args *args = (boot_args *)PE_state.bootArgs;
    gPCIFromPE.Address = args->pciConfigSpaceBaseAddress;
    gPCIFromPE.PciSegment = 0;
    gPCIFromPE.StartBusNumber = args->pciConfigSpaceStartBusNumber;
    gPCIFromPE.EndBusNumber = args->pciConfigSpaceEndBusNumber;
    return AE_OK;
}

void *AcpiOsExtMapMemory(ACPI_PHYSICAL_ADDRESS addr, ACPI_SIZE size)
{
    IOMemoryDescriptor *desc = IOMemoryDescriptor::withAddressRange(addr, size, kIOMemoryDirectionInOut | kIOMemoryMapperNone, kernel_task);
    if (desc) {
        IOMemoryMap *map = desc->map();
        if (map) {
            IOVirtualAddress va = map->getVirtualAddress();
            IOLockLock(gAcpiOsExtMemoryMapLock);
            gAcpiOsExtMemoryMapSet->setObject(map);
            IOLockUnlock(gAcpiOsExtMemoryMapLock);
            map->release();
            return (void *)va;
        }
        desc->release();
    }
    return NULL;
}

void AcpiOsExtUnmapMemory(void *p, ACPI_SIZE size)
{
    IOVirtualAddress va = (IOVirtualAddress)p;

    IOLockLock(gAcpiOsExtMemoryMapLock);
    gAcpiOsExtMemoryMapIterator->reset();
    while (IOMemoryMap *map = OSDynamicCast(IOMemoryMap, gAcpiOsExtMemoryMapIterator->getNextObject())) {
        if (map->getVirtualAddress() == va && map->getLength() == size) {
            gAcpiOsExtMemoryMapSet->removeObject(map);
            break;
        }
    }
    IOLockUnlock(gAcpiOsExtMemoryMapLock);
}


/* Locate the EFI configuration table */
ACPI_PHYSICAL_ADDRESS AcpiOsExtGetRootPointer(void)
{
    EFI_PHYSICAL_ADDRESS tableAddr = 0;
#if __LP64__
    EFI_CONFIGURATION_TABLE_64 tbl;
#else
    EFI_CONFIGURATION_TABLE_32 tbl;
#endif
    
    /* These are populated at boot time by the UEFI bootloader (boot.efi to be specific) */
    static const char *table_paths[] = {
        "/efi/configuration-table/8868E871-E4F1-11D3-BC22-0080C73C8881", /* ACPI_20 */
        "/efi/configuration-table/EB9D2D30-2D88-11D3-9A16-0090273FC14D"  /* ACPI */
    };
    
    for (int i = 0; i < 2; i++) {
        IORegistryEntry *reg = IORegistryEntry::fromPath(table_paths[i], gIODTPlane);

        if (reg) {
            /* The kernel interprets this field as a data entry, it's the physical address to the EFI config table */
            /* I referenced AppleSMBIOS for this. */
            OSData *d = OSDynamicCast(OSData, reg->getProperty("table"));
            if (d && (d->getLength() <= sizeof(tableAddr))) {
                bcopy(d->getBytesNoCopy(), &tableAddr, d->getLength());
                IOMemoryDescriptor *desc = IOMemoryDescriptor::withAddressRange(0, sizeof(tbl), kIODirectionOutIn | kIOMemoryMapperNone, kernel_task);
                bzero(&tbl, sizeof(tbl));
                desc->readBytes(0, &tbl, sizeof(tbl));
                reg->release();
                desc->release();
                return (ACPI_PHYSICAL_ADDRESS)tbl.VendorTable;
            }
        }
        reg->release();
    }
    AcpiOsPrintf("ACPI: No RSDP found.\n");
    return 0;
}

ACPI_STATUS AcpiOsExtExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    _iocmdq_callback_data *d = (_iocmdq_callback_data *)IOMalloc(sizeof(_iocmdq_callback_data));
    d->Callback = Function;
    d->Context = Context;
    gAcpiOsThreadCommandGate->runAction(&AcpiOsThreadDispatch, d);
    return AE_OK;
}

void AcpiOsExtWaitEventsComplete(void)
{
    /* How do I check that my IOCommandGate has finished all of it's runAction calls? */
    return;
}

ACPI_STATUS AcpiOsReadPCIConfigSpace(ACPI_PCI_ID *PciId, UInt32 Reg, UInt64 *Value, UInt32 Width) {
    return AE_OK;
}

/*
 #define AE_ERROR                        EXCEP_ENV (0x0001)
 #define AE_NO_ACPI_TABLES               EXCEP_ENV (0x0002)
 #define AE_NO_NAMESPACE                 EXCEP_ENV (0x0003)
 #define AE_NO_MEMORY                    EXCEP_ENV (0x0004)
 #define AE_NOT_FOUND                    EXCEP_ENV (0x0005)
 #define AE_NOT_EXIST                    EXCEP_ENV (0x0006)
 #define AE_ALREADY_EXISTS               EXCEP_ENV (0x0007)
 #define AE_TYPE                         EXCEP_ENV (0x0008)
 #define AE_NULL_OBJECT                  EXCEP_ENV (0x0009)
 #define AE_NULL_ENTRY                   EXCEP_ENV (0x000A)
 #define AE_BUFFER_OVERFLOW              EXCEP_ENV (0x000B)
 #define AE_STACK_OVERFLOW               EXCEP_ENV (0x000C)
 #define AE_STACK_UNDERFLOW              EXCEP_ENV (0x000D)
 #define AE_NOT_IMPLEMENTED              EXCEP_ENV (0x000E)
 #define AE_SUPPORT                      EXCEP_ENV (0x000F)
 #define AE_LIMIT                        EXCEP_ENV (0x0010)
 #define AE_TIME                         EXCEP_ENV (0x0011)
 #define AE_ACQUIRE_DEADLOCK             EXCEP_ENV (0x0012)
 #define AE_RELEASE_DEADLOCK             EXCEP_ENV (0x0013)
 #define AE_NOT_ACQUIRED                 EXCEP_ENV (0x0014)
 #define AE_ALREADY_ACQUIRED             EXCEP_ENV (0x0015)
 #define AE_NO_HARDWARE_RESPONSE         EXCEP_ENV (0x0016)
 #define AE_NO_GLOBAL_LOCK               EXCEP_ENV (0x0017)
 #define AE_ABORT_METHOD                 EXCEP_ENV (0x0018)
 #define AE_SAME_HANDLER                 EXCEP_ENV (0x0019)
 #define AE_NO_HANDLER                   EXCEP_ENV (0x001A)
 #define AE_OWNER_ID_LIMIT               EXCEP_ENV (0x001B)
 #define AE_NOT_CONFIGURED               EXCEP_ENV (0x001C)
 #define AE_ACCESS                       EXCEP_ENV (0x001D)
 #define AE_IO_ERROR                     EXCEP_ENV (0x001E)
 #define AE_NUMERIC_OVERFLOW             EXCEP_ENV (0x001F)
 #define AE_HEX_OVERFLOW                 EXCEP_ENV (0x0020)
 #define AE_DECIMAL_OVERFLOW             EXCEP_ENV (0x0021)
 #define AE_OCTAL_OVERFLOW               EXCEP_ENV (0x0022)
 #define AE_END_OF_TABLE                 EXCEP_ENV (0x0023)
 */

IOReturn AcpiStatus2IOReturn(ACPI_STATUS stat) {
    return kIOReturnUnsupported;
}
