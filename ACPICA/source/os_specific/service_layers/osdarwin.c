/*
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
 */

/* standard includes... */
#include "acpi.h"
#include "actbl.h"  /* For MCFG table definitions */

#include <mach/semaphore.h>
#include <machine/machine_routines.h>
#include <mach/machine.h>
#include <IOKit/IOLib.h>
#include <mach/thread_status.h>

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
 * Cache functions - DONE added Implementation
 */

/* track memory allocations, otherwise all hell will break loose in XNU. */
struct _memory_tag {
    UINT32 magic;
    ACPI_SIZE size;
};

/* Cache management structures for ACPICA object caching */
struct _cache_object {
    struct _cache_object *next;
    /* Object data follows this header */
};

struct _acpi_cache {
    UINT32 magic;
    char name[16];
    ACPI_SIZE object_size;
    UINT16 max_depth;
    UINT16 current_depth;
    struct _cache_object *list_head;
    IOSimpleLock *lock;
    UINT32 requests;
    UINT32 hits;
};

#define ACPI_CACHE_MAGIC 'cach'

#define ACPI_OS_PRINTF_USE_KPRINTF 0x1
#define ACPI_OS_PRINTF_USE_IOLOG   0x2

/* Local static variables go here */
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
extern ACPI_STATUS AcpiOsReadPCIConfigSpace(ACPI_PCI_ID *PciId, UInt32 Reg, UInt64 *Value, UInt32 Width);
extern ACPI_STATUS AcpiOsWritePCIConfigSpace(ACPI_PCI_ID *PciId, UInt32 Reg, UInt64 Value, UInt32 Width);

/* XNU Private KPI declarations */


ACPI_STATUS AcpiOsInitialize(void)
{
    ACPI_STATUS status;
    
    PE_parse_boot_argn("acpi_os_log", &gAcpiOsPrintfFlags, sizeof(UInt32));
    
    status = AcpiOsExtInitialize(); /* dispatch to AcpiOsLayer.cpp to establish the memory map tracking + PCI access. */
    if (ACPI_FAILURE(status)) {
        return status;
    }
    
    /* Initialize ECAM support - this will be done lazily on first PCI access if ACPI tables aren't ready yet */
    /* AcpiOsInitializePciEcam(); */
    
    return AE_OK;
}

/* AcpiOsValidateCache (Debug helper) - Validate cache integrity (debug builds only) */
#if DEBUG
ACPI_STATUS
AcpiOsValidateCache(ACPI_CACHE_T *Cache)
{
    struct _acpi_cache *cache = (struct _acpi_cache *)Cache;
    struct _cache_object *object;
    UINT16 count = 0;
    
    if (!cache || cache->magic != ACPI_CACHE_MAGIC) {
        AcpiOsPrintf("ACPI: Invalid cache object\n");
        return AE_BAD_PARAMETER;
    }
    
    IOSimpleLockLock(cache->lock);
    
    /* Count objects in cache */
    object = cache->list_head;
    while (object && count < cache->max_depth + 10) { /* Prevent infinite loops */
        count++;
        object = object->next;
    }
    
    if (count != cache->current_depth) {
        AcpiOsPrintf("ACPI: Cache '%s' depth mismatch: reported %d, actual %d\n",
                     cache->name, cache->current_depth, count);
        IOSimpleLockUnlock(cache->lock);
        return AE_ERROR;
    }
    
    IOSimpleLockUnlock(cache->lock);
    
    AcpiOsPrintf("ACPI: Cache '%s' validated: %d/%d objects, %u requests, %u hits\n",
                 cache->name, cache->current_depth, cache->max_depth,
                 cache->requests, cache->hits);
    
    return AE_OK;
}
#endif

