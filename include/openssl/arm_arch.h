/* ====================================================================
 * Copyright (c) 1998-2011 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#ifndef OPENSSL_HEADER_ARM_ARCH_H
#define OPENSSL_HEADER_ARM_ARCH_H

#if !defined(__ARM_ARCH__)
# if defined(__CC_ARM)
#  define __ARM_ARCH__ __TARGET_ARCH_ARM
#  if defined(__BIG_ENDIAN)
#   define __ARMEB__
#  else
#   define __ARMEL__
#  endif
# elif defined(__GNUC__)
#  if defined(__aarch64__)
#    define __ARM_ARCH__ 8
#    if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define __ARMEB__
#    else
#      define __ARMEL__
#    endif
  // Why doesn't gcc define __ARM_ARCH__? Instead it defines
  // bunch of below macros. See all_architectires[] table in
  // gcc/config/arm/arm.c. On a side note it defines
  // __ARMEL__/__ARMEB__ for little-/big-endian.
#  elif	defined(__ARM_ARCH)
#    define __ARM_ARCH__ __ARM_ARCH
#  elif	defined(__ARM_ARCH_8A__)
#    define __ARM_ARCH__ 8
#  elif	defined(__ARM_ARCH_7__)	|| defined(__ARM_ARCH_7A__)	|| \
	defined(__ARM_ARCH_7R__)|| defined(__ARM_ARCH_7M__)	|| \
	defined(__ARM_ARCH_7EM__)
#   define __ARM_ARCH__ 7
#  elif	defined(__ARM_ARCH_6__)	|| defined(__ARM_ARCH_6J__)	|| \
	defined(__ARM_ARCH_6K__)|| defined(__ARM_ARCH_6M__)	|| \
	defined(__ARM_ARCH_6Z__)|| defined(__ARM_ARCH_6ZK__)	|| \
	defined(__ARM_ARCH_6T2__)
#   define __ARM_ARCH__ 6
#  elif	defined(__ARM_ARCH_5__)	|| defined(__ARM_ARCH_5T__)	|| \
	defined(__ARM_ARCH_5E__)|| defined(__ARM_ARCH_5TE__)	|| \
	defined(__ARM_ARCH_5TEJ__)
#   define __ARM_ARCH__ 5
#  elif	defined(__ARM_ARCH_4__)	|| defined(__ARM_ARCH_4T__)
#   define __ARM_ARCH__ 4
#  else
#   error "unsupported ARM architecture"
#  endif
# endif
#endif

// Even when building for 32-bit ARM, support for aarch64 crypto instructions
// will be included.
#define __ARM_MAX_ARCH__ 8

// ARMV7_NEON is true when a NEON unit is present in the current CPU.
#define ARMV7_NEON (1 << 0)

// ARMV8_AES indicates support for hardware AES instructions.
#define ARMV8_AES (1 << 2)

// ARMV8_SHA1 indicates support for hardware SHA-1 instructions.
#define ARMV8_SHA1 (1 << 3)

// ARMV8_SHA256 indicates support for hardware SHA-256 instructions.
#define ARMV8_SHA256 (1 << 4)

// ARMV8_PMULL indicates support for carryless multiplication.
#define ARMV8_PMULL (1 << 5)

#if defined(__ASSEMBLER__)

// Support macros for
//   - Armv8.3-A Pointer Authentication and
//   - Armv8.5-A Branch Target Identification
// features which require emitting a .note.gnu.property section with the
// appropriate architecture-dependent feature bits set.
// Read more: "ELF for the Arm® 64-bit Architecture"

#if (__ARM_FEATURE_BTI_DEFAULT == 1)
# define __aarch64_feature_bti (1 << 0) // Has Branch Target Identification
# define __entry_bp_call hint #34 // BTI C
#else
# define __aarch64_feature_bti 0 // No Branch Target Identification
# define __entry_bp_call
#endif

#if ((__ARM_FEATURE_PAC_DEFAULT & 1) == 1) // Signed with A-key
# define __aarch64_feature_pac (1 << 1) // Has Pointer Authentication
# define __entry_bp_standard hint #25 // PACIASP
# define __exit_bp_standard hint #29 // AUTIASP
#elif ((__ARM_FEATURE_PAC_DEFAULT & 2) == 2)  // Signed with B-key
# define __aarch64_feature_pac (1 << 1) // Has Pointer Authentication
# define __entry_bp_standard hint #27 // PACIBSP
# define __exit_bp_standard hint #31 // AUTIBSP
#else
# define __aarch64_feature_pac 0 // No Pointer Authentication
# if defined(__ARM_FEATURE_BTI_DEFAULT)
#  define __entry_bp_standard __entry_bp_call
# else
#  define __entry_bp_standard
# endif
# define __exit_bp_standard
#endif

#if (__aarch64_feature_pac != 0) || (__aarch64_feature_bti != 0)
# define ARMV8_NOTE_GNU_PROPERTY() \
  .section .note.gnu.property, "a"; \
  .balign 8; \
  .long 4; \
  .long 0x10; \
  .long 0x5; \
  .asciz "GNU"; \
  .long 0xc0000000; /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */ \
  .long 4; \
  .long (__aarch64_feature_pac | \
         __aarch64_feature_bti); \
  .long 0; \

#else
# define ARMV8_NOTE_GNU_PROPERTY()
#endif

// Emit the note section here for assembly files.
ARMV8_NOTE_GNU_PROPERTY()

#endif  /* defined __ASSEMBLER__ */

#endif  // OPENSSL_HEADER_ARM_ARCH_H
