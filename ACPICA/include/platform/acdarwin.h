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

#ifndef __ACDARWIN_H__
#define __ACDARWIN_H__

#if KERNEL

#include <stdint.h>
#include <mach/semaphore.h>
#include <machine/machine_routines.h>
#include <IOKit/IOLib.h>

#define ACPI_USE_LOCAL_CACHE

#define ACPI_USE_GPE_POLLING

#define ACPI_SEMAPHORE semaphore_t
#define ACPI_SPINLOCK IOSimpleLock *

#define ACPI_USE_SYSTEM_CLIBRARY

#define ACPI_MSG_ERROR          "ACPI Error: "
#define ACPI_MSG_EXCEPTION      "ACPI Exception: "
#define ACPI_MSG_WARNING        "ACPI Warning: "
#define ACPI_MSG_INFO           "ACPI: "

#define ACPI_MSG_BIOS_ERROR     "ACPI BIOS Error (bug): "
#define ACPI_MSG_BIOS_WARNING   "ACPI BIOS Warning (bug): "

#define ACPI_DEBUG_OUTPUT
#define ACPI_DISASSEMBLER
#define ACPI_DEBUGGER

#if __LP64__
#define ACPI_MACHINE_WIDTH 64
#else
#define ACPI_MACHINE_WIDTH 32
#endif

#define stderr NULL
#define stdout NULL

#define EOF -1

extern const unsigned char AcpiGbl_Ctypes[];

#define _ACPI_XA     0x00    /* extra alphabetic - not supported */
#define _ACPI_XS     0x40    /* extra space */
#define _ACPI_BB     0x00    /* BEL, BS, etc. - not supported */
#define _ACPI_CN     0x20    /* CR, FF, HT, NL, VT */
#define _ACPI_DI     0x04    /* '0'-'9' */
#define _ACPI_LO     0x02    /* 'a'-'z' */
#define _ACPI_PU     0x10    /* punctuation */
#define _ACPI_SP     0x08    /* space, tab, CR, LF, VT, FF */
#define _ACPI_UP     0x01    /* 'A'-'Z' */
#define _ACPI_XD     0x80    /* '0'-'9', 'A'-'F', 'a'-'f' */

#define isdigit(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_DI))
#define isspace(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_SP))
#define isxdigit(c) (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_XD))
#define isupper(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_UP))
#define islower(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_LO))
#define isprint(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_LO | _ACPI_UP | _ACPI_DI | _ACPI_XS | _ACPI_PU))
#define isalpha(c)  (AcpiGbl_Ctypes[(unsigned char)(c)] & (_ACPI_LO | _ACPI_UP))

/*
 * XNU doesn't export some Libc functions that ACPI CA wants, so we provide it instead.
 *
 * if anyone links to PDACPIPlatform for them, let them know it's PPI and not for general use.
 */

int tolower(int c);
int toupper(int c);
const char *strstr(const char *s1, const char *s2);

// HACK: kernel string.h is borked on at *least* 10.15.
#if defined(strcpy) && __has_builtin(__builtin___strcpy_chk)
#undef strcpy

#define strcpy(dest, src) __builtin___strcpy_chk(dest, src, __builtin_object_size(dest, 1))
#endif

/* This is to compensate for the fact that the public SDK headers are stripped of these functions */
extern unsigned int ml_phys_read_byte(vm_offset_t paddr);
extern unsigned int ml_phys_read_byte_64(addr64_t paddr);
extern unsigned int ml_phys_read_half(vm_offset_t paddr);
extern unsigned int ml_phys_read_half_64(addr64_t paddr);
extern unsigned int ml_phys_read_word(vm_offset_t paddr);
extern unsigned int ml_phys_read_word_64(addr64_t paddr);
extern unsigned long long ml_phys_read_double(vm_offset_t paddr);
extern unsigned long long ml_phys_read_double_64(addr64_t paddr);
extern void ml_phys_write_byte(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_byte_64(addr64_t paddr, unsigned int data);
extern void ml_phys_write_half(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_half_64(addr64_t paddr, unsigned int data);
extern void ml_phys_write_word(vm_offset_t paddr, unsigned int data);
extern void ml_phys_write_word_64(addr64_t paddr, unsigned int data);
extern void ml_phys_write_double(vm_offset_t paddr, unsigned long long data);
extern void ml_phys_write_double_64(addr64_t paddr, unsigned long long data);
extern void ml_port_io_write8(uint16_t ioport, uint8_t val);
extern void ml_port_io_write16(uint16_t ioport, uint16_t val);
extern void ml_port_io_write32(uint16_t ioport, uint32_t val);
extern uint8_t ml_port_io_read8(uint16_t ioport);
extern uint16_t ml_port_io_read16(uint16_t ioport);
extern uint32_t ml_port_io_read32(uint16_t ioport);

/* Will PDACPIPlatform even run in a 32-bit kernel? */
#if __LP64__
#define PHYS_READ_8(p) ml_phys_read_byte_64(p)
#define PHYS_READ_16(p) ml_phys_read_half_64(p)
#define PHYS_READ_32(p) ml_phys_read_word_64(p)
#define PHYS_READ_64(p) ml_phys_read_double_64(p)
#define PHYS_WRITE_8(p, v) ml_phys_write_byte_64(p, (v & 0x000000FF))
#define PHYS_WRITE_16(p, v) ml_phys_write_half_64(p, (v & 0x0000FFFF))
#define PHYS_WRITE_32(p, v) ml_phys_write_word_64(p, (v))
#define PHYS_WRITE_64(p, v) ml_phys_write_double_64(p, v)
#else
#define PHYS_READ_8(p) ml_phys_read_byte(p)
#define PHYS_READ_16(p) ml_phys_read_half(p)
#define PHYS_READ_32(p) ml_phys_read_word(p)
#define PHYS_READ_64(p) ml_phys_read_double(p)
#define PHYS_WRITE_8(p, v) ml_phys_write_byte(p, (v & 0x000000FF))
#define PHYS_WRITE_16(p, v) ml_phys_write_half(p, (v & 0x0000FFFF))
#define PHYS_WRITE_32(p, v) ml_phys_write_word(p, (v))
#define PHYS_WRITE_64(p, v) ml_phys_write_double(p, v)
#endif

#endif

#endif /* __ACDARWIN_H__ */