/* AcpiOsGetCacheStatistics (Debug helper) -  Get cache statistics */
ACPI_STATUS
AcpiOsGetCacheStatistics(ACPI_CACHE_T *Cache, UINT32 *Requests, UINT32 *Hits)
{
    struct _acpi_cache *cache = (struct _acpi_cache *)Cache;
    
    if (!cache || cache->magic != ACPI_CACHE_MAGIC) {
        return AE_BAD_PARAMETER;
    }
    
    if (Requests) {
        *Requests = cache->requests;
    }
    
    if (Hits) {
        *Hits = cache->hits;
    }
    
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(void)
{
    /* Cleanup any OS-specific resources if needed */
    
    /* Note: ACPICA should clean up its own caches via AcpiOsDeleteCache() */
    /* but we could add cache leak detection here in debug builds */
    
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

/* me is kernel. i can write and read as i want. */
BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length) { return true; }
BOOLEAN AcpiOsWriteable(void *Memory, ACPI_SIZE Length) { return true; }

ACPI_STATUS
AcpiOsReadMemory(
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  *Value,
    UINT32                  Width)
{
    switch (Width) {
        case 8:
            *Value = PHYS_READ_8(Address);
            return AE_OK;
        case 16:
            *Value = PHYS_READ_16(Address);
            return AE_OK;
        case 32:
            *Value = PHYS_READ_32(Address);
            return AE_OK;
        case 64:
            *Value = PHYS_READ_64(Address);
            return AE_OK;
        default:
            AcpiOsPrintf("ACPI: bad width value\n");
            return AE_ERROR;
    }
}

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

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address,
                  UINT64 Value,
                  UINT32 Width)
{
    switch (Width) {
        case 8:
            PHYS_WRITE_8(Address, Value);
            return AE_OK;
        case 16:
            PHYS_WRITE_16(Address, Value);
            return AE_OK;
        case 32:
            PHYS_WRITE_32(Address, Value);
            return AE_OK;
        case 64:
            PHYS_WRITE_64(Address, Value);
            return AE_OK;
        default:
            AcpiOsPrintf("ACPI: bad width value\n");
            return AE_ERROR;
    }
}

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

