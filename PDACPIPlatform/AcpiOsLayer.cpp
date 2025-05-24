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
#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <pexpert/i386/efi.h>

/* for some reason Xcode has disabled any and all forms of auto completion. */

extern "C" void *AcpiOsExtMapMemory(ACPI_PHYSICAL_ADDRESS, ACPI_SIZE);
extern "C" void AcpiOsExtUnmapMemory(void *);
extern "C" ACPI_STATUS AcpiOsExtInitialize(void);
extern "C" ACPI_PHYSICAL_ADDRESS AcpiOsExtGetRootPointer(void);

IOLock *gAcpiOsExtMemoryMapLock;
OSSet *gAcpiOsExtMemoryMapSet;
OSCollectionIterator *gAcpiOsExtMemoryMapIterator;

/* Will have to access PE boot args to get the MCFG table data. */
ACPI_STATUS AcpiOsExtInitialize(void) {
    /* Initialize local resources. */
    gAcpiOsExtMemoryMapLock = IOLockAlloc();
    gAcpiOsExtMemoryMapSet = OSSet::withCapacity(4); /* OSSet's can expand if need be, right? */
    gAcpiOsExtMemoryMapIterator = OSCollectionIterator::withCollection(gAcpiOsExtMemoryMapSet);
    return AE_OK;
}

void *AcpiOsExtMapMemory(ACPI_PHYSICAL_ADDRESS addr, ACPI_SIZE size) {
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
    }
    return NULL;
}

void AcpiOsExtUnmapMemory(void *p, ACPI_SIZE size) {
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
ACPI_PHYSICAL_ADDRESS AcpiOsExtGetRootPointer(void) {
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
                return (ACPI_PHYSICAL_ADDRESS)tbl.VendorTable;
            }
        }
    }
    AcpiOsPrintf("ACPI: No RSDP found.\n");
    return 0;
}
