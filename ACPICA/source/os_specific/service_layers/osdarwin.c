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
#include <IOKit/IOLib.h>
#include <mach/thread_status.h>

/* ACPI OS Layer implementations because yes */
#define _COMPONENT ACPI_OS_SERVICES
ACPI_MODULE_NAME("osdarwin");

/* TODO:
 * - AcpiOsCreateSemaphore - DONE
 * - AcpiOsDeleteSemaphore - DONE (implemented as AcpiOsDestroySemaphore)
 * - AcpiOsGetTimer - DONE
 * - AcpiOsSignalSemaphore - DONE
 * - AcpiOsWaitSemaphore - DONE
 *
 * PCI I/O accessing too - DONE Implemented
 */

/* track memory allocations, otherwise all hell will break loose in XNU. */
struct _memory_tag {
    UINT32 magic;
    ACPI_SIZE size;
};

#define ACPI_OS_PRINTF_USE_KPRINTF 0x1
#define ACPI_OS_PRINTF_USE_IOLOG   0x2

#if DEBUG
UInt32 gAcpiOsPrintfFlags = ACPI_OS_PRINTF_USE_KPRINTF | ACPI_OS_PRINTF_USE_IOLOG;
#else
/* ZORMEISTER: silence ACPICA's terror from the bad ACPI of HP, Lenovo and various other companies */
UInt32 gAcpiOsPrintfFlags = ACPI_OS_PRINTF_USE_IOLOG;
#endif

/* External functions - see PDACPIPlatform/AcpiOsLayer.cpp */
extern void *AcpiOsExtMapMemory(ACPI_PHYSICAL_ADDRESS, ACPI_SIZE);
extern void AcpiOsExtUnmapMemory(void *);
extern ACPI_STATUS AcpiOsExtInitialize(void);
extern ACPI_PHYSICAL_ADDRESS AcpiOsExtGetRootPointer(void);
extern ACPI_STATUS AcpiOsExtExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context);

ACPI_STATUS AcpiOsInitialize(void)
{
    PE_parse_boot_argn("acpi_os_log", &gAcpiOsPrintfFlags, sizeof(UInt32));
    return AcpiOsExtInitialize(); /* dispatch to AcpiOsLayer.cpp to establish the memory map tracking + PCI access. */
}

ACPI_STATUS AcpiOsTerminate(void)
{
    /* Cleanup any OS-specific resources if needed */
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void)
{
    return AcpiOsExtGetRootPointer();
}

#pragma mark Memory-related services

/* This is an XNU private API. I'd much rather a public function but that seems impossible. */
extern vm_offset_t ml_vtophys(vm_offset_t);

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
    IOVirtualAddress va = (IOVirtualAddress)LogicalAddress;
    *PhysicalAddress = ml_vtophys(va); /* i sure do hope this is compatible */
    return AE_OK;
}

void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
    return AcpiOsExtMapMemory(Where, Length);
}

void
AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Length)
{
    AcpiOsExtUnmapMemory(LogicalAddress);
}

void *
AcpiOsAllocate(ACPI_SIZE Size)
{
    void *alloc = IOMalloc(Size + sizeof(struct _memory_tag));
    struct _memory_tag *mem = alloc;
    mem->magic = 'mema';
    mem->size = Size + sizeof(struct _memory_tag);
    return (alloc + sizeof(struct _memory_tag));
}

void *
AcpiOsAllocateZeroed(ACPI_SIZE Size)
{
    void *alloc = AcpiOsAllocate(Size);
    memset(alloc, 0, Size);
    return alloc;
}

void
AcpiOsFree(void *p)
{
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
    UINT32                  Width)
{
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
    }
}

extern uint8_t ml_port_io_read8(uint16_t ioport);
extern uint16_t ml_port_io_read16(uint16_t ioport);
extern uint32_t ml_port_io_read32(uint16_t ioport);

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS Address,
               UINT32 *Value,
               UINT32 Width)
{
    switch (Width) {
        case 8:
            *Value = ml_port_io_read8(Address);
            return AE_OK;
        case 16:
            *Value = ml_port_io_read16(Address);
            return AE_OK;
        case 32:
            *Value = ml_port_io_read32(Address);
            return AE_OK;
        default:
            return AE_BAD_PARAMETER;
    }
}


