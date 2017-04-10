/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <openssl/rsa.h>

#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/nid.h>

#include "../bn/internal.h"
#include "../internal.h"
#include "../test/test_util.h"
#include "internal.h"


// kPlaintext is a sample plaintext.
static const uint8_t kPlaintext[] = "\x54\x85\x9b\x34\x2c\x49\xea\x2a";
static const size_t kPlaintextLen = sizeof(kPlaintext) - 1;

// kKey1 is a DER-encoded RSAPrivateKey.
static const uint8_t kKey1[] =
    "\x30\x82\x01\x38\x02\x01\x00\x02\x41\x00\xaa\x36\xab\xce\x88\xac\xfd\xff"
    "\x55\x52\x3c\x7f\xc4\x52\x3f\x90\xef\xa0\x0d\xf3\x77\x4a\x25\x9f\x2e\x62"
    "\xb4\xc5\xd9\x9c\xb5\xad\xb3\x00\xa0\x28\x5e\x53\x01\x93\x0e\x0c\x70\xfb"
    "\x68\x76\x93\x9c\xe6\x16\xce\x62\x4a\x11\xe0\x08\x6d\x34\x1e\xbc\xac\xa0"
    "\xa1\xf5\x02\x01\x11\x02\x40\x0a\x03\x37\x48\x62\x64\x87\x69\x5f\x5f\x30"
    "\xbc\x38\xb9\x8b\x44\xc2\xcd\x2d\xff\x43\x40\x98\xcd\x20\xd8\xa1\x38\xd0"
    "\x90\xbf\x64\x79\x7c\x3f\xa7\xa2\xcd\xcb\x3c\xd1\xe0\xbd\xba\x26\x54\xb4"
    "\xf9\xdf\x8e\x8a\xe5\x9d\x73\x3d\x9f\x33\xb3\x01\x62\x4a\xfd\x1d\x51\x02"
    "\x21\x00\xd8\x40\xb4\x16\x66\xb4\x2e\x92\xea\x0d\xa3\xb4\x32\x04\xb5\xcf"
    "\xce\x33\x52\x52\x4d\x04\x16\xa5\xa4\x41\xe7\x00\xaf\x46\x12\x0d\x02\x21"
    "\x00\xc9\x7f\xb1\xf0\x27\xf4\x53\xf6\x34\x12\x33\xea\xaa\xd1\xd9\x35\x3f"
    "\x6c\x42\xd0\x88\x66\xb1\xd0\x5a\x0f\x20\x35\x02\x8b\x9d\x89\x02\x20\x59"
    "\x0b\x95\x72\xa2\xc2\xa9\xc4\x06\x05\x9d\xc2\xab\x2f\x1d\xaf\xeb\x7e\x8b"
    "\x4f\x10\xa7\x54\x9e\x8e\xed\xf5\xb4\xfc\xe0\x9e\x05\x02\x21\x00\x8e\x3c"
    "\x05\x21\xfe\x15\xe0\xea\x06\xa3\x6f\xf0\xf1\x0c\x99\x52\xc3\x5b\x7a\x75"
    "\x14\xfd\x32\x38\xb8\x0a\xad\x52\x98\x62\x8d\x51\x02\x20\x36\x3f\xf7\x18"
    "\x9d\xa8\xe9\x0b\x1d\x34\x1f\x71\xd0\x9b\x76\xa8\xa9\x43\xe1\x1d\x10\xb2"
    "\x4d\x24\x9f\x2d\xea\xfe\xf8\x0c\x18\x26";

// kOAEPCiphertext1 is a sample encryption of |kPlaintext| with |kKey1| using
// RSA OAEP.
static const uint8_t kOAEPCiphertext1[] =
    "\x1b\x8f\x05\xf9\xca\x1a\x79\x52\x6e\x53\xf3\xcc\x51\x4f\xdb\x89\x2b\xfb"
    "\x91\x93\x23\x1e\x78\xb9\x92\xe6\x8d\x50\xa4\x80\xcb\x52\x33\x89\x5c\x74"
    "\x95\x8d\x5d\x02\xab\x8c\x0f\xd0\x40\xeb\x58\x44\xb0\x05\xc3\x9e\xd8\x27"
    "\x4a\x9d\xbf\xa8\x06\x71\x40\x94\x39\xd2";

// kKey2 is a DER-encoded RSAPrivateKey.
static const uint8_t kKey2[] =
    "\x30\x81\xfb\x02\x01\x00\x02\x33\x00\xa3\x07\x9a\x90\xdf\x0d\xfd\x72\xac"
    "\x09\x0c\xcc\x2a\x78\xb8\x74\x13\x13\x3e\x40\x75\x9c\x98\xfa\xf8\x20\x4f"
    "\x35\x8a\x0b\x26\x3c\x67\x70\xe7\x83\xa9\x3b\x69\x71\xb7\x37\x79\xd2\x71"
    "\x7b\xe8\x34\x77\xcf\x02\x01\x03\x02\x32\x6c\xaf\xbc\x60\x94\xb3\xfe\x4c"
    "\x72\xb0\xb3\x32\xc6\xfb\x25\xa2\xb7\x62\x29\x80\x4e\x68\x65\xfc\xa4\x5a"
    "\x74\xdf\x0f\x8f\xb8\x41\x3b\x52\xc0\xd0\xe5\x3d\x9b\x59\x0f\xf1\x9b\xe7"
    "\x9f\x49\xdd\x21\xe5\xeb\x02\x1a\x00\xcf\x20\x35\x02\x8b\x9d\x86\x98\x40"
    "\xb4\x16\x66\xb4\x2e\x92\xea\x0d\xa3\xb4\x32\x04\xb5\xcf\xce\x91\x02\x1a"
    "\x00\xc9\x7f\xb1\xf0\x27\xf4\x53\xf6\x34\x12\x33\xea\xaa\xd1\xd9\x35\x3f"
    "\x6c\x42\xd0\x88\x66\xb1\xd0\x5f\x02\x1a\x00\x8a\x15\x78\xac\x5d\x13\xaf"
    "\x10\x2b\x22\xb9\x99\xcd\x74\x61\xf1\x5e\x6d\x22\xcc\x03\x23\xdf\xdf\x0b"
    "\x02\x1a\x00\x86\x55\x21\x4a\xc5\x4d\x8d\x4e\xcd\x61\x77\xf1\xc7\x36\x90"
    "\xce\x2a\x48\x2c\x8b\x05\x99\xcb\xe0\x3f\x02\x1a\x00\x83\xef\xef\xb8\xa9"
    "\xa4\x0d\x1d\xb6\xed\x98\xad\x84\xed\x13\x35\xdc\xc1\x08\xf3\x22\xd0\x57"
    "\xcf\x8d";

// kOAEPCiphertext2 is a sample encryption of |kPlaintext| with |kKey2| using
// RSA OAEP.
static const uint8_t kOAEPCiphertext2[] =
    "\x14\xbd\xdd\x28\xc9\x83\x35\x19\x23\x80\xe8\xe5\x49\xb1\x58\x2a\x8b\x40"
    "\xb4\x48\x6d\x03\xa6\xa5\x31\x1f\x1f\xd5\xf0\xa1\x80\xe4\x17\x53\x03\x29"
    "\xa9\x34\x90\x74\xb1\x52\x13\x54\x29\x08\x24\x52\x62\x51";

// kKey3 is a DER-encoded RSAPrivateKey.
static const uint8_t kKey3[] =
    "\x30\x82\x02\x5b\x02\x01\x00\x02\x81\x81\x00\xbb\xf8\x2f\x09\x06\x82\xce"
    "\x9c\x23\x38\xac\x2b\x9d\xa8\x71\xf7\x36\x8d\x07\xee\xd4\x10\x43\xa4\x40"
    "\xd6\xb6\xf0\x74\x54\xf5\x1f\xb8\xdf\xba\xaf\x03\x5c\x02\xab\x61\xea\x48"
    "\xce\xeb\x6f\xcd\x48\x76\xed\x52\x0d\x60\xe1\xec\x46\x19\x71\x9d\x8a\x5b"
    "\x8b\x80\x7f\xaf\xb8\xe0\xa3\xdf\xc7\x37\x72\x3e\xe6\xb4\xb7\xd9\x3a\x25"
    "\x84\xee\x6a\x64\x9d\x06\x09\x53\x74\x88\x34\xb2\x45\x45\x98\x39\x4e\xe0"
    "\xaa\xb1\x2d\x7b\x61\xa5\x1f\x52\x7a\x9a\x41\xf6\xc1\x68\x7f\xe2\x53\x72"
    "\x98\xca\x2a\x8f\x59\x46\xf8\xe5\xfd\x09\x1d\xbd\xcb\x02\x01\x11\x02\x81"
    "\x81\x00\xa5\xda\xfc\x53\x41\xfa\xf2\x89\xc4\xb9\x88\xdb\x30\xc1\xcd\xf8"
    "\x3f\x31\x25\x1e\x06\x68\xb4\x27\x84\x81\x38\x01\x57\x96\x41\xb2\x94\x10"
    "\xb3\xc7\x99\x8d\x6b\xc4\x65\x74\x5e\x5c\x39\x26\x69\xd6\x87\x0d\xa2\xc0"
    "\x82\xa9\x39\xe3\x7f\xdc\xb8\x2e\xc9\x3e\xda\xc9\x7f\xf3\xad\x59\x50\xac"
    "\xcf\xbc\x11\x1c\x76\xf1\xa9\x52\x94\x44\xe5\x6a\xaf\x68\xc5\x6c\x09\x2c"
    "\xd3\x8d\xc3\xbe\xf5\xd2\x0a\x93\x99\x26\xed\x4f\x74\xa1\x3e\xdd\xfb\xe1"
    "\xa1\xce\xcc\x48\x94\xaf\x94\x28\xc2\xb7\xb8\x88\x3f\xe4\x46\x3a\x4b\xc8"
    "\x5b\x1c\xb3\xc1\x02\x41\x00\xee\xcf\xae\x81\xb1\xb9\xb3\xc9\x08\x81\x0b"
    "\x10\xa1\xb5\x60\x01\x99\xeb\x9f\x44\xae\xf4\xfd\xa4\x93\xb8\x1a\x9e\x3d"
    "\x84\xf6\x32\x12\x4e\xf0\x23\x6e\x5d\x1e\x3b\x7e\x28\xfa\xe7\xaa\x04\x0a"
    "\x2d\x5b\x25\x21\x76\x45\x9d\x1f\x39\x75\x41\xba\x2a\x58\xfb\x65\x99\x02"
    "\x41\x00\xc9\x7f\xb1\xf0\x27\xf4\x53\xf6\x34\x12\x33\xea\xaa\xd1\xd9\x35"
    "\x3f\x6c\x42\xd0\x88\x66\xb1\xd0\x5a\x0f\x20\x35\x02\x8b\x9d\x86\x98\x40"
    "\xb4\x16\x66\xb4\x2e\x92\xea\x0d\xa3\xb4\x32\x04\xb5\xcf\xce\x33\x52\x52"
    "\x4d\x04\x16\xa5\xa4\x41\xe7\x00\xaf\x46\x15\x03\x02\x40\x54\x49\x4c\xa6"
    "\x3e\xba\x03\x37\xe4\xe2\x40\x23\xfc\xd6\x9a\x5a\xeb\x07\xdd\xdc\x01\x83"
    "\xa4\xd0\xac\x9b\x54\xb0\x51\xf2\xb1\x3e\xd9\x49\x09\x75\xea\xb7\x74\x14"
    "\xff\x59\xc1\xf7\x69\x2e\x9a\x2e\x20\x2b\x38\xfc\x91\x0a\x47\x41\x74\xad"
    "\xc9\x3c\x1f\x67\xc9\x81\x02\x40\x47\x1e\x02\x90\xff\x0a\xf0\x75\x03\x51"
    "\xb7\xf8\x78\x86\x4c\xa9\x61\xad\xbd\x3a\x8a\x7e\x99\x1c\x5c\x05\x56\xa9"
    "\x4c\x31\x46\xa7\xf9\x80\x3f\x8f\x6f\x8a\xe3\x42\xe9\x31\xfd\x8a\xe4\x7a"
    "\x22\x0d\x1b\x99\xa4\x95\x84\x98\x07\xfe\x39\xf9\x24\x5a\x98\x36\xda\x3d"
    "\x02\x41\x00\xb0\x6c\x4f\xda\xbb\x63\x01\x19\x8d\x26\x5b\xdb\xae\x94\x23"
    "\xb3\x80\xf2\x71\xf7\x34\x53\x88\x50\x93\x07\x7f\xcd\x39\xe2\x11\x9f\xc9"
    "\x86\x32\x15\x4f\x58\x83\xb1\x67\xa9\x67\xbf\x40\x2b\x4e\x9e\x2e\x0f\x96"
    "\x56\xe6\x98\xea\x36\x66\xed\xfb\x25\x79\x80\x39\xf7";

