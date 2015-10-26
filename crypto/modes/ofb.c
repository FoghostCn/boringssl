/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
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
 * ==================================================================== */

#include <openssl/type_check.h>

#include <assert.h>

#include "internal.h"


OPENSSL_COMPILE_ASSERT((16 % sizeof(size_t)) == 0, bad_size_t_size);

void CRYPTO_ofb128_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const void *key, uint8_t ivec[16], int *num,
                           block128_f block) {
  unsigned int n;

  assert(in && out && key && ivec && num);

  n = *num;

  while (n && len) {
    *(out++) = *(in++) ^ ivec[n];
    --len;
    n = (n + 1) % 16;
  }

#if STRICT_ALIGNMENT
  if (((size_t)in | (size_t)out | (size_t)ivec) % sizeof(size_t) != 0) {
    size_t l = 0;
    while (l < len) {
      if (n == 0) {
        (*block)(ivec, ivec, key);
      }
      out[l] = in[l] ^ ivec[n];
      ++l;
      n = (n + 1) % 16;
    }

    *num = n;
    return;
  }
#endif

  while (len >= 16) {
    (*block)(ivec, ivec, key);
    for (; n < 16; n += sizeof(size_t)) {
      *(size_t *)(out + n) = *(size_t *)(in + n) ^ *(size_t *)(ivec + n);
    }
    len -= 16;
    out += 16;
    in += 16;
    n = 0;
  }
  if (len) {
    (*block)(ivec, ivec, key);
    while (len--) {
      out[n] = in[n] ^ ivec[n];
      ++n;
    }
  }
  *num = n;
}