extern void ml_phys_write_byte(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_byte_64(addr64_t paddr, unsigned int data);
extern void ml_phys_write_half(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_half_64(addr64_t paddr, unsigned int data);
extern void ml_phys_write_word(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_word_64(addr64_t paddr, unsigned int data);
extern void ml_phys_write_double(vm_offset_t paddr, unsigned long long data);
extern void ml_phys_write_double_64(addr64_t paddr, unsigned long long data);

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address,
                  UINT64 Value,
                  UINT32 Width)
{
    switch (Width) {
        case 8:
#if __LP64__
            ml_phys_write_byte_64(Address, (UINT32)Value);
#else
            ml_phys_write_byte(Address, (UINT32)Value);
#endif
            return AE_OK;
        case 16:
#if __LP64__
            ml_phys_write_half_64(Address, (UINT32)Value);
#else
            ml_phys_write_half(Address, (UINT32)Value);
#endif
            return AE_OK;
        case 32:
#if __LP64__
            ml_phys_write_word_64(Address, (UINT32)Value);
#else
            ml_phys_write_word(Address, (UINT32)Value);
#endif
            return AE_OK;
        case 64:
#if __LP64__
            ml_phys_write_double_64(Address, Value);
#else
            ml_phys_write_double(Address, Value);
#endif
        default:
            AcpiOsPrintf("ACPI: bad width value\n");
            return AE_ERROR;
    }
}

extern void ml_port_io_write8(uint16_t ioport, uint8_t val);
extern void ml_port_io_write16(uint16_t ioport, uint16_t val);
extern void ml_port_io_write32(uint16_t ioport, uint32_t val);

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS Address,
                UINT32 Value,
                UINT32 Width)
{
    switch (Width) {
        case 8:
            ml_port_io_write8(Address, (UINT8)Value);
            return AE_OK;
        case 16:
            ml_port_io_write16(Address, (UINT16)Value);
            return AE_OK;
        case 32:
            ml_port_io_write32(Address, Value);
            return AE_OK;
        default:
            return AE_BAD_PARAMETER;
    }
}

/* PCI I/O Access Implementation - COMPLETED */
ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId,
                           UINT32 Register,
                           UINT64 *Value,
                           UINT32 Width)
{
    UINT32 pci_address;
    UINT32 data = 0;
    
    if (!PciId || !Value) {
        return AE_BAD_PARAMETER;
    }
    
    if (Width != 8 && Width != 16 && Width != 32) {
        return AE_BAD_PARAMETER;
    }
    
    /* Construct PCI configuration address */
    pci_address = (1U << 31) |                  /* Enable bit */
                  (PciId->Segment << 16) |      /* Segment (if supported) */
                  (PciId->Bus << 16) |          /* Bus number */
                  (PciId->Device << 11) |       /* Device number */
                  (PciId->Function << 8) |      /* Function number */
                  (Register & 0xFC);            /* Register (aligned to 32-bit) */
    
    /* Write address to CONFIG_ADDRESS port (0xCF8) */
    ml_port_io_write32(0xCF8, pci_address);
    
    /* Read data from CONFIG_DATA port (0xCFC) with appropriate offset */
    switch (Width) {
        case 8:
            data = ml_port_io_read8(0xCFC + (Register & 3));
            break;
        case 16:
            data = ml_port_io_read16(0xCFC + (Register & 2));
            break;
        case 32:
            data = ml_port_io_read32(0xCFC);
            break;
    }
    
    *Value = data;
    
#if DEBUG
    AcpiOsPrintf("PCI read: %02X:%02X:%02X reg 0x%02X width %d = 0x%X\n",
                 PciId->Bus, PciId->Device, PciId->Function, 
                 Register, Width, (UINT32)*Value);
#endif
    
    return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId,
                            UINT32 Register,
                            UINT64 Value,
                            UINT32 Width)
{
    UINT32 pci_address;
    
    if (!PciId) {
        return AE_BAD_PARAMETER;
    }
    
    if (Width != 8 && Width != 16 && Width != 32) {
        return AE_BAD_PARAMETER;
    }
    
#if DEBUG
    AcpiOsPrintf("PCI write: %02X:%02X:%02X reg 0x%02X width %d = 0x%X\n",
                 PciId->Bus, PciId->Device, PciId->Function, 
                 Register, Width, (UINT32)Value);
#endif
    
    /* Construct PCI configuration address */
    pci_address = (1U << 31) |                  /* Enable bit */
                  (PciId->Segment << 16) |      /* Segment (if supported) */
                  (PciId->Bus << 16) |          /* Bus number */
                  (PciId->Device << 11) |       /* Device number */
                  (PciId->Function << 8) |      /* Function number */
                  (Register & 0xFC);            /* Register (aligned to 32-bit) */
    
    /* Write address to CONFIG_ADDRESS port (0xCF8) */
    ml_port_io_write32(0xCF8, pci_address);
    
    /* Write data to CONFIG_DATA port (0xCFC) with appropriate offset */
    switch (Width) {
        case 8:
            ml_port_io_write8(0xCFC + (Register & 3), (UINT8)Value);
            break;
        case 16:
            ml_port_io_write16(0xCFC + (Register & 2), (UINT16)Value);
            break;
        case 32:
            ml_port_io_write32(0xCFC, (UINT32)Value);
            break;
    }
    
    return AE_OK;
}