// kOAEPCiphertext3 is a sample encryption of |kPlaintext| with |kKey3| using
// RSA OAEP.
static const uint8_t kOAEPCiphertext3[] =
    "\xb8\x24\x6b\x56\xa6\xed\x58\x81\xae\xb5\x85\xd9\xa2\x5b\x2a\xd7\x90\xc4"
    "\x17\xe0\x80\x68\x1b\xf1\xac\x2b\xc3\xde\xb6\x9d\x8b\xce\xf0\xc4\x36\x6f"
    "\xec\x40\x0a\xf0\x52\xa7\x2e\x9b\x0e\xff\xb5\xb3\xf2\xf1\x92\xdb\xea\xca"
    "\x03\xc1\x27\x40\x05\x71\x13\xbf\x1f\x06\x69\xac\x22\xe9\xf3\xa7\x85\x2e"
    "\x3c\x15\xd9\x13\xca\xb0\xb8\x86\x3a\x95\xc9\x92\x94\xce\x86\x74\x21\x49"
    "\x54\x61\x03\x46\xf4\xd4\x74\xb2\x6f\x7c\x48\xb4\x2e\xe6\x8e\x1f\x57\x2a"
    "\x1f\xc4\x02\x6a\xc4\x56\xb4\xf5\x9f\x7b\x62\x1e\xa1\xb9\xd8\x8f\x64\x20"
    "\x2f\xb1";

static const uint8_t kTwoPrimeKey[] =
    "\x30\x82\x04\xa1\x02\x01\x00\x02\x82\x01\x01\x00\x93\x3a\x4f\xc9\x6a\x0a"
    "\x6b\x28\x04\xfa\xb7\x05\x56\xdf\xa0\xaa\x4f\xaa\xab\x94\xa0\xa9\x25\xef"
    "\xc5\x96\xd2\xd4\x66\x16\x62\x2c\x13\x7b\x91\xd0\x36\x0a\x10\x11\x6d\x7a"
    "\x91\xb6\xe4\x74\x57\xc1\x3d\x7a\xbe\x24\x05\x3a\x04\x0b\x73\x91\x53\xb1"
    "\x74\x10\xe1\x87\xdc\x91\x28\x9c\x1e\xe5\xf2\xb9\xfc\xa2\x48\x34\xb6\x78"
    "\xed\x6d\x95\xfb\xf2\xc0\x4e\x1c\xa4\x15\x00\x3c\x8a\x68\x2b\xd6\xce\xd5"
    "\xb3\x9f\x66\x02\xa7\x0d\x08\xa3\x23\x9b\xe5\x36\x96\x13\x22\xf9\x69\xa6"
    "\x87\x88\x9b\x85\x3f\x83\x9c\xab\x1a\x1b\x6d\x8d\x16\xf4\x5e\xbd\xee\x4b"
    "\x59\x56\xf8\x9d\x58\xcd\xd2\x83\x85\x59\x43\x84\x63\x4f\xe6\x1a\x86\x66"
    "\x0d\xb5\xa0\x87\x89\xb6\x13\x82\x43\xda\x34\x92\x3b\x68\xc4\x95\x71\x2f"
    "\x15\xc2\xe0\x43\x67\x3c\x08\x00\x36\x10\xc3\xb4\x46\x4c\x4e\x6e\xf5\x44"
    "\xa9\x04\x44\x9d\xce\xc7\x05\x79\xee\x11\xcf\xaf\x2c\xd7\x9a\x32\xd3\xa5"
    "\x30\xd4\x3a\x78\x43\x37\x74\x22\x90\x24\x04\x11\xd7\x95\x08\x52\xa4\x71"
    "\x41\x68\x94\xb0\xa0\xc3\xec\x4e\xd2\xc4\x30\x71\x98\x64\x9c\xe3\x7c\x76"
    "\xef\x33\xa3\x2b\xb1\x87\x63\xd2\x5c\x09\xfc\x90\x2d\x92\xf4\x57\x02\x01"
    "\x03\x02\x82\x01\x00\x62\x26\xdf\xdb\x9c\x06\xf2\x1a\xad\xfc\x7a\x03\x8f"
    "\x3f\xc0\x71\x8a\x71\xc7\xb8\x6b\x1b\x6e\x9f\xd9\x0f\x37\x38\x44\x0e\xec"
    "\x1d\x62\x52\x61\x35\x79\x5c\x0a\xb6\x48\xfc\x61\x24\x98\x4d\x8f\xd6\x28"
    "\xfc\x7e\xc2\xae\x26\xad\x5c\xf7\xb6\x37\xcb\xa2\xb5\xeb\xaf\xe8\x60\xc5"
    "\xbd\x69\xee\xa1\xd1\x53\x16\xda\xcd\xce\xfb\x48\xf3\xb9\x52\xa1\xd5\x89"
    "\x68\x6d\x63\x55\x7d\xb1\x9a\xc7\xe4\x89\xe3\xcd\x14\xee\xac\x6f\x5e\x05"
    "\xc2\x17\xbd\x43\x79\xb9\x62\x17\x50\xf1\x19\xaf\xb0\x67\xae\x2a\x57\xbd"
    "\xc7\x66\xbc\xf3\xb3\x64\xa1\xe3\x16\x74\x9e\xea\x02\x5c\xab\x94\xd8\x97"
    "\x02\x42\x0c\x2c\xba\x54\xb9\xaf\xe0\x45\x93\xad\x7f\xb3\x10\x6a\x96\x50"
    "\x4b\xaf\xcf\xc8\x27\x62\x2d\x83\xe9\x26\xc6\x94\xc1\xef\x5c\x8e\x06\x42"
    "\x53\xe5\x56\xaf\xc2\x99\x01\xaa\x9a\x71\xbc\xe8\x21\x33\x2a\x2d\xa3\x36"
    "\xac\x1b\x86\x19\xf8\xcd\x1f\x80\xa4\x26\x98\xb8\x9f\x62\x62\xd5\x1a\x7f"
    "\xee\xdb\xdf\x81\xd3\x21\xdb\x33\x92\xee\xff\xe2\x2f\x32\x77\x73\x6a\x58"
    "\xab\x21\xf3\xe3\xe1\xbc\x4f\x12\x72\xa6\xb5\xc2\xfb\x27\x9e\xc8\xca\xab"
    "\x64\xa0\x87\x07\x9d\xef\xca\x0f\xdb\x02\x81\x81\x00\xe6\xd3\x4d\xc0\xa1"
    "\x91\x0e\x62\xfd\xb0\xdd\xc6\x30\xb8\x8c\xcb\x14\xc1\x4b\x69\x30\xdd\xcd"
    "\x86\x67\xcb\x37\x14\xc5\x03\xd2\xb4\x69\xab\x3d\xe5\x16\x81\x0f\xe5\x50"
    "\xf4\x18\xb1\xec\xbc\x71\xe9\x80\x99\x06\xe4\xa3\xfe\x44\x84\x4a\x2d\x1e"
    "\x07\x7f\x22\x70\x6d\x4f\xd4\x93\x0b\x8b\x99\xce\x1e\xab\xcd\x4c\xd2\xd3"
    "\x10\x47\x5c\x09\x9f\x6d\x82\xc0\x08\x75\xe3\x3d\x83\xc2\x19\x50\x29\xec"
    "\x1f\x84\x29\xcc\xf1\x56\xee\xbd\x54\x5d\xe6\x19\xdf\x0d\x1c\xa4\xbb\x0a"
    "\xfe\x84\x44\x29\x1d\xf9\x5c\x80\x96\x5b\x24\xb4\xf7\x02\x1b\x02\x81\x81"
    "\x00\xa3\x48\xf1\x9c\x58\xc2\x5f\x38\xfb\xd8\x12\x39\xf1\x8e\x73\xa1\xcf"
    "\x78\x12\xe0\xed\x2a\xbb\xef\xac\x23\xb2\xbf\xd6\x0c\xe9\x6e\x1e\xab\xea"
    "\x3f\x68\x36\xa7\x1f\xe5\xab\xe0\x86\xa5\x76\x32\x98\xdd\x75\xb5\x2b\xbc"
    "\xcb\x8a\x03\x00\x7c\x2e\xca\xf8\xbc\x19\xe4\xe3\xa3\x31\xbd\x1d\x20\x2b"
    "\x09\xad\x6f\x4c\xed\x48\xd4\xdf\x87\xf9\xf0\x46\xb9\x86\x4c\x4b\x71\xe7"
    "\x48\x78\xdc\xed\xc7\x82\x02\x44\xd3\xa6\xb3\x10\x5f\x62\x81\xfc\xb8\xe4"
    "\x0e\xf4\x1a\xdd\xab\x3f\xbc\x63\x79\x5b\x39\x69\x5e\xea\xa9\x15\xfe\x90"
    "\xec\xda\x75\x02\x81\x81\x00\x99\xe2\x33\xd5\xc1\x0b\x5e\xec\xa9\x20\x93"
    "\xd9\x75\xd0\x5d\xdc\xb8\x80\xdc\xf0\xcb\x3e\x89\x04\x45\x32\x24\xb8\x83"
    "\x57\xe1\xcd\x9b\xc7\x7e\x98\xb9\xab\x5f\xee\x35\xf8\x10\x76\x9d\xd2\xf6"
    "\x9b\xab\x10\xaf\x43\x17\xfe\xd8\x58\x31\x73\x69\x5a\x54\xc1\xa0\x48\xdf"
    "\xe3\x0c\xb2\x5d\x11\x34\x14\x72\x88\xdd\xe1\xe2\x0a\xda\x3d\x5b\xbf\x9e"
    "\x57\x2a\xb0\x4e\x97\x7e\x57\xd6\xbb\x8a\xc6\x9d\x6a\x58\x1b\xdd\xf6\x39"
    "\xf4\x7e\x38\x3e\x99\x66\x94\xb3\x68\x6d\xd2\x07\x54\x58\x2d\x70\xbe\xa6"
    "\x3d\xab\x0e\xe7\x6d\xcd\xfa\x01\x67\x02\x81\x80\x6c\xdb\x4b\xbd\x90\x81"
    "\x94\xd0\xa7\xe5\x61\x7b\xf6\x5e\xf7\xc1\x34\xfa\xb7\x40\x9e\x1c\x7d\x4a"
    "\x72\xc2\x77\x2a\x8e\xb3\x46\x49\x69\xc7\xf1\x7f\x9a\xcf\x1a\x15\x43\xc7"
    "\xeb\x04\x6e\x4e\xcc\x65\xe8\xf9\x23\x72\x7d\xdd\x06\xac\xaa\xfd\x74\x87"
    "\x50\x7d\x66\x98\x97\xc2\x21\x28\xbe\x15\x72\x06\x73\x9f\x88\x9e\x30\x8d"
    "\xea\x5a\xa6\xa0\x2f\x26\x59\x88\x32\x4b\xef\x85\xa5\xe8\x9e\x85\x01\x56"
    "\xd8\x8d\x19\xcc\xb5\x94\xec\x56\xa8\x7b\x42\xb4\xa2\xbc\x93\xc7\x7f\xd2"
    "\xec\xfb\x92\x26\x46\x3f\x47\x1b\x63\xff\x0b\x48\x91\xa3\x02\x81\x80\x2c"
    "\x4a\xb9\xa4\x46\x7b\xff\x50\x7e\xbf\x60\x47\x3b\x2b\x66\x82\xdc\x0e\x53"
    "\x65\x71\xe9\xda\x2a\xb8\x32\x93\x42\xb7\xff\xea\x67\x66\xf1\xbc\x87\x28"
    "\x65\x29\x79\xca\xab\x93\x56\xda\x95\xc1\x26\x44\x3d\x27\xc1\x91\xc6\x9b"
    "\xd9\xec\x9d\xb7\x49\xe7\x16\xee\x99\x87\x50\x95\x81\xd4\x5c\x5b\x5a\x5d"
    "\x0a\x43\xa5\xa7\x8f\x5a\x80\x49\xa0\xb7\x10\x85\xc7\xf4\x42\x34\x86\xb6"
    "\x5f\x3f\x88\x9e\xc7\xf5\x59\x29\x39\x68\x48\xf2\xd7\x08\x5b\x92\x8e\x6b"
    "\xea\xa5\x63\x5f\xc0\xfb\xe4\xe1\xb2\x7d\xb7\x40\xe9\x55\x06\xbf\x58\x25"
    "\x6f";

