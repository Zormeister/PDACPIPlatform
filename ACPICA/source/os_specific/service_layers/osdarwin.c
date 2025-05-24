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

/* standard includes... */
#include "acpica/acpi.h"

#include <mach/semaphore.h>
#include <machine/machine_routines.h>
#include <mach/machine.h>
#include <architecture/i386/pio.h>
#include <IOKit/IOLib.h>

/* ACPI OS Layer implementations because yes */
#define _COMPONENT ACPI_OS_SERVICES
ACPI_MODULE_NAME("osdarwin");

/* track memory allocations, otherwise all hell will break loose in XNU. */
struct _memory_tag {
    UINT32 magic;
    ACPI_SIZE size;
};

/* External functions - see source/PDACPIPlatform/AcpiOsLayer.cpp */
extern void *AcpiOsExtMapMemory(ACPI_PHYSICAL_ADDRESS, ACPI_SIZE);
extern void AcpiOsExtUnmapMemory(void *);
extern ACPI_STATUS AcpiOsExtInitialize(void);
extern ACPI_PHYSICAL_ADDRESS AcpiOsExtGetRootPointer(void);

ACPI_STATUS AcpiOsInitialize(void) {
    return AcpiOsExtInitialize();
}

#pragma mark Memory-related services

void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
    return AcpiOsExtMapMemory(Where, Length);
}

void *
AcpiOsAllocate(ACPI_SIZE Size) {
    void *alloc = IOMalloc(Size + sizeof(struct _memory_tag));
    struct _memory_tag *mem = alloc;
    mem->magic = 'mema';
    mem->size = Size + sizeof(struct _memory_tag);
    return (alloc + sizeof(struct _memory_tag));
}

void *
AcpiOsAllocateZeroed(ACPI_SIZE Size) {
    void *alloc = AcpiOsAllocate(Size);
    memset(alloc, 0, Size);
    return alloc;
}

void
AcpiOsFree(void *p) {
    struct _memory_tag *m = p - sizeof(struct _memory_tag);
    if (m->magic == 'mema') {
        IOFree(m, m->size);
    } else {
        /* induce panic? */
        return;
    }
}

#pragma mark OS time related functions

void AcpiOsSleep(UINT64 ms) {
    IOSleep((UINT32)ms);
}

void AcpiOsStall(UINT32 us) {
    IODelay(us);
}

#pragma mark Lock functions

#pragma mark OS memory functions

/* import KPIs because otherwise this won't work. */
extern unsigned int ml_phys_read_byte(vm_offset_t paddr);
extern unsigned int ml_phys_read_byte_64(addr64_t paddr);
extern unsigned int ml_phys_read_half(vm_offset_t paddr);
extern unsigned int ml_phys_read_half_64(addr64_t paddr);
extern unsigned int ml_phys_read_word(vm_offset_t paddr);
extern unsigned int ml_phys_read_word_64(addr64_t paddr);
extern unsigned long long ml_phys_read_double(vm_offset_t paddr);
extern unsigned long long ml_phys_read_double_64(addr64_t paddr);

ACPI_STATUS
AcpiOsReadMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  *Value,
    UINT32                  Width) {
    
    switch (Width) {
        case 8:
            *Value = ml_phys_read_byte_64(Address);
            return AE_OK;
        case 16:
            *Value = ml_phys_read_half_64(Address);
            return AE_OK;
        case 32:
            *Value = ml_phys_read_word_64(Address);
            return AE_OK;
        case 64:
            *Value = ml_phys_read_double_64(Address);
            return AE_OK;
        default:
            AcpiOsPrintf("ACPI: bad width value\n");
            return AE_ERROR;
            break;
    }
}

#pragma mark thread related stuff

ACPI_THREAD_ID
AcpiOsGetThreadId(void) {
    return thread_tid(current_thread()); /* I think? */
}
