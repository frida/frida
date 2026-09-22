/*
 * Macros for asm code.
 *
 * Copyright (c) 2019-2020, Arm Limited.
 * Copyright (c) 2022, Ole André Vadla Ravnås.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef __GUM_ASMDEFS_H__
#define __GUM_ASMDEFS_H__

#ifdef __APPLE__
# define GUM_SYMBOL(s) _ ## s
#else
# define GUM_SYMBOL(s) s
#endif

/* Branch Target Identitication support.  */
#define BTI_C		hint	34
#define BTI_J		hint	36
/* Return address signing support (pac-ret).  */
#define PACIASP		hint	25; .cfi_window_save
#define AUTIASP		hint	29; .cfi_window_save

/* GNU_PROPERTY_AARCH64_* macros from elf.h.  */
#define FEATURE_1_AND 0xc0000000
#define FEATURE_1_BTI 1
#define FEATURE_1_PAC 2

/* Add a NT_GNU_PROPERTY_TYPE_0 note.  */
#define GNU_PROPERTY(type, value)	\
  .section .note.gnu.property, "a";	\
  .p2align 3;				\
  .word 4;				\
  .word 16;				\
  .word 5;				\
  .asciz "GNU";				\
  .word type;				\
  .word 4;				\
  .word value;				\
  .word 0;				\
  .text

/* If set then the GNU Property Note section will be added to
   mark objects to support BTI and PAC-RET.  */
#if !defined (WANT_GNU_PROPERTY) && !defined (__APPLE__)
# define WANT_GNU_PROPERTY 1
#endif

#if WANT_GNU_PROPERTY
/* Add property note with supported features to all asm files.  */
GNU_PROPERTY (FEATURE_1_AND, FEATURE_1_BTI|FEATURE_1_PAC)
#endif

.macro ENTRY name
  .global GUM_SYMBOL(\name)
#ifndef __APPLE__
  .type GUM_SYMBOL(\name), %function
#endif
  .align 6
  GUM_SYMBOL(\name):
  .cfi_startproc
  BTI_C
.endm

.macro ENTRY_ALIAS name
  .global GUM_SYMBOL(\name)
#ifndef __APPLE__
  .type GUM_SYMBOL(\name), %function
#endif
  GUM_SYMBOL(\name):
.endm

.macro END name
  .cfi_endproc
#ifndef __APPLE__
  .size GUM_SYMBOL(\name), .-GUM_SYMBOL(\name)
#endif
.endm

#define L(l) .L ## l

#endif