static const uint8_t kTwoPrimeEncryptedMessage[] = {
    0x63, 0x0a, 0x30, 0x45, 0x43, 0x11, 0x45, 0xb7, 0x99, 0x67, 0x90, 0x35,
    0x37, 0x27, 0xff, 0xbc, 0xe0, 0xbf, 0xa6, 0xd1, 0x47, 0x50, 0xbb, 0x6c,
    0x1c, 0xaa, 0x66, 0xf2, 0xff, 0x9d, 0x9a, 0xa6, 0xb4, 0x16, 0x63, 0xb0,
    0xa1, 0x7c, 0x7c, 0x0c, 0xef, 0xb3, 0x66, 0x52, 0x42, 0xd7, 0x5e, 0xf3,
    0xa4, 0x15, 0x33, 0x40, 0x43, 0xe8, 0xb1, 0xfc, 0xe0, 0x42, 0x83, 0x46,
    0x28, 0xce, 0xde, 0x7b, 0x01, 0xeb, 0x28, 0x92, 0x70, 0xdf, 0x8d, 0x54,
    0x9e, 0xed, 0x23, 0xb4, 0x78, 0xc3, 0xca, 0x85, 0x53, 0x48, 0xd6, 0x8a,
    0x87, 0xf7, 0x69, 0xcd, 0x82, 0x8c, 0x4f, 0x5c, 0x05, 0x55, 0xa6, 0x78,
    0x89, 0xab, 0x4c, 0xd8, 0xa9, 0xd6, 0xa5, 0xf4, 0x29, 0x4c, 0x23, 0xc8,
    0xcf, 0xf0, 0x4c, 0x64, 0x6b, 0x4e, 0x02, 0x17, 0x69, 0xd6, 0x47, 0x83,
    0x30, 0x43, 0x02, 0x29, 0xda, 0xda, 0x75, 0x3b, 0xd7, 0xa7, 0x2b, 0x31,
    0xb3, 0xe9, 0x71, 0xa4, 0x41, 0xf7, 0x26, 0x9b, 0xcd, 0x23, 0xfa, 0x45,
    0x3c, 0x9b, 0x7d, 0x28, 0xf7, 0xf9, 0x67, 0x04, 0xba, 0xfc, 0x46, 0x75,
    0x11, 0x3c, 0xd5, 0x27, 0x43, 0x53, 0xb1, 0xb6, 0x9e, 0x18, 0xeb, 0x11,
    0xb4, 0x25, 0x20, 0x30, 0x0b, 0xe0, 0x1c, 0x17, 0x36, 0x22, 0x10, 0x0f,
    0x99, 0xb5, 0x50, 0x14, 0x73, 0x07, 0xf0, 0x2f, 0x5d, 0x4c, 0xe3, 0xf2,
    0x86, 0xc2, 0x05, 0xc8, 0x38, 0xed, 0xeb, 0x2a, 0x4a, 0xab, 0x76, 0xe3,
    0x1a, 0x75, 0x44, 0xf7, 0x6e, 0x94, 0xdc, 0x25, 0x62, 0x7e, 0x31, 0xca,
    0xc2, 0x73, 0x51, 0xb5, 0x03, 0xfb, 0xf9, 0xf6, 0xb5, 0x8d, 0x4e, 0x6c,
    0x21, 0x0e, 0xf9, 0x97, 0x26, 0x57, 0xf3, 0x52, 0x72, 0x07, 0xf8, 0xb4,
    0xcd, 0xb4, 0x39, 0xcf, 0xbf, 0x78, 0xcc, 0xb6, 0x87, 0xf9, 0xb7, 0x8b,
    0x6a, 0xce, 0x9f, 0xc8,
};

