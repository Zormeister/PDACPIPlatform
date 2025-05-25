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
#include <mach/thread_status.h>

/* ACPI OS Layer implementations because yes */
#define _COMPONENT ACPI_OS_SERVICES
ACPI_MODULE_NAME("osdarwin");

/* track memory allocations, otherwise all hell will break loose in XNU. */
struct _memory_tag {
    UINT32 magic;
    ACPI_SIZE size;
};

/* External functions - see PDACPIPlatform/AcpiOsLayer.cpp */
extern void *AcpiOsExtMapMemory(ACPI_PHYSICAL_ADDRESS, ACPI_SIZE);
extern void AcpiOsExtUnmapMemory(void *);
extern ACPI_STATUS AcpiOsExtInitialize(void);
extern ACPI_PHYSICAL_ADDRESS AcpiOsExtGetRootPointer(void);

ACPI_STATUS AcpiOsInitialize(void) {
    return AcpiOsExtInitialize(); /* dispatch to AcpiOsLayer.cpp to establish the memory map tracking + PCI access. */
}

#pragma mark Memory-related services

/* This is an XNU private API. I'd much rather a public function but that seems impossible. */
extern vm_offset_t ml_vtophys(vm_offset_t);

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress) {
    IOVirtualAddress va = (IOVirtualAddress)LogicalAddress;
    *PhysicalAddress = ml_vtophys(va); /* i sure do hope this is compatible */
    return AE_OK;
}

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

/* ZORMEISTER: me is kernel. i can write and read as i want. */
BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length) { return true; }
BOOLEAN AcpiOsWriteable(void *Memory, ACPI_SIZE Length) { return true; }

/* ZORMEISTER: import KPIs because otherwise this won't work. */
extern unsigned int ml_phys_read_byte(vm_offset_t paddr);
extern unsigned int ml_phys_read_byte_64(addr64_t paddr);
extern unsigned int ml_phys_read_half(vm_offset_t paddr);
extern unsigned int ml_phys_read_half_64(addr64_t paddr);
extern unsigned int ml_phys_read_word(vm_offset_t paddr);
extern unsigned int ml_phys_read_word_64(addr64_t paddr);
extern unsigned long long ml_phys_read_double(vm_offset_t paddr);
extern unsigned long long ml_phys_read_double_64(addr64_t paddr);

/*
 * ZORMEISTER:
 * mind you this is my local machine using the following:
 *
 * SDK: MacOSX15.4.sdk
 * Xcode Version: 16.3 build 16E140
 * Apple Clang version: clang-1700.0.13.3
 *
 * i need to establish a build server for my projects
 *
 */

ACPI_STATUS
AcpiOsReadMemory(
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  *Value,
    UINT32                  Width) {
    
    switch (Width) {
        case 8:
#if __LP64__
            *Value = ml_phys_read_byte_64(Address);
#else
            *Value = ml_phys_read_byte(Address);
#endif
            return AE_OK;
        case 16:
#if __LP64__
            *Value = ml_phys_read_half_64(Address);
#else
            *Value = ml_phys_read_half(Address);
#endif
            return AE_OK;
        case 32:
#if __LP64__
            *Value = ml_phys_read_word_64(Address);
#else
            *Value = ml_phys_read_word(Address);
#endif
            return AE_OK;
        case 64:
#if __LP64__
            *Value = ml_phys_read_double_64(Address);
#else
            *Value = ml_phys_read_double(Address);
#endif
        default:
            AcpiOsPrintf("ACPI: bad width value\n");
            return AE_ERROR;
            break;
    }
}

extern void ml_phys_write_byte(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_byte_64(addr64_t paddr, unsigned int data);
extern void ml_phys_write_half(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_half_64(addr64_t paddr, unsigned int data);


#pragma mark OS time related functions

void AcpiOsSleep(UINT64 ms) {
    IOSleep((UINT32)ms);
}

void AcpiOsStall(UINT32 us) {
    IODelay(us);
}

#pragma mark Lock functions

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *Lock) {
    IOSimpleLock *lck = IOSimpleLockAlloc();
    if (!lck) {
        return AE_NO_MEMORY;
    }
    
    *Lock = lck;
    
    return AE_OK;
};

void AcpiOsDeleteLock(ACPI_SPINLOCK Lock) {
    IOSimpleLockFree(Lock);
}


/* 'May be called from interrupt handlers, GPE handlers, and Fixed event handlers.' */
/* Fun way of saying I should disable interrupts until the lock is released. */
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Lock) {
    ml_set_interrupts_enabled(false);
    IOSimpleLockLock(Lock);
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Lock, ACPI_CPU_FLAGS Flags) {
    IOSimpleLockUnlock(Lock);
    ml_set_interrupts_enabled(true);
}

#pragma mark thread related stuff

ACPI_THREAD_ID
AcpiOsGetThreadId(void) {
    return thread_tid(current_thread()); /* I think? */
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) {
    /* ok what? 'Schedule a procedure for deferred execution.' what the shit does that mean??? */
    return AE_OK;
}


#pragma mark Override functions - they do nothing.

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue) {
    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable) {
    *NewTable = NULL;
    return AE_OK;
}

#pragma mark Misc. OSL services

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info) {
    switch (Function) {
        case ACPI_SIGNAL_BREAKPOINT: {
            if (Info) {
                char *breakpt = (char *)Info;
                AcpiOsPrintf("ACPI: recieved breakpoint signal: %s", breakpt);
            } else {
                AcpiOsPrintf("ACPI: recieved breakpoint signal");
            }
            break;
        }

        case ACPI_SIGNAL_FATAL: {
            if (Info) {
                ACPI_SIGNAL_FATAL_INFO *ftl = (ACPI_SIGNAL_FATAL_INFO *)Info;
                AcpiOsPrintf("ACPI: recieved AML fatal signal, type: %d, code: %d, arg: %d\n", ftl->Type, ftl->Code, ftl->Argument);
            } else {
                AcpiOsPrintf("ACPI: recieved AML fatal signal");
            }
            break;
        }

        default: {
            AcpiOsPrintf("ACPI: unknown signal recieved (%d)\n", Function);
            break;
        }
    }
    return AE_OK;
}