#pragma mark OS time related functions

void AcpiOsSleep(UINT64 ms)
{
    IOSleep((UINT32)ms);
}

void AcpiOsStall(UINT32 us)
{
    IODelay(us);
}

/* ZORMEISTER: I think? - 'The current value of the system timer in 100-nanosecond units. '*/
UInt64 AcpiOsGetTimer(void)
{
    UInt64 abs = mach_absolute_time();
    UInt64 ns = 0;
    absolutetime_to_nanoseconds(abs, &ns);
    return (ns / 100);
}

#pragma mark Lock functions

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *Lock)
{
    IOSimpleLock *lck = IOSimpleLockAlloc();
    if (!lck) {
        return AE_NO_MEMORY;
    }
    
    *Lock = lck;
    
    return AE_OK;
};

void AcpiOsDeleteLock(ACPI_SPINLOCK Lock)
{
    IOSimpleLockFree(Lock);
}


/* 'May be called from interrupt handlers, GPE handlers, and Fixed event handlers.' */
/* Fun way of saying I should disable interrupts until the lock is released. */
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Lock)
{
    ml_set_interrupts_enabled(false);
    IOSimpleLockLock(Lock);
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Lock, ACPI_CPU_FLAGS Flags)
{
    IOSimpleLockUnlock(Lock);
    ml_set_interrupts_enabled(true);
}

#pragma mark Semaphore code

ACPI_STATUS AcpiOsCreateSemaphore(UInt32 InitialUnits, UInt32 MaxUnits, ACPI_SEMAPHORE *Handle)
{
    if (Handle == NULL) {
        return AE_BAD_PARAMETER;
    }
    
    if (semaphore_create(current_task(), Handle, 0, MaxUnits) == KERN_SUCCESS) {
        return AE_OK;
    }

    return AE_NO_MEMORY;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    return AcpiOsDestroySemaphore(Handle);
}

ACPI_STATUS AcpiOsDestroySemaphore(ACPI_SEMAPHORE Semaphore)
{
    if (Semaphore == NULL) {
        return AE_BAD_PARAMETER;
    }

    semaphore_destroy(current_task(), Semaphore);

    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Semaphore, UInt32 Units, UInt16 Timeout)
{
    if (Semaphore == NULL) {
        return AE_BAD_PARAMETER;
    }
    
    if (Timeout == 0xFFFF) {
        if (semaphore_wait(Semaphore) != KERN_SUCCESS) {
            return AE_TIME;
        }
    } else {
        if (semaphore_wait_deadline(Semaphore, (Timeout * NSEC_PER_MSEC)) != KERN_SUCCESS) {
            return AE_TIME;
        }
    }
    
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Semaphore, UInt32 Units)
{
    if (Semaphore == NULL) {
        return AE_BAD_PARAMETER;
    }
    
    if (Units > 1) {
        semaphore_signal_all(Semaphore);
    } else {
        semaphore_signal(Semaphore);
    }
    
    return AE_OK;
}


#pragma mark thread related stuff

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
    return thread_tid(current_thread()); /* I think? */
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    return AcpiOsExtExecute(Type, Function, Context);
}

void AcpiOsWaitEventsComplete(void)
{
    /* Wait for all queued asynchronous events to complete */
    /* Implementation depends on how AcpiOsExecute queues work */
    /* For now, this is a no-op since AcpiOsExecute delegates to external implementation */
}

#pragma mark Override functions - they do nothing.

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
{
    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable,
                                        ACPI_PHYSICAL_ADDRESS *NewAddress,
                                        UINT32 *NewTableLength)
{
    if (!ExistingTable || !NewAddress || !NewTableLength) {
        return AE_BAD_PARAMETER;
    }
    
    *NewAddress = 0;
    *NewTableLength = 0;
    return AE_OK;
}

#pragma mark Misc. OSL services

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info)
{
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

void AcpiOsPrintf(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    AcpiOsVprintf(fmt, va);
    va_end(va);
}

void AcpiOsVprintf(const char *fmt, va_list list)
{
    char msg[4096]; /* I don't think a message will exceed this size in one go. */
    vsnprintf(msg, 4096, fmt, list);

    if (gAcpiOsPrintfFlags & ACPI_OS_PRINTF_USE_KPRINTF) {
        kprintf("%s", msg);
    }
    
    if (gAcpiOsPrintfFlags & ACPI_OS_PRINTF_USE_IOLOG) {
        /* Allegedly IOLog can't be used within an interrupt context. I believe. */
        if (!ml_at_interrupt_context()) {
            IOLog("%s", msg);
        }
    }
}