static const uint8_t kThreePrimeKey[] =
    "\x30\x82\x04\xd7\x02\x01\x01\x02\x82\x01\x00\x62\x91\xe9\xea\xb3\x5d\x6c"
    "\x29\xae\x21\x83\xbb\xb5\x82\xb1\x9e\xea\xe0\x64\x5b\x1e\x2f\x5e\x2c\x0a"
    "\x80\x3d\x29\xd4\xfa\x9a\xe7\x44\xe6\x21\xbd\x98\xc0\x3d\xe0\x53\x59\xae"
    "\xd3\x3e\xfe\xc4\xc2\xc4\x5a\x5a\x89\x07\xf4\x4f\xdc\xb0\x6a\xd4\x3e\x99"
    "\x7d\x7a\x97\x26\x4e\xe1\x93\xca\x6e\xed\x07\xfc\xb4\xfa\x95\x1e\x73\x7b"
    "\x86\x08\x6a\xb9\xd4\x29\xb0\x7e\x59\xb7\x9d\x7b\xeb\x67\x6e\xf0\xbb\x5e"
    "\xcf\xb9\xcd\x58\x93\xf0\xe7\x88\x17\x6c\x0d\x76\x1e\xb9\x27\x9a\x4d\x02"
    "\x16\xb6\x49\x6d\xa7\x83\x23\x4d\x02\x48\x0c\x0c\x1f\x0e\x85\x21\xe3\x06"
    "\x76\x0a\x73\xe6\xc1\x21\xfa\x30\x18\x78\x29\x5c\x31\xd0\x29\xae\x6f\x7d"
    "\x87\xd8\x2f\x16\xfa\xbc\x67\x8a\x94\x71\x59\x9b\xec\x22\x40\x55\x9f\xc2"
    "\x94\xb5\xbd\x78\x01\xc9\xef\x18\xc8\x6d\x0d\xdc\x53\x42\xb2\x5c\xab\x65"
    "\x05\xbd\x35\x08\x85\x1b\xf8\xe9\x47\xbc\xfe\xc5\xae\x47\x29\x63\x44\x8e"
    "\x4d\xb7\x47\xab\x0d\xd8\x76\x68\x4f\xc7\x07\x02\xe4\x86\xb0\xcf\xd8\x19"
    "\xad\xf4\x85\x76\x8b\x3b\x4e\x40\x8d\x29\x7a\x8a\x07\x36\xf3\x78\xae\x17"
    "\xa6\x8f\x53\x58\x65\x4c\x86\x9e\xd7\x8b\xec\x38\x4f\x99\xc7\x02\x01\x03"
    "\x02\x82\x01\x00\x41\xb6\x9b\xf1\xcc\xe8\xf2\xc6\x74\x16\x57\xd2\x79\x01"
    "\xcb\xbf\x47\x40\x42\xe7\x69\x74\xe9\x72\xb1\xaa\xd3\x71\x38\xa7\x11\xef"
    "\x83\x44\x16\x7e\x65\xd5\x7e\x95\x8c\xe6\x74\x8c\xd4\xa9\xd8\x81\xd8\x3c"
    "\x3c\x5b\x5a\xa2\xdf\xe8\x75\x9c\x8d\x7f\x10\xfe\x51\xba\x19\x89\xeb\xb7"
    "\xdc\x49\xf3\x5a\xa8\x78\xa7\x0e\x14\x4c\xfd\x04\x05\x9c\x7b\xe2\xc5\xa3"
    "\x04\xee\xd9\x4c\xfd\x7d\x47\xb0\x0d\x9b\x3d\x70\x91\x81\x2c\xab\x2b\x87"
    "\xad\x11\x68\x24\xfc\x2b\xd4\xee\x5e\x28\xeb\x6d\xab\xde\x0f\x77\x15\x58"
    "\x76\x39\xc9\x59\x3a\x7f\x19\x9d\xc6\x7e\x86\xe4\xd5\x38\x70\x9e\xae\xb9"
    "\xfb\x33\x33\xd1\x0c\x2d\xab\x01\x20\xe1\x8b\x29\x99\xd3\xeb\x87\x05\x72"
    "\xaa\x43\x58\x64\x8e\x9e\x31\xdb\x45\x9b\x2b\xac\x58\x80\x5d\x33\xa2\x43"
    "\x05\x96\xcc\xca\x2d\x04\x5f\xd6\xb7\x3d\x8b\x8f\x2d\xa3\xa5\xf8\x73\xf5"
    "\xd7\xc0\x19\xff\x10\xe6\xee\x3a\x26\x2f\xe1\x64\x3d\x11\xcd\x2d\xe4\x0a"
    "\x84\x27\xe3\xcb\x16\x62\x19\xe7\xe3\x0d\x13\xe8\x09\x5a\x53\xd0\x20\x56"
    "\x15\xf5\xb3\x67\xac\xa1\xb5\x94\x6b\xab\xdc\x71\xc7\xbf\x0a\xde\x76\xf5"
    "\x03\xa0\x30\xd8\x27\x9d\x00\x2b\x02\x57\x00\xf1\x4f\xc2\x86\x13\x06\x17"
    "\xf7\x69\x7e\x37\xdf\x67\xc5\x32\xa0\x74\x1c\x32\x69\x0f\x9f\x08\x88\x24"
    "\xb1\x51\xbc\xbc\x92\xba\x73\x1f\x9c\x75\xc2\x14\x6d\x4f\xc4\x5a\xcf\xda"
    "\x44\x35\x00\x6b\x42\x3b\x9f\x14\xf1\x05\xb3\x51\x22\xb6\xbe\x9c\xe0\xc1"
    "\x5c\x48\x61\xdf\x4e\x4c\x72\xb8\x05\x35\x7c\xac\xf1\xbb\xa0\x3b\x2a\xea"
    "\xf7\x86\xe9\xd2\xff\x1e\x1d\x02\x56\x00\xca\xb1\x39\xf6\xa2\xc6\x3b\x65"
    "\x45\x2f\x39\x00\xcd\x6e\xd6\x55\xf7\x71\x37\x89\xc2\xe7\x7a\xc0\x1a\xa6"
    "\x2f\xea\x17\x7c\xaa\x2a\x91\x8f\xd4\xc7\x50\x8b\xab\x8e\x99\x3b\x33\x91"
    "\xbc\x02\x10\x58\x4b\x58\x40\x9b\xc4\x8f\x48\x2b\xa7\x44\xfd\x07\x04\xf0"
    "\x98\x67\x56\xea\x25\x92\x8b\x2e\x4b\x4a\xa1\xd3\xc2\xa4\xb4\x9b\x59\x70"
    "\x32\xa6\xd8\x8b\xd9\x02\x57\x00\xa0\xdf\xd7\x04\x0c\xae\xba\xa4\xf0\xfe"
    "\xcf\xea\x45\x2e\x21\xc0\x4d\x68\x21\x9b\x5f\xbf\x5b\x05\x6d\xcb\x8b\xd3"
    "\x28\x61\xd1\xa2\x15\x12\xf9\x2c\x0d\x9e\x35\x2d\x91\xdf\xe6\xd8\x23\x55"
    "\x9c\xd6\xd2\x6a\x0d\xf6\x03\xcc\xe0\xc1\xcf\x29\xbd\xeb\x2b\x92\xda\xeb"
    "\xea\x34\x32\xf7\x25\x58\xce\x53\x1d\xf6\x7d\x15\x7c\xc7\x47\x4f\xaf\x46"
    "\x8c\xaa\x14\x13\x02\x56\x00\x87\x20\xd1\x4f\x17\x2e\xd2\x43\x83\x74\xd0"
    "\xab\x33\x9f\x39\x8e\xa4\xf6\x25\x06\x81\xef\xa7\x2a\xbc\x6e\xca\x9c\x0f"
    "\xa8\x71\x71\xb6\x5f\xe3\x2f\x8b\x07\xc7\xb4\x66\x27\x77\xb6\x7d\x56\xb5"
    "\x90\x32\x3a\xd5\xbd\x2d\xb4\xda\xc7\xc4\xd8\xa8\xaf\x58\xa0\x65\x9a\x39"
    "\xf1\x6e\x61\xb2\x1e\xdc\xdc\x6b\xe2\x81\xc3\x23\x12\x3b\xa0\x21\xc4\x90"
    "\x5d\x3b\x02\x57\x00\xe6\x8a\xaa\xb8\x6d\x2c\x81\x43\xb5\xd6\xa0\x2b\x42"
    "\x49\xa9\x0a\x51\xfa\x18\xc8\x32\xea\x54\x18\xf3\x60\xc2\xb5\x4a\x43\x05"
    "\x93\x9c\x01\xd9\x28\xed\x73\xfa\x82\xbc\x12\x64\xcb\xc4\x24\xa9\x3e\xae"
    "\x7c\x4b\x8f\x94\x57\x7b\x14\x10\x41\xdc\x62\x12\x8c\xb2\x4a\x7c\xf6\x53"
    "\xd4\xc6\xe4\xda\xd1\xa2\x00\x0e\x3d\x30\xf7\x05\x4f\x1d\x82\xbc\x52\xd9"
    "\xb1\x30\x82\x01\x0a\x30\x82\x01\x06\x02\x56\x00\x84\x12\x4f\xf7\x3b\x65"
    "\x53\x34\x6c\x6c\x4d\x77\xdf\xfd\x1f\xb6\x16\xe2\x25\x15\xca\xc9\xc1\x41"
    "\x9a\x50\xda\xeb\x88\x4f\x3d\xb3\x01\x00\x44\xc4\xac\xe7\x14\x62\xa6\x56"
    "\xde\xc5\xb7\xc3\x1d\x07\xbd\x7d\x64\xc5\x7e\x45\x25\x56\xed\x7a\xd2\x14"
    "\xdb\x4e\x27\xd4\x1f\xf8\x94\xa7\xef\x07\xce\xdb\x24\xb7\xdd\x71\x5c\x63"
    "\xc9\x33\xfe\xde\x40\x52\xeb\x02\x55\x58\x0c\x35\x4f\x7c\xee\x37\x78\x48"
    "\x48\x33\xa5\x3f\xfe\x15\x24\x0f\x41\x6e\x0e\x87\x31\x2b\x81\x11\x8b\x3c"
    "\x9d\x05\x8a\x29\x22\x00\xaa\xd8\x83\x1d\xef\x62\xec\x6e\xe4\x94\x83\xcf"
    "\xd7\x68\xaf\xd3\xa8\xed\xd8\xfe\xd8\xc3\x8f\x48\xfc\x8c\x0d\xe7\x89\x6f"
    "\xe2\xbf\xfb\x0d\xc5\x4a\x05\x34\x92\x18\x7a\x93\xa0\xe8\x42\x86\x22\xa9"
    "\xe9\x80\x37\x47\x02\x55\x60\x76\xab\xde\x2b\xf5\xa2\x2c\xaa\x0c\x99\x81"
    "\xee\x72\x2c\x7d\x22\x59\x2a\x35\xea\x50\x4e\x47\x6b\x92\x2d\x30\xa1\x01"
    "\xa5\x9e\x26\x6e\x27\xca\xf5\xf2\x87\x5d\x31\xaf\xe9\x32\xcd\x10\xfd\x4d"
    "\xdb\xf9\x86\x05\x12\x1b\x01\x84\x55\x97\x5f\xe2\x78\x27\xd9\xe4\x26\x7d"
    "\xab\x0e\xe0\x1b\x6f\xcb\x4b\x14\xdd\xdc\xdc\x8b\xe8\x9f\xd0\x62\x96\xca"
    "\xcf";

static const uint8_t kThreePrimeEncryptedMessage[] = {
    0x58, 0xd9, 0xea, 0x8a, 0xf6, 0x3d, 0xb4, 0xd9, 0xf7, 0xbb, 0x02, 0xc5,
    0x58, 0xd2, 0xa9, 0x46, 0x80, 0x70, 0x70, 0x16, 0x07, 0x64, 0x32, 0x4c,
    0x4e, 0x92, 0x61, 0xb7, 0xff, 0x92, 0xdc, 0xfc, 0xf8, 0xf0, 0x2c, 0x84,
    0x56, 0xbc, 0xe5, 0x93, 0x76, 0xe5, 0xa3, 0x72, 0x98, 0xf2, 0xdf, 0xef,
    0x99, 0x53, 0xf6, 0xd8, 0x4b, 0x09, 0xac, 0xa9, 0xa3, 0xdb, 0x63, 0xa1,
    0xb5, 0x09, 0x8e, 0x40, 0x84, 0x8f, 0x4d, 0xd5, 0x1d, 0xac, 0x6c, 0xaa,
    0x6b, 0x15, 0xe7, 0xb1, 0x0c, 0x67, 0xd2, 0xb2, 0x81, 0x58, 0x30, 0x0e,
    0x18, 0x27, 0xa1, 0x9b, 0x96, 0xad, 0xae, 0x76, 0x1a, 0x32, 0xf7, 0x10,
    0x0b, 0x53, 0x85, 0x31, 0xd6, 0x2a, 0xf6, 0x1c, 0x9f, 0xc2, 0xc7, 0xb1,
    0x05, 0x63, 0x0b, 0xa5, 0x07, 0x1f, 0x1c, 0x01, 0xf0, 0xe0, 0x06, 0xea,
    0x20, 0x69, 0x41, 0x19, 0x57, 0x92, 0x17, 0xf7, 0x0c, 0x5c, 0x66, 0x75,
    0x0e, 0xe5, 0xb3, 0xf1, 0x67, 0x3b, 0x27, 0x47, 0xb2, 0x8e, 0x1c, 0xb6,
    0x3f, 0xdd, 0x76, 0x42, 0x31, 0x13, 0x68, 0x96, 0xdf, 0x3b, 0xd4, 0x87,
    0xd9, 0x16, 0x44, 0x71, 0x52, 0x2e, 0x54, 0x3e, 0x09, 0xcd, 0x71, 0xc1,
    0x1e, 0x5e, 0x96, 0x13, 0xc9, 0x1e, 0xa4, 0xe6, 0xe6, 0x97, 0x2c, 0x6b,
    0xf2, 0xa9, 0x5c, 0xc6, 0x60, 0x2a, 0xbc, 0x82, 0xf8, 0xcb, 0xd4, 0xd7,
    0xea, 0x8a, 0xa1, 0x8a, 0xd9, 0xa5, 0x14, 0x8b, 0x9e, 0xf9, 0x25, 0x02,
    0xd2, 0xab, 0x0c, 0x42, 0xca, 0x2d, 0x45, 0xa3, 0x56, 0x5e, 0xa2, 0x2a,
    0xc8, 0x60, 0xa5, 0x87, 0x5d, 0x85, 0x5c, 0xde, 0xc7, 0xa2, 0x47, 0xc3,
    0x99, 0x29, 0x23, 0x79, 0x36, 0x88, 0xad, 0x40, 0x3e, 0x27, 0x7d, 0xf0,
    0xb6, 0xfa, 0x95, 0x20, 0x3c, 0xec, 0xfc, 0x56, 0x3b, 0x20, 0x91, 0xee,
    0x98, 0x10, 0x2c, 0x82,
};