/* Main PCI Configuration Space Access Functions */
ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId,
                           UINT32 Register,
                           UINT64 *Value,
                           UINT32 Width)
{
    if (!PciId || !Value) {
        return AE_BAD_PARAMETER;
    }
    
    return AcpiOsReadPCIConfigSpace(PciId, Register, Value, Width);
}

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId,
                            UINT32 Register,
                            UINT64 Value,
                            UINT32 Width)
{
    if (!PciId) {
        return AE_BAD_PARAMETER;
    }
    
    return AcpiOsWritePciConfiguration(PciId, Register, Value, Width);
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
UINT64 AcpiOsGetTimer(void)
{
    UInt64 abs = 0;
    UInt64 ns = 0;

    /*
     * use the clock uptime over mach absolute time
     */

    clock_get_uptime(&abs);
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

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Semaphore)
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


#pragma mark Cache management functions

/* adjustments by csekel (InSaneDarwin)
 * ACPICA Object Cache Implementation for Darwin
 * 
 * ACPICA uses object caching to improve performance by reusing frequently
 * allocated/freed objects like parse tree nodes, namespace entries, etc.
 *
 * Cache objects are stored as a simple linked list (LIFO) for fast
 * acquire/release operations. Each cache maintains its own lock and
 * statistics to minimize contention between different object types.
 */

/* AcpiOsCreateCache - Create a cache object for ACPICA */
ACPI_STATUS
AcpiOsCreateCache(char *CacheName,
                  UINT16 ObjectSize,
                  UINT16 MaxDepth,
                  ACPI_CACHE_T **ReturnCache)
{
    struct _acpi_cache *cache;
    
    if (!CacheName || !ReturnCache || ObjectSize == 0) {
        return AE_BAD_PARAMETER;
    }
    
    /* Allocate cache structure */
    cache = (struct _acpi_cache *)IOMalloc(sizeof(struct _acpi_cache));
    if (!cache) {
        return AE_NO_MEMORY;
    }
    
    /* Initialize cache structure */
    memset(cache, 0, sizeof(struct _acpi_cache));
    cache->magic = ACPI_CACHE_MAGIC;
    strncpy(cache->name, CacheName, sizeof(cache->name) - 1);
    cache->name[sizeof(cache->name) - 1] = '\0';
    cache->object_size = ObjectSize;
    cache->max_depth = MaxDepth;
    cache->current_depth = 0;
    cache->list_head = NULL;
    cache->requests = 0;
    cache->hits = 0;
    
    /* Create lock for thread safety */
    cache->lock = IOSimpleLockAlloc();
    if (!cache->lock) {
        IOFree(cache, sizeof(struct _acpi_cache));
        return AE_NO_MEMORY;
    }
    
    *ReturnCache = (ACPI_CACHE_T *)cache;
    
#if DEBUG
    AcpiOsPrintf("ACPI: Created cache '%s', object size %d, max depth %d\n",
                 CacheName, ObjectSize, MaxDepth);
#endif
    
    return AE_OK;
}

/* AcpiOsDeleteCache - Free all objects within a cache and delete the cache object */
ACPI_STATUS
AcpiOsDeleteCache(ACPI_CACHE_T *Cache)
{
    struct _acpi_cache *cache = (struct _acpi_cache *)Cache;
    struct _cache_object *object, *next;
    
    if (!cache || cache->magic != ACPI_CACHE_MAGIC) {
        return AE_BAD_PARAMETER;
    }
    
#if DEBUG
    AcpiOsPrintf("ACPI: Deleting cache '%s', requests %u, hits %u (%.1f%%)\n",
                 cache->name, cache->requests, cache->hits,
                 cache->requests ? (cache->hits * 100.0 / cache->requests) : 0.0);
#endif
    
    /* Purge all objects from cache */
    IOSimpleLockLock(cache->lock);
    
    object = cache->list_head;
    while (object) {
        next = object->next;
        IOFree(object, sizeof(struct _cache_object) + cache->object_size);
        object = next;
    }
    
    IOSimpleLockUnlock(cache->lock);
    
    /* Free the lock and cache structure */
    IOSimpleLockFree(cache->lock);
    cache->magic = 0; /* Invalidate */
    IOFree(cache, sizeof(struct _acpi_cache));
    
    return AE_OK;
}

/* AcpiOsPurgeCache - Free all objects within a cache */
ACPI_STATUS
AcpiOsPurgeCache(ACPI_CACHE_T *Cache)
{
    struct _acpi_cache *cache = (struct _acpi_cache *)Cache;
    struct _cache_object *object, *next;
    UINT16 purged = 0;
    
    if (!cache || cache->magic != ACPI_CACHE_MAGIC) {
        return AE_BAD_PARAMETER;
    }
    
    IOSimpleLockLock(cache->lock);
    
    object = cache->list_head;
    while (object) {
        next = object->next;
        IOFree(object, sizeof(struct _cache_object) + cache->object_size);
        object = next;
        purged++;
    }
    
    cache->list_head = NULL;
    cache->current_depth = 0;
    
    IOSimpleLockUnlock(cache->lock);
    
#if DEBUG
    AcpiOsPrintf("ACPI: Purged %d objects from cache '%s'\n", purged, cache->name);
#endif
    
    return AE_OK;
}

/* AcpiOsAcquireObject - Get an object from the cache or allocate a new one */
void *
AcpiOsAcquireObject(ACPI_CACHE_T *Cache)
{
    struct _acpi_cache *cache = (struct _acpi_cache *)Cache;
    struct _cache_object *object;
    void *return_object = NULL;
    
    if (!cache || cache->magic != ACPI_CACHE_MAGIC) {
        return NULL;
    }
    
    IOSimpleLockLock(cache->lock);
    
    cache->requests++;
    
    /* Try to get an object from the cache first */
    if (cache->list_head) {
        object = cache->list_head;
        cache->list_head = object->next;
        cache->current_depth--;
        cache->hits++;
        
        /* Return pointer to data area (after the header) */
        return_object = (void *)((char *)object + sizeof(struct _cache_object));
        
        IOSimpleLockUnlock(cache->lock);
        
        /* Clear the object data */
        memset(return_object, 0, cache->object_size);
        
        return return_object;
    }
    
    IOSimpleLockUnlock(cache->lock);
    
    /* Cache is empty, allocate a new object */
    object = (struct _cache_object *)IOMalloc(sizeof(struct _cache_object) + cache->object_size);
    if (!object) {
        return NULL;
    }
    
    /* Clear the entire object */
    memset(object, 0, sizeof(struct _cache_object) + cache->object_size);
    
    /* Return pointer to data area */
    return_object = (void *)((char *)object + sizeof(struct _cache_object));
    
    return return_object;
}

/* AcpiOsReleaseObject - Release an object back to the cache */
ACPI_STATUS
AcpiOsReleaseObject(ACPI_CACHE_T *Cache, void *Object)
{
    struct _acpi_cache *cache = (struct _acpi_cache *)Cache;
    struct _cache_object *cache_object;
    
    if (!cache || cache->magic != ACPI_CACHE_MAGIC || !Object) {
        /* If cache is invalid, just free the object */
        if (Object) {
            cache_object = (struct _cache_object *)((char *)Object - sizeof(struct _cache_object));
            IOFree(cache_object, sizeof(struct _cache_object) + (cache ? cache->object_size : 0));
        }
        return AE_BAD_PARAMETER;
    }
    
    /* Get pointer to cache object header */
    cache_object = (struct _cache_object *)((char *)Object - sizeof(struct _cache_object));
    
    IOSimpleLockLock(cache->lock);
    
    /* If cache is full, just free the object */
    if (cache->current_depth >= cache->max_depth) {
        IOSimpleLockUnlock(cache->lock);
        IOFree(cache_object, sizeof(struct _cache_object) + cache->object_size);
        return AE_OK;
    }
    
    /* Add object back to cache */
    cache_object->next = cache->list_head;
    cache->list_head = cache_object;
    cache->current_depth++;
    
    IOSimpleLockUnlock(cache->lock);
    
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

void AcpiOsRedirectOutput(void *Destination)
{
    
}
