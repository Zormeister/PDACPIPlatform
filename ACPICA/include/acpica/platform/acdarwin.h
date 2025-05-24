//
//  acdarwin.h
//  PDACPIPlatform
//
//  Created by Zormeister on 24/5/2025.
//

#ifndef __ACDARWIN_H__
#define __ACDARWIN_H__

#if KERNEL

#include <mach/semaphore.h>
#include <IOKit/IOLib.h>

#define ACPI_USE_LOCAL_CACHE

#define ACPI_USE_GPE_POLLING

#define ACPI_SEMAPHORE semaphore_t
#define ACPI_LOCK IOLock *

#define ACPI_MSG_ERROR          KERN_ERR "ACPI Error: "
#define ACPI_MSG_EXCEPTION      KERN_ERR "ACPI Exception: "
#define ACPI_MSG_WARNING        KERN_WARNING "ACPI Warning: "
#define ACPI_MSG_INFO           KERN_INFO "ACPI: "

#define ACPI_MSG_BIOS_ERROR     KERN_ERR "ACPI BIOS Error (bug): "
#define ACPI_MSG_BIOS_WARNING   KERN_WARNING "ACPI BIOS Warning (bug): "

#include <stdint.h>

#if __LP64__
#define ACPI_MACHINE_WIDTH 64
#else
#define ACPI_MACHINE_WIDTH 32
#endif

#endif

#endif /* __ACDARWIN_H__ */