static const uint8_t kSixPrimeKey[] =
    "\x30\x82\x05\x20\x02\x01\x01\x02\x82\x01\x00\x1c\x04\x39\x44\xb9\xb8\x71"
    "\x1c\x1c\xf7\xdc\x11\x1b\x85\x3b\x2b\xe8\xa6\xeb\xeb\xe9\xb6\x86\x97\x73"
    "\x5d\x75\x46\xd1\x35\x25\xf8\x30\x9a\xc3\x57\x44\x89\xa6\x44\x59\xe3\x3a"
    "\x60\xb5\x33\x84\x72\xa4\x03\xc5\x1a\x20\x98\x70\xbd\xe8\x3b\xc1\x9b\x8a"
    "\x3a\x24\x45\xb6\x6a\x73\xb4\xd0\x6c\x18\xc6\xa7\x94\xd3\x24\x70\xf0\x2d"
    "\x0c\xa5\xb2\x3b\xc5\x33\x90\x9d\x56\x8d\x33\xf6\x93\x7d\xa7\x95\x88\x05"
    "\xdf\xf5\x65\x58\xb9\x5b\xd3\x07\x9c\x16\x8e\x74\xfc\xb8\x76\xaf\x62\x99"
    "\x6c\xd4\xc5\xb3\x69\xe5\x64\xdf\x38\x00\x25\x24\xe9\xb1\x4a\x85\xa6\xf4"
    "\xb6\x23\x68\x67\x4a\x2c\xbd\x9d\x01\x3b\x04\x8c\x70\x94\x82\x76\x45\x0c"
    "\x8b\x95\x8a\x07\x1c\x32\xe7\x09\x97\x3a\xfd\xca\x57\xe9\x57\x0c\xae\x2b"
    "\xa3\x25\xd1\xf2\x0d\x34\xa1\xe6\x2f\x7b\x1b\x36\x53\x83\x95\xb9\x26\x6e"
    "\x4f\x36\x26\xf8\x47\xae\xdf\xe8\x4d\xf6\xb2\xff\x03\x23\x74\xfa\xa5\x6d"
    "\xcb\xcb\x80\x12\xc3\x77\xf0\x19\xb7\xf2\x6b\x19\x5c\xde\x0a\xd7\xee\x8c"
    "\x48\x2f\x50\x24\xa5\x2e\xcc\x2a\xed\xc2\x35\xe0\x3d\x29\x31\x17\xd6\x8f"
    "\x44\xaa\x5b\x33\xbd\xb4\x88\x87\xd9\x29\x3f\x94\xe7\x75\xe3\x02\x01\x03"
    "\x02\x82\x01\x00\x12\xad\x7b\x83\x26\x7a\xf6\x12\xbd\xfa\x92\xb6\x12\x58"
    "\xd2\x1d\x45\xc4\x9d\x47\xf1\x24\x59\xba\x4c\xe8\xf8\xd9\xe0\xce\x19\x50"
    "\x20\x67\x2c\xe4\xd8\x5b\xc4\x2d\x91\x41\xeb\x05\x4f\xf4\xb4\x20\xc7\xbc"
    "\xd6\xe2\x5c\xa0\x27\xcf\xb8\xb3\x3b\x5c\xeb\x5e\x96\xb7\x99\x4b\x8a\xc3"
    "\x70\xaf\x7f\xd8\x5f\xeb\xcb\x1a\x79\x44\x68\x97\x84\xd8\x29\x87\x64\xba"
    "\x18\x2e\x95\x66\x1a\x7d\xd9\x35\x3a\x5c\x92\x7a\x81\x1b\x6c\xa9\xf8\xfa"
    "\x05\x23\x18\x5b\xb2\xf8\x77\x1c\xc5\x1b\x7d\x26\x5f\x48\x69\x1b\xc4\x34"
    "\xef\x6e\xa1\x15\xd2\xb2\xac\xb8\xa8\xed\x1e\xee\xdc\xb5\xb9\x5c\x79\x25"
    "\x48\xbb\xe5\x9d\xd8\xe5\xe2\x94\xdf\xd5\x32\x22\x84\xbf\xc2\xaa\xa4\x54"
    "\xbb\x29\xdb\x13\x4a\x28\x3d\x83\x3a\xff\xa3\xae\x38\x08\xfc\x36\x84\x91"
    "\x30\xd1\xfd\x82\x64\xf1\x0f\xae\xba\xd7\x9a\x43\x58\x03\x5e\x5f\x01\xcb"
    "\x8b\x90\x8d\x77\x34\x6f\x37\x40\xb6\x6d\x22\x23\x90\xb2\xfd\x32\xb5\x96"
    "\x45\xbf\xae\x8c\xc4\x62\x03\x6c\x68\x90\x59\x31\x1a\xcb\xfb\xa4\x0b\x94"
    "\x15\x13\xda\x1a\x8d\xa7\x0b\x34\x62\x93\xea\xbe\x6e\x71\xc2\x1d\xc8\x9d"
    "\xac\x66\xcc\x31\x87\xff\x99\xab\x02\x2c\x00\xa5\x57\x41\x66\x87\x68\x02"
    "\x6a\xdf\x97\xb0\xfe\x6b\x34\xc4\x33\x88\x2b\xce\x82\xaf\x2d\x33\x5a\xad"
    "\x75\x2d\xac\xa5\xd6\x3a\x2d\x65\x43\x68\xfb\x44\x9e\xb8\x25\x05\xed\x97"
    "\x02\x2c\x00\xd2\x77\x34\x24\xac\x60\x9a\xc4\x68\x34\xe5\x6a\xa3\xdc\xe2"
    "\xb0\x58\x5c\x35\x83\x5a\xc7\xa7\xc1\x0b\x7e\x9e\xa5\x85\x32\x47\x93\x22"
    "\xee\xb6\x59\xe9\xe3\x61\x94\xd0\x0e\xcb\x02\x2b\x6e\x3a\x2b\x99\xaf\x9a"
    "\xac\x47\x3f\xba\x75\xfe\xf2\x23\x2d\x77\xb0\x1d\x34\x57\x1f\x73\x77\x91"
    "\xc8\xf8\xc9\x1d\xc3\xe4\x26\xc8\xee\x2c\xf0\xa7\x83\x14\x7a\xc3\x59\x49"
    "\x0f\x02\x2c\x00\x8c\x4f\x78\x18\x72\xeb\x11\xd8\x45\x78\x98\xf1\xc2\x93"
    "\x41\xca\xe5\x92\xce\x57\x91\xda\x6f\xd6\x07\xa9\xbf\x19\x03\x76\xda\x62"
    "\x17\x49\xce\xe6\x9b\xec\xeb\xb8\x8a\xb4\x87\x02\x2c\x00\xa3\xc2\x29\xa6"
    "\xa7\xe1\x3c\xe9\xcf\x0f\x50\x51\x1c\xcc\xc8\x5b\x08\x9c\x97\x24\x3a\x86"
    "\x23\xa8\x0b\xbb\x54\xa6\xb9\x70\x3d\x1d\xd0\x1b\xa3\xac\xd9\xb2\x03\x80"
    "\xd7\x67\xec\x30\x82\x02\x29\x30\x81\x88\x02\x2c\x00\x97\x5d\x3b\xf2\xcc"
    "\xba\xd9\x77\x67\xaa\xd2\x22\xa7\xa3\x49\x08\xc7\xb8\x27\xa1\x59\x4b\xa7"
    "\xa5\xd2\x74\x05\xe7\x5a\x35\xd7\x25\x79\x18\x20\x8a\x25\xec\x3b\x52\xaf"
    "\xcb\xdb\x02\x2b\x64\xe8\xd2\xa1\xdd\xd1\xe6\x4f\x9a\x71\xe1\x6c\x6f\xc2"
    "\x30\xb0\x85\x25\x6f\xc0\xe6\x32\x6f\xc3\xe1\xa2\xae\x9a\x3c\x23\xe4\xc3"
    "\xa6\x10\x15\xb1\x6e\x9d\x7c\xe1\xca\x87\xe7\x02\x2b\x5e\xef\x25\x29\xed"
    "\xf6\x52\x15\xd3\x60\xb6\x88\xcf\x0f\xe2\x24\xa4\x04\x97\x9c\x9d\x58\x13"
    "\xbb\x00\x6d\x39\xf6\xad\x21\x7e\x56\x2c\x2e\x06\x06\xc4\x6d\x44\xac\x79"
    "\x1f\xe5\x30\x81\x89\x02\x2c\x00\xdb\xf1\x78\xf9\xa4\x94\xea\x39\x8a\x3f"
    "\x23\x48\x2a\x23\x8f\xd2\x18\x97\xd2\xdf\x0f\xb8\x2b\x33\xa0\xe8\x8f\xbc"
    "\x4e\x42\xfd\x54\xc7\x0f\xde\xba\x6d\xba\x96\xa7\xce\x67\x3d\x02\x2c\x00"
    "\x92\xa0\xfb\x51\x18\x63\x46\xd1\x06\xd4\xc2\x30\x1c\x17\xb5\x36\xbb\x0f"
    "\xe1\xea\x0a\x7a\xc7\x77\xc0\x9b\x0a\x7d\x89\x81\xfe\x38\x84\xb5\x3f\x26"
    "\xf3\xd1\xb9\xc5\x34\x44\xd3\x02\x2b\x4c\xbd\x1d\x44\xc8\x19\x23\xd8\xb3"
    "\x96\x66\x4b\x62\xcb\x3e\xe6\x6c\x11\xdf\xb2\x92\xd3\xc8\x34\xb9\xa6\x5a"
    "\x2f\x19\xf4\x0b\xb2\xe6\x8e\xa6\xaf\xa3\xae\xa4\xb3\x92\xc4\x79\x30\x81"
    "\x85\x02\x2b\x00\x89\xab\x30\xfc\x7b\x37\x94\x11\x9f\x4d\x31\x3b\xac\x09"
    "\x57\xe6\x64\xec\xa0\xc8\xf8\x04\x1a\xf9\x2a\xa4\x4b\x36\x18\xbb\x5f\xdc"
    "\xcd\xf0\xc8\xcb\x97\xd1\xdf\x13\x12\x3f\x02\x2a\x5b\xc7\x75\xfd\xa7\x7a"
    "\x62\xb6\x6a\x33\x76\x27\xc8\x06\x3a\x99\x98\x9d\xc0\x85\xfa\xad\x67\x50"
    "\xc7\x18\x32\x24\x10\x7c\xea\x93\x33\xf5\xdb\x32\x65\x36\x94\xb7\x61\x7f"
    "\x02\x2a\x16\x6c\x96\xa1\x50\x6f\x3a\x92\xc0\x75\x43\xb5\x6b\x9c\x17\x09"
    "\xd3\xf0\x67\x69\x45\x92\xfb\x7b\x50\xa8\x42\x9b\x33\x92\xab\xd5\xe6\x49"
    "\xb3\x26\x99\x55\x16\x3a\x39\x63\x30\x81\x87\x02\x2b\x00\xc1\x25\x19\x1d"
    "\x6e\x18\xcb\x2d\x64\xe2\xe6\xb6\x1c\xe4\xaa\x9c\xb9\xee\x18\xd4\xf7\x5f"
    "\x66\x40\xf0\xe1\x31\x38\xf2\x53\x00\x8b\xcc\xe4\x0d\xb7\x81\xb4\xe6\x1c"
    "\x19\xaf\x02\x2b\x00\x80\xc3\x66\x13\x9e\xbb\x32\x1e\x43\x41\xef\x24\x13"
    "\x43\x1c\x68\x7b\xf4\x10\x8d\xfa\x3f\x99\x80\xa0\x96\x20\xd0\xa1\x8c\xab"
    "\x07\xdd\xed\x5e\x7a\x56\x78\x99\x68\x11\x1f\x02\x2b\x00\xb0\x59\xea\x67"
    "\x93\x42\xbf\x07\x54\x38\x41\xcb\x73\xa4\x0e\xc2\xae\x56\x19\x41\xc9\x8a"
    "\xb2\x2f\xa8\x0a\xb1\x4e\x12\x39\x2e\xc0\x94\x9a\xc6\xa3\xe4\xaf\x8a\x16"
    "\x06\xb8";

