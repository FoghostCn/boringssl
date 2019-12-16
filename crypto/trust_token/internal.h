/* Copyright (c) 2019, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef OPENSSL_HEADER_TRUST_TOKEN_INTERNAL_H
#define OPENSSL_HEADER_TRUST_TOKEN_INTERNAL_H

#include <openssl/base.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/nid.h>

#include <openssl/trust_token.h>


struct trust_token_client_method_st {
  int (*new_client)(TRUST_TOKEN_CLIENT *ctx);
  void (*free_client)(TRUST_TOKEN_CLIENT *ctx);
  int (*begin_issuance)(TRUST_TOKEN_CLIENT *ctx, uint8_t **out, size_t *out_len,
                        size_t count);
  STACK_OF(TRUST_TOKEN) *
      (*finish_issuance)(TRUST_TOKEN_CLIENT *ctx, uint32_t *out_id,
                         const uint8_t *response, size_t response_len);
  int (*begin_redemption)(TRUST_TOKEN_CLIENT *ctx, uint8_t **out,
                          size_t *out_len, const TRUST_TOKEN *token);
};

struct trust_token_issuer_method_st {
  int (*new_issuer)(TRUST_TOKEN_ISSUER *ctx);
  void (*free_issuer)(TRUST_TOKEN_ISSUER *ctx);
  int (*set_metadata)(TRUST_TOKEN_ISSUER *ctx, uint8_t public_metadata,
                      int private_metadata);
  int (*get_public)(TRUST_TOKEN_ISSUER *ctx, uint8_t **out, size_t *out_len,
                    uint8_t public_metadata);
  int (*issue)(TRUST_TOKEN_ISSUER *ctx, uint8_t **out, size_t *out_len,
               uint8_t *out_tokens_issued, const uint8_t *request,
               size_t request_len, uint8_t max_issuance);
  int (*redeem)(TRUST_TOKEN_ISSUER *ctx, int *result, TRUST_TOKEN **out_token,
                uint8_t *out_public_metadata, int *out_private_metadata,
                const uint8_t *request, size_t request_len);
};

typedef struct trust_token_client_method_st TRUST_TOKEN_CLIENT_METHOD;
typedef struct trust_token_issuer_method_st TRUST_TOKEN_ISSUER_METHOD;

struct trust_token_client_st {
  const TRUST_TOKEN_CLIENT_METHOD *method;
  void *protocol;
  EVP_PKEY *srr_key;
};


struct trust_token_issuer_st {
  const TRUST_TOKEN_ISSUER_METHOD *method;
  void *protocol;
  EVP_PKEY *srr_key;
  const uint8_t *metadata_key;
  size_t metadata_key_len;
};

#endif  // OPENSSL_HEADER_TRUST_TOKEN_INTERNAL_H