static const uint8_t kSixPrimeEncryptedMessage[] = {
    0x0a, 0xcb, 0x6c, 0x02, 0x9d, 0x1a, 0x7c, 0xf3, 0x4e, 0xff, 0x16, 0x88,
    0xee, 0x22, 0x1d, 0x8d, 0xd2, 0xfd, 0xde, 0x83, 0xb3, 0xd9, 0x35, 0x2c,
    0x82, 0xe0, 0xff, 0xe6, 0x79, 0x6d, 0x06, 0x21, 0x74, 0xa8, 0x04, 0x0c,
    0xe2, 0xd3, 0x98, 0x3f, 0xbf, 0xd0, 0xe9, 0x88, 0x24, 0xe2, 0x05, 0xa4,
    0x45, 0x51, 0x87, 0x6b, 0x1c, 0xef, 0x5f, 0x2d, 0x61, 0xb6, 0xf1, 0x4c,
    0x1f, 0x3d, 0xbf, 0x4b, 0xf2, 0xda, 0x09, 0x97, 0x81, 0xde, 0x91, 0xb7,
    0x0d, 0xb4, 0xc2, 0xab, 0x41, 0x64, 0x9d, 0xd9, 0x39, 0x46, 0x79, 0x66,
    0x43, 0xf1, 0x34, 0x21, 0x56, 0x2f, 0xc6, 0x68, 0x40, 0x4a, 0x2d, 0x73,
    0x96, 0x50, 0xe1, 0xb0, 0xaf, 0x49, 0x39, 0xb4, 0xf0, 0x3a, 0x78, 0x38,
    0x70, 0xa9, 0x91, 0x5d, 0x5e, 0x07, 0xf4, 0xec, 0xbb, 0xc4, 0xe5, 0x8a,
    0xb8, 0x06, 0xba, 0xdf, 0xc6, 0x48, 0x78, 0x4b, 0xca, 0x2a, 0x8a, 0x92,
    0x64, 0xe3, 0xa6, 0xae, 0x87, 0x97, 0x12, 0x16, 0x46, 0x67, 0x59, 0xdf,
    0xf2, 0xf3, 0x89, 0x6f, 0xe8, 0xa9, 0x13, 0x57, 0x63, 0x4e, 0x07, 0x98,
    0xcc, 0x73, 0xa0, 0x84, 0x9d, 0xe8, 0xb3, 0x50, 0x59, 0xb5, 0x51, 0xb3,
    0x41, 0x7d, 0x55, 0xfe, 0xd9, 0xf0, 0xc6, 0xff, 0x6e, 0x96, 0x4f, 0x22,
    0xb2, 0x0d, 0x6b, 0xc9, 0x83, 0x2d, 0x98, 0x98, 0xb2, 0xd1, 0xb7, 0xe4,
    0x50, 0x83, 0x1a, 0xa9, 0x02, 0x9f, 0xaf, 0x54, 0x74, 0x2a, 0x2c, 0x63,
    0x10, 0x79, 0x45, 0x5c, 0x95, 0x0d, 0xa1, 0x9b, 0x55, 0xf3, 0x1e, 0xb7,
    0x56, 0x59, 0xf1, 0x59, 0x8d, 0xd6, 0x15, 0x89, 0xf6, 0xfe, 0xc0, 0x00,
    0xdd, 0x1f, 0x2b, 0xf0, 0xf7, 0x5d, 0x64, 0x84, 0x76, 0xd3, 0xc2, 0x92,
    0x35, 0xac, 0xb5, 0xf9, 0xf6, 0xa8, 0x05, 0x89, 0x4c, 0x95, 0x41, 0x4e,
    0x34, 0x25, 0x11, 0x14,
};

// kEstonianRSAKey is an RSAPublicKey encoded with a negative modulus. See
// https://crbug.com/532048.
static const uint8_t kEstonianRSAKey[] = {
    0x30, 0x82, 0x01, 0x09, 0x02, 0x82, 0x01, 0x00, 0x96, 0xa6, 0x2e, 0x9c,
    0x4e, 0x6a, 0xc3, 0xcc, 0xcd, 0x8f, 0x70, 0xc3, 0x55, 0xbf, 0x5e, 0x9c,
    0xd4, 0xf3, 0x17, 0xc3, 0x97, 0x70, 0xae, 0xdf, 0x12, 0x5c, 0x15, 0x80,
    0x03, 0xef, 0x2b, 0x18, 0x9d, 0x6a, 0xcb, 0x52, 0x22, 0xc1, 0x81, 0xb8,
    0x7e, 0x61, 0xe8, 0x0f, 0x79, 0x24, 0x0f, 0x82, 0x70, 0x24, 0x4e, 0x29,
    0x20, 0x05, 0x54, 0xeb, 0xd4, 0xa9, 0x65, 0x59, 0xb6, 0x3c, 0x75, 0x95,
    0x2f, 0x4c, 0xf6, 0x9d, 0xd1, 0xaf, 0x5f, 0x14, 0x14, 0xe7, 0x25, 0xea,
    0xa5, 0x47, 0x5d, 0xc6, 0x3e, 0x28, 0x8d, 0xdc, 0x54, 0x87, 0x2a, 0x7c,
    0x10, 0xe9, 0xc6, 0x76, 0x2d, 0xe7, 0x79, 0xd8, 0x0e, 0xbb, 0xa9, 0xac,
    0xb5, 0x18, 0x98, 0xd6, 0x47, 0x6e, 0x06, 0x70, 0xbf, 0x9e, 0x82, 0x25,
    0x95, 0x4e, 0xfd, 0x70, 0xd7, 0x73, 0x45, 0x2e, 0xc1, 0x1f, 0x7a, 0x9a,
    0x9d, 0x60, 0xc0, 0x1f, 0x67, 0x06, 0x2a, 0x4e, 0x87, 0x3f, 0x19, 0x88,
    0x69, 0x64, 0x4d, 0x9f, 0x75, 0xf5, 0xd3, 0x1a, 0x41, 0x3d, 0x35, 0x17,
    0xb6, 0xd1, 0x44, 0x0d, 0x25, 0x8b, 0xe7, 0x94, 0x39, 0xb0, 0x7c, 0xaf,
    0x3e, 0x6a, 0xfa, 0x8d, 0x90, 0x21, 0x0f, 0x8a, 0x43, 0x94, 0x37, 0x7c,
    0x2a, 0x15, 0x4c, 0xa0, 0xfa, 0xa9, 0x2f, 0x21, 0xa6, 0x6f, 0x8e, 0x2f,
    0x89, 0xbc, 0xbb, 0x33, 0xf8, 0x31, 0xfc, 0xdf, 0xcd, 0x68, 0x9a, 0xbc,
    0x75, 0x06, 0x95, 0xf1, 0x3d, 0xef, 0xca, 0x76, 0x27, 0xd2, 0xba, 0x8e,
    0x0e, 0x1c, 0x43, 0xd7, 0x70, 0xb9, 0xc6, 0x15, 0xca, 0xd5, 0x4d, 0x87,
    0xb9, 0xd1, 0xae, 0xde, 0x69, 0x73, 0x00, 0x2a, 0x97, 0x51, 0x4b, 0x30,
    0x01, 0xc2, 0x85, 0xd0, 0x05, 0xcc, 0x2e, 0xe8, 0xc7, 0x42, 0xe7, 0x94,
    0x51, 0xe3, 0xf5, 0x19, 0x35, 0xdc, 0x57, 0x96, 0xe7, 0xd9, 0xb4, 0x49,
    0x02, 0x03, 0x01, 0x00, 0x01,
};

// kExponent1RSAKey is an RSAPublicKey encoded with an exponent of 1. See
// https://crbug.com/541257
static const uint8_t kExponent1RSAKey[] = {
    0x30, 0x82, 0x01, 0x08, 0x02, 0x82, 0x01, 0x01, 0x00, 0xcf, 0x86, 0x9a,
    0x7d, 0x5c, 0x9f, 0xbd, 0x33, 0xbb, 0xc2, 0xb1, 0x06, 0xa8, 0x3e, 0xc5,
    0x18, 0xf3, 0x01, 0x04, 0xdd, 0x7a, 0x38, 0x0e, 0x8e, 0x8d, 0x10, 0xaa,
    0xf8, 0x64, 0x49, 0x82, 0xa6, 0x16, 0x9d, 0xd9, 0xae, 0x5e, 0x7f, 0x9b,
    0x53, 0xcb, 0xbb, 0x29, 0xda, 0x98, 0x47, 0x26, 0x88, 0x2e, 0x1d, 0x64,
    0xb3, 0xbc, 0x7e, 0x96, 0x3a, 0xa7, 0xd6, 0x87, 0xf6, 0xf5, 0x3f, 0xa7,
    0x3b, 0xd3, 0xc5, 0xd5, 0x61, 0x3c, 0x63, 0x05, 0xf9, 0xbc, 0x64, 0x1d,
    0x71, 0x65, 0xf5, 0xc8, 0xe8, 0x64, 0x41, 0x35, 0x88, 0x81, 0x6b, 0x2a,
    0x24, 0xbb, 0xdd, 0x9f, 0x75, 0x4f, 0xea, 0x35, 0xe5, 0x32, 0x76, 0x5a,
    0x8b, 0x7a, 0xb5, 0x92, 0x65, 0x34, 0xb7, 0x88, 0x42, 0x5d, 0x41, 0x0b,
    0xd1, 0x00, 0x2d, 0x43, 0x47, 0x55, 0x60, 0x3c, 0x0e, 0x60, 0x04, 0x5c,
    0x88, 0x13, 0xc7, 0x42, 0x55, 0x16, 0x31, 0x32, 0x81, 0xba, 0xde, 0xa9,
    0x56, 0xeb, 0xdb, 0x66, 0x7f, 0x31, 0xba, 0xe8, 0x87, 0x1a, 0xcc, 0xad,
    0x90, 0x86, 0x4b, 0xa7, 0x6d, 0xd5, 0xc1, 0xb7, 0xe7, 0x67, 0x56, 0x41,
    0xf7, 0x03, 0xb3, 0x09, 0x61, 0x63, 0xb5, 0xb0, 0x19, 0x7b, 0xc5, 0x91,
    0xc8, 0x96, 0x5b, 0x6a, 0x80, 0xa1, 0x53, 0x0f, 0x9a, 0x47, 0xb5, 0x9a,
    0x44, 0x53, 0xbd, 0x93, 0xe3, 0xe4, 0xce, 0x0c, 0x17, 0x11, 0x51, 0x1d,
    0xfd, 0x6c, 0x74, 0xe4, 0xec, 0x2a, 0xce, 0x57, 0x27, 0xcc, 0x83, 0x98,
    0x08, 0x32, 0x2c, 0xd5, 0x75, 0xa9, 0x27, 0xfe, 0xaa, 0x5e, 0x48, 0xc9,
    0x46, 0x9a, 0x29, 0x3f, 0xe6, 0x01, 0x4d, 0x97, 0x4a, 0x70, 0xd1, 0x5d,
    0xf8, 0xc0, 0x0b, 0x23, 0xcb, 0xbe, 0xf5, 0x70, 0x0b, 0xc2, 0xf2, 0xc0,
    0x33, 0x9c, 0xc4, 0x8b, 0x39, 0x7e, 0x3d, 0xc6, 0x23, 0x39, 0x9a, 0x98,
    0xdd, 0x02, 0x01, 0x01,
};

struct RSAEncryptParam {
  const uint8_t *der;
  size_t der_len;
  const uint8_t *oaep_ciphertext;
  size_t oaep_ciphertext_len;
} kRSAEncryptParams[] = {
    {kKey1, sizeof(kKey1) - 1, kOAEPCiphertext1, sizeof(kOAEPCiphertext1) - 1},
    {kKey2, sizeof(kKey2) - 1, kOAEPCiphertext2, sizeof(kOAEPCiphertext2) - 1},
    {kKey3, sizeof(kKey3) - 1, kOAEPCiphertext3, sizeof(kOAEPCiphertext3) - 1},
};

class RSAEncryptTest : public testing::TestWithParam<RSAEncryptParam> {};

TEST_P(RSAEncryptTest, TestKey) {
  const auto &param = GetParam();
  bssl::UniquePtr<RSA> key(
      RSA_private_key_from_bytes(param.der, param.der_len));
  ASSERT_TRUE(key);

  EXPECT_TRUE(RSA_check_key(key.get()));

  uint8_t ciphertext[256];

  // Test that PKCS#1 v1.5 encryption round-trips.
  size_t ciphertext_len = 0;
  ASSERT_TRUE(RSA_encrypt(key.get(), &ciphertext_len, ciphertext,
                          sizeof(ciphertext), kPlaintext, kPlaintextLen,
                          RSA_PKCS1_PADDING));
  EXPECT_EQ(RSA_size(key.get()), ciphertext_len);

  uint8_t plaintext[256];
  size_t plaintext_len = 0;
  ASSERT_TRUE(RSA_decrypt(key.get(), &plaintext_len, plaintext,
                          sizeof(plaintext), ciphertext, ciphertext_len,
                          RSA_PKCS1_PADDING));
  EXPECT_EQ(Bytes(kPlaintext, kPlaintextLen), Bytes(plaintext, plaintext_len));

  // Test that OAEP encryption round-trips.
  ciphertext_len = 0;
  ASSERT_TRUE(RSA_encrypt(key.get(), &ciphertext_len, ciphertext,
                          sizeof(ciphertext), kPlaintext, kPlaintextLen,
                          RSA_PKCS1_OAEP_PADDING));
  EXPECT_EQ(RSA_size(key.get()), ciphertext_len);

  plaintext_len = 0;
  ASSERT_TRUE(RSA_decrypt(key.get(), &plaintext_len, plaintext,
                          sizeof(plaintext), ciphertext, ciphertext_len,
                          RSA_PKCS1_OAEP_PADDING));
  EXPECT_EQ(Bytes(kPlaintext, kPlaintextLen), Bytes(plaintext, plaintext_len));

  // |oaep_ciphertext| should decrypt to |kPlaintext|.
  plaintext_len = 0;
  ASSERT_TRUE(RSA_decrypt(key.get(), &plaintext_len, plaintext,
                          sizeof(plaintext), param.oaep_ciphertext,
                          param.oaep_ciphertext_len, RSA_PKCS1_OAEP_PADDING));
  EXPECT_EQ(Bytes(kPlaintext, kPlaintextLen), Bytes(plaintext, plaintext_len));

  // Try decrypting corrupted ciphertexts.
  OPENSSL_memcpy(ciphertext, param.oaep_ciphertext, param.oaep_ciphertext_len);
  for (size_t i = 0; i < param.oaep_ciphertext_len; i++) {
    SCOPED_TRACE(i);
    ciphertext[i] ^= 1;
    EXPECT_FALSE(RSA_decrypt(
        key.get(), &plaintext_len, plaintext, sizeof(plaintext), ciphertext,
        param.oaep_ciphertext_len, RSA_PKCS1_OAEP_PADDING));
    ERR_clear_error();
    ciphertext[i] ^= 1;
  }

  // Test truncated ciphertexts.
  for (size_t len = 0; len < param.oaep_ciphertext_len; len++) {
    SCOPED_TRACE(len);
    EXPECT_FALSE(RSA_decrypt(key.get(), &plaintext_len, plaintext,
                             sizeof(plaintext), ciphertext, len,
                             RSA_PKCS1_OAEP_PADDING));
    ERR_clear_error();
  }
}

INSTANTIATE_TEST_CASE_P(, RSAEncryptTest, testing::ValuesIn(kRSAEncryptParams));

struct RSAMultiPrimeParam {
  const uint8_t *der;
  size_t der_size;
  const uint8_t *enc;
  size_t enc_size;
} kRSAMultiPrimeParams[] = {
    {kTwoPrimeKey, sizeof(kTwoPrimeKey) - 1, kTwoPrimeEncryptedMessage,
     sizeof(kTwoPrimeEncryptedMessage)},
    {kThreePrimeKey, sizeof(kThreePrimeKey) - 1, kThreePrimeEncryptedMessage,
     sizeof(kThreePrimeEncryptedMessage)},
    {kSixPrimeKey, sizeof(kSixPrimeKey) - 1, kSixPrimeEncryptedMessage,
     sizeof(kSixPrimeEncryptedMessage)},
};

class RSAMultiPrimeTest : public testing::TestWithParam<RSAMultiPrimeParam> {};

TEST_P(RSAMultiPrimeTest, TestDecrypt) {
  const auto &param = GetParam();
  bssl::UniquePtr<RSA> rsa(
      RSA_private_key_from_bytes(param.der, param.der_size));
  ASSERT_TRUE(rsa);

  EXPECT_TRUE(RSA_check_key(rsa.get()));

  uint8_t out[256];
  size_t out_len;
  ASSERT_TRUE(RSA_decrypt(rsa.get(), &out_len, out, sizeof(out), param.enc,
                          param.enc_size, RSA_PKCS1_PADDING));
  EXPECT_EQ(Bytes("hello world"), Bytes(out, out_len));
}

INSTANTIATE_TEST_CASE_P(, RSAMultiPrimeTest,
                        testing::ValuesIn(kRSAMultiPrimeParams));

TEST(RSATest, BadKey) {
  bssl::UniquePtr<RSA> key(RSA_new());
  bssl::UniquePtr<BIGNUM> e(BN_new());
  ASSERT_TRUE(key);
  ASSERT_TRUE(e);
  ASSERT_TRUE(BN_set_word(e.get(), RSA_F4));

  // Generate a bad key.
  ASSERT_TRUE(RSA_generate_key_ex(key.get(), 512, e.get(), nullptr));
  ASSERT_TRUE(BN_add(key->p, key->p, BN_value_one()));

  // Bad keys are detected.
  EXPECT_FALSE(RSA_check_key(key.get()));
  EXPECT_FALSE(RSA_check_fips(key.get()));

  // Bad keys may not be parsed.
  uint8_t *der;
  size_t der_len;
  ASSERT_TRUE(RSA_private_key_to_bytes(&der, &der_len, key.get()));
  bssl::UniquePtr<uint8_t> delete_der(der);
  key.reset(RSA_private_key_from_bytes(der, der_len));
  EXPECT_FALSE(key);
}

TEST(RSATest, OnlyDGiven) {
  static const char kN[] =
      "00e77bbf3889d4ef36a9a25d4d69f3f632eb4362214c74517da6d6aeaa9bd09ac42b2662"
      "1cd88f3a6eb013772fc3bf9f83914b6467231c630202c35b3e5808c659";
  static const char kE[] = "010001";
  static const char kD[] =
      "0365db9eb6d73b53b015c40cd8db4de7dd7035c68b5ac1bf786d7a4ee2cea316eaeca21a"
      "73ac365e58713195f2ae9849348525ca855386b6d028e437a9495a01";

  bssl::UniquePtr<RSA> key(RSA_new());
  ASSERT_TRUE(key);
  ASSERT_TRUE(BN_hex2bn(&key->n, kN));
  ASSERT_TRUE(BN_hex2bn(&key->e, kE));
  ASSERT_TRUE(BN_hex2bn(&key->d, kD));

  // Keys with only n, e, and d are functional.
  EXPECT_TRUE(RSA_check_key(key.get()));

  const uint8_t kDummyHash[16] = {0};
  uint8_t buf[64];
  unsigned buf_len = sizeof(buf);
  ASSERT_LE(RSA_size(key.get()), sizeof(buf));
  EXPECT_TRUE(RSA_sign(NID_sha256, kDummyHash, sizeof(kDummyHash), buf,
                       &buf_len, key.get()));
  EXPECT_TRUE(RSA_verify(NID_sha256, kDummyHash, sizeof(kDummyHash), buf,
                         buf_len, key.get()));

  // Keys without the public exponent must continue to work when blinding is
  // disabled to support Java's RSAPrivateKeySpec API. See
  // https://bugs.chromium.org/p/boringssl/issues/detail?id=12.
  bssl::UniquePtr<RSA> key2(RSA_new());
  ASSERT_TRUE(key2);
  ASSERT_TRUE(BN_hex2bn(&key2->n, kN));
  ASSERT_TRUE(BN_hex2bn(&key2->d, kD));
  key2->flags |= RSA_FLAG_NO_BLINDING;

  ASSERT_LE(RSA_size(key2.get()), sizeof(buf));
  EXPECT_TRUE(RSA_sign(NID_sha256, kDummyHash, sizeof(kDummyHash), buf,
                       &buf_len, key2.get()));

  // Verify the signature with |key|. |key2| has no public exponent.
  EXPECT_TRUE(RSA_verify(NID_sha256, kDummyHash, sizeof(kDummyHash), buf,
                         buf_len, key.get()));
}

TEST(RSATest, RecoverCRTParams) {
  bssl::UniquePtr<BIGNUM> e(BN_new());
  ASSERT_TRUE(e);
  ASSERT_TRUE(BN_set_word(e.get(), RSA_F4));

  bssl::UniquePtr<RSA> key1(RSA_new());
  ASSERT_TRUE(key1);
  ASSERT_TRUE(RSA_generate_key_ex(key1.get(), 512, e.get(), nullptr));

  EXPECT_TRUE(RSA_check_key(key1.get()));

  // Create a copy of the key without CRT parameters.
  bssl::UniquePtr<RSA> key2(RSA_new());
  ASSERT_TRUE(key2);
  key2->n = BN_dup(key1->n);
  key2->e = BN_dup(key1->e);
  key2->d = BN_dup(key1->d);
  ASSERT_TRUE(key2->n);
  ASSERT_TRUE(key2->e);
  ASSERT_TRUE(key2->d);

  ASSERT_TRUE(RSA_recover_crt_params(key2.get()));

  // The recovered RSA parameters should work.
  EXPECT_TRUE(RSA_check_key(key2.get()));

  uint8_t buf[128];
  unsigned buf_len = sizeof(buf);
  ASSERT_LE(RSA_size(key2.get()), buf_len);

  const uint8_t kDummyHash[16] = {0};
  EXPECT_TRUE(RSA_sign(NID_sha256, kDummyHash, sizeof(kDummyHash), buf,
                       &buf_len, key2.get()));
  EXPECT_TRUE(RSA_verify(NID_sha256, kDummyHash, sizeof(kDummyHash), buf,
                         buf_len, key2.get()));
}

TEST(RSATest, ASN1) {
  // Test that private keys may be decoded.
  bssl::UniquePtr<RSA> rsa(
      RSA_private_key_from_bytes(kKey1, sizeof(kKey1) - 1));
  ASSERT_TRUE(rsa);

  // Test that the serialization round-trips.
  uint8_t *der;
  size_t der_len;
  ASSERT_TRUE(RSA_private_key_to_bytes(&der, &der_len, rsa.get()));
  bssl::UniquePtr<uint8_t> delete_der(der);
  EXPECT_EQ(Bytes(kKey1, sizeof(kKey1) - 1), Bytes(der, der_len));

  // Test that serializing public keys works.
  ASSERT_TRUE(RSA_public_key_to_bytes(&der, &der_len, rsa.get()));
  delete_der.reset(der);

  // Public keys may be parsed back out.
  rsa.reset(RSA_public_key_from_bytes(der, der_len));
  ASSERT_TRUE(rsa);
  EXPECT_FALSE(rsa->p);
  EXPECT_FALSE(rsa->q);

  // Serializing the result round-trips.
  uint8_t *der2;
  size_t der2_len;
  ASSERT_TRUE(RSA_public_key_to_bytes(&der2, &der2_len, rsa.get()));
  bssl::UniquePtr<uint8_t> delete_der2(der2);
  EXPECT_EQ(Bytes(der, der_len), Bytes(der2, der2_len));

  // Public keys cannot be serialized as private keys.
  int ok = RSA_private_key_to_bytes(&der, &der_len, rsa.get());
  if (ok) {
    OPENSSL_free(der);
  }
  EXPECT_FALSE(ok);
  ERR_clear_error();

  // Public keys with negative moduli are invalid.
  rsa.reset(RSA_public_key_from_bytes(kEstonianRSAKey,
                                      sizeof(kEstonianRSAKey)));
  EXPECT_FALSE(rsa);
  ERR_clear_error();

  // But |RSA_parse_public_key_buggy| will accept it.
  CBS cbs;
  CBS_init(&cbs, kEstonianRSAKey, sizeof(kEstonianRSAKey));
  rsa.reset(RSA_parse_public_key_buggy(&cbs));
  EXPECT_TRUE(rsa);
  EXPECT_EQ(0u, CBS_len(&cbs));
}

TEST(RSATest, BadExponent) {
  bssl::UniquePtr<RSA> rsa(
      RSA_public_key_from_bytes(kExponent1RSAKey, sizeof(kExponent1RSAKey)));
  EXPECT_FALSE(rsa);
  ERR_clear_error();
}

// Attempting to generate an excessively small key should fail.
TEST(RSATest, GenerateSmallKey) {
  bssl::UniquePtr<RSA> rsa(RSA_new());
  ASSERT_TRUE(rsa);
  bssl::UniquePtr<BIGNUM> e(BN_new());
  ASSERT_TRUE(e);
  ASSERT_TRUE(BN_set_word(e.get(), RSA_F4));

  EXPECT_FALSE(RSA_generate_key_ex(rsa.get(), 255, e.get(), nullptr));
  uint32_t err = ERR_get_error();
  EXPECT_EQ(ERR_LIB_RSA, ERR_GET_LIB(err));
  EXPECT_EQ(RSA_R_KEY_SIZE_TOO_SMALL, ERR_GET_REASON(err));
}

// Attempting to generate an funny RSA key length should round down.
TEST(RSATest, RoundKeyLengths) {
  bssl::UniquePtr<BIGNUM> e(BN_new());
  ASSERT_TRUE(e);
  ASSERT_TRUE(BN_set_word(e.get(), RSA_F4));

  bssl::UniquePtr<RSA> rsa(RSA_new());
  ASSERT_TRUE(rsa);
  EXPECT_TRUE(RSA_generate_key_ex(rsa.get(), 1025, e.get(), nullptr));
  EXPECT_EQ(1024u, BN_num_bits(rsa->n));

  rsa.reset(RSA_new());
  ASSERT_TRUE(rsa);
  EXPECT_TRUE(RSA_generate_key_ex(rsa.get(), 1027, e.get(), nullptr));
  EXPECT_EQ(1024u, BN_num_bits(rsa->n));

  rsa.reset(RSA_new());
  ASSERT_TRUE(rsa);
  EXPECT_TRUE(RSA_generate_key_ex(rsa.get(), 1151, e.get(), nullptr));
  EXPECT_EQ(1024u, BN_num_bits(rsa->n));

  rsa.reset(RSA_new());
  ASSERT_TRUE(rsa);
  EXPECT_TRUE(RSA_generate_key_ex(rsa.get(), 1152, e.get(), nullptr));
  EXPECT_EQ(1152u, BN_num_bits(rsa->n));
}

#if !defined(BORINGSSL_SHARED_LIBRARY)
TEST(RSATest, SqrtTwo) {
  bssl::UniquePtr<BIGNUM> sqrt(BN_new()), pow2(BN_new());
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  ASSERT_TRUE(sqrt);
  ASSERT_TRUE(pow2);
  ASSERT_TRUE(ctx);

  size_t bits = kBoringSSLRSASqrtTwoLen * BN_BITS2;
  ASSERT_TRUE(BN_one(pow2.get()));
  ASSERT_TRUE(BN_lshift(pow2.get(), pow2.get(), 2 * bits - 1));

  // Check that sqrt² < pow2.
  ASSERT_TRUE(
      bn_set_words(sqrt.get(), kBoringSSLRSASqrtTwo, kBoringSSLRSASqrtTwoLen));
  ASSERT_TRUE(BN_sqr(sqrt.get(), sqrt.get(), ctx.get()));
  EXPECT_LT(BN_cmp(sqrt.get(), pow2.get()), 0);

  // Check that pow2 < (sqrt + 1)².
  ASSERT_TRUE(
      bn_set_words(sqrt.get(), kBoringSSLRSASqrtTwo, kBoringSSLRSASqrtTwoLen));
  ASSERT_TRUE(BN_add_word(sqrt.get(), 1));
  ASSERT_TRUE(BN_sqr(sqrt.get(), sqrt.get(), ctx.get()));
  EXPECT_LT(BN_cmp(pow2.get(), sqrt.get()), 0);

  // Check the kBoringSSLRSASqrtTwo is sized for a 3072-bit RSA key.
  EXPECT_EQ(3072u / 2u, bits);
}

TEST(RSATest, LessThanWords) {
  // kTestVectors is an array of 256-bit values in sorted order.
  static const BN_ULONG kTestVectors[][256 / BN_BITS2] = {
      {TOBN(0x00000000, 0x00000000), TOBN(0x00000000, 0x00000000),
       TOBN(0x00000000, 0x00000000), TOBN(0x00000000, 0x00000000)},
      {TOBN(0x00000000, 0x00000001), TOBN(0x00000000, 0x00000000),
       TOBN(0x00000000, 0x00000000), TOBN(0x00000000, 0x00000000)},
      {TOBN(0xffffffff, 0xffffffff), TOBN(0x00000000, 0x00000000),
       TOBN(0x00000000, 0x00000000), TOBN(0x00000000, 0x00000000)},
      {TOBN(0xffffffff, 0xffffffff), TOBN(0xffffffff, 0xffffffff),
       TOBN(0x00000000, 0x00000000), TOBN(0x00000000, 0x00000000)},
      {TOBN(0xffffffff, 0xffffffff), TOBN(0xffffffff, 0xffffffff),
       TOBN(0xffffffff, 0xffffffff), TOBN(0x00000000, 0x00000000)},
      {TOBN(0x00000000, 0x00000000), TOBN(0x1d6f60ba, 0x893ba84c),
       TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484)},
      {TOBN(0x00000000, 0x83339915), TOBN(0x1d6f60ba, 0x893ba84c),
       TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484)},
      {TOBN(0xed17ac85, 0x00000000), TOBN(0x1d6f60ba, 0x893ba84c),
       TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484)},
      {TOBN(0xed17ac85, 0x83339915), TOBN(0x1d6f60ba, 0x893ba84c),
       TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484)},
      {TOBN(0xed17ac85, 0xffffffff), TOBN(0x1d6f60ba, 0x893ba84c),
       TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484)},
      {TOBN(0xffffffff, 0x83339915), TOBN(0x1d6f60ba, 0x893ba84c),
       TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484)},
      {TOBN(0xffffffff, 0xffffffff), TOBN(0x1d6f60ba, 0x893ba84c),
       TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484)},
      {TOBN(0x00000000, 0x00000000), TOBN(0x00000000, 0x00000000),
       TOBN(0x00000000, 0x00000000), TOBN(0xffffffff, 0xffffffff)},
      {TOBN(0x00000000, 0x00000000), TOBN(0x00000000, 0x00000000),
       TOBN(0xffffffff, 0xffffffff), TOBN(0xffffffff, 0xffffffff)},
      {TOBN(0x00000000, 0x00000001), TOBN(0x00000000, 0x00000000),
       TOBN(0xffffffff, 0xffffffff), TOBN(0xffffffff, 0xffffffff)},
      {TOBN(0x00000000, 0x00000000), TOBN(0xffffffff, 0xffffffff),
       TOBN(0xffffffff, 0xffffffff), TOBN(0xffffffff, 0xffffffff)},
      {TOBN(0xffffffff, 0xffffffff), TOBN(0xffffffff, 0xffffffff),
       TOBN(0xffffffff, 0xffffffff), TOBN(0xffffffff, 0xffffffff)},
  };

  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kTestVectors); i++) {
    SCOPED_TRACE(i);
    for (size_t j = 0; j < OPENSSL_ARRAY_SIZE(kTestVectors); j++) {
      SCOPED_TRACE(j);
      EXPECT_EQ(i < j ? 1 : 0,
                rsa_less_than_words(kTestVectors[i], kTestVectors[j],
                                    OPENSSL_ARRAY_SIZE(kTestVectors[i])));
    }
  }

  EXPECT_EQ(0, rsa_less_than_words(NULL, NULL, 0));
}

TEST(RSATest, GreaterThanPow2) {
  bssl::UniquePtr<BIGNUM> b(BN_new());
  BN_zero(b.get());
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 0));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 1));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 20));

  ASSERT_TRUE(BN_set_word(b.get(), 1));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 0));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 1));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 20));

  ASSERT_TRUE(BN_set_word(b.get(), 2));
  EXPECT_TRUE(rsa_greater_than_pow2(b.get(), 0));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 1));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 20));

  ASSERT_TRUE(BN_set_word(b.get(), 3));
  EXPECT_TRUE(rsa_greater_than_pow2(b.get(), 0));
  EXPECT_TRUE(rsa_greater_than_pow2(b.get(), 1));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 2));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 20));

  BN_set_negative(b.get(), 1);
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 0));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 1));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 2));
  EXPECT_FALSE(rsa_greater_than_pow2(b.get(), 20));

  // Check all bit lengths mod 64.
  for (int n = 1024; n < 1024 + 64; n++) {
    SCOPED_TRACE(n);
    ASSERT_TRUE(BN_set_word(b.get(), 1));
    ASSERT_TRUE(BN_lshift(b.get(), b.get(), n));
    EXPECT_TRUE(rsa_greater_than_pow2(b.get(), n - 1));
    EXPECT_FALSE(rsa_greater_than_pow2(b.get(), n));
    EXPECT_FALSE(rsa_greater_than_pow2(b.get(), n + 1));

    ASSERT_TRUE(BN_sub_word(b.get(), 1));
    EXPECT_TRUE(rsa_greater_than_pow2(b.get(), n - 1));
    EXPECT_FALSE(rsa_greater_than_pow2(b.get(), n));
    EXPECT_FALSE(rsa_greater_than_pow2(b.get(), n + 1));

    ASSERT_TRUE(BN_add_word(b.get(), 2));
    EXPECT_TRUE(rsa_greater_than_pow2(b.get(), n - 1));
    EXPECT_TRUE(rsa_greater_than_pow2(b.get(), n));
    EXPECT_FALSE(rsa_greater_than_pow2(b.get(), n + 1));
  }
}
#endif  // !BORINGSSL_SHARED_LIBRARY
