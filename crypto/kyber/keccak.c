/* Copyright (c) 2023, Google Inc.
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

#include <openssl/base.h>

#include "../internal.h"
#include "./internal.h"


// keccak_f implements the Keccak-1600 permutation as described at
// https://keccak.team/keccak_specs_summary.html. Each lane is represented as a
// 64-bit value and the 5×5 lanes are stored as an array in row-major order.
static void keccak_f(uint64_t state[25]) {
  static const int kNumRounds = 24;
  for (int round = 0; round < kNumRounds; round++) {
    // θ step
    uint64_t c[5];
    for (int x = 0; x < 5; x++) {
      c[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^
             state[x + 20];
    }

    for (int x = 0; x < 5; x++) {
      const uint64_t d = c[(x + 4) % 5] ^ CRYPTO_rotl_u64(c[(x + 1) % 5], 1);
      for (int y = 0; y < 5; y++) {
        state[y * 5 + x] ^= d;
      }
    }

    // ρ and π steps.
    //
    // These steps involve a mapping of the state matrix. Each input point,
    // (x,y), is rotated and written to the point (y, 2x + 3y). In the Keccak
    // pseudo-code a separate array is used because an in-place operation would
    // overwrite some values that are subsequently needed. However, the mapping
    // forms a trail through 24 of the 25 values so we can do it in place with
    // only a single temporary variable.
    //
    // Start with (1, 0). The value here will be mapped and end up at (0, 2).
    // That value will end up at (2, 1), then (1, 2), and so on. After 24
    // steps, 24 of the 25 values have been hit (as this mapping is injective)
    // and the sequence will repeat. All that remains is to handle the element
    // at (0, 0), but the rotation for that element is zero, and it goes to (0,
    // 0), so we can ignore it.
    int pi_x = 1, pi_y = 0;
    uint64_t prev_value = state[1];
    int pi_rot = 1;
    for (int i = 1; i < 25; i++) {
      const int out_x = pi_y;
      const int out_y = (2 * pi_x + 3 * pi_y) % 5;
      const int index = out_y * 5 + out_x;
      const uint64_t t = state[index];
      state[index] = CRYPTO_rotl_u64(prev_value, pi_rot);
      pi_rot = (pi_rot + i + 1) % 64;
      prev_value = t;
      pi_x = out_x;
      pi_y = out_y;
    }

    // χ step
    for (int y = 0; y < 5; y++) {
      const int row_index = 5 * y;
      const uint64_t orig_x0 = state[row_index];
      const uint64_t orig_x1 = state[row_index + 1];
      state[row_index] ^= ~orig_x1 & state[row_index + 2];
      state[row_index + 1] ^= ~state[row_index + 2] & state[row_index + 3];
      state[row_index + 2] ^= ~state[row_index + 3] & state[row_index + 4];
      state[row_index + 3] ^= ~state[row_index + 4] & orig_x0;
      state[row_index + 4] ^= ~orig_x0 & orig_x1;
    }

    // ι step
    //
    // From https://keccak.team/files/Keccak-reference-3.0.pdf, section
    // 1.2, the round constants are based on the output of a LFSR. Thus, as
    // suggested in the appendix of of
    // https://keccak.team/keccak_specs_summary.html, the values are
    // simply encoded here.
    static const uint64_t kRoundConstants[24] = {
        0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
        0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
        0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
    };

    state[0] ^= kRoundConstants[round];
  }
}

static void keccak_init(struct BORINGSSL_keccak_st *ctx,
                        size_t *out_required_out_len, const uint8_t *in,
                        size_t in_len, enum boringssl_keccak_config_t config) {
  size_t capacity_bytes;
  uint8_t terminator;
  switch (config) {
    case boringssl_sha3_256:
      capacity_bytes = 512 / 8;
      *out_required_out_len = 32;
      terminator = 0x06;
      break;
    case boringssl_sha3_512:
      capacity_bytes = 1024 / 8;
      *out_required_out_len = 64;
      terminator = 0x06;
      break;
    case boringssl_shake128:
      capacity_bytes = 256 / 8;
      *out_required_out_len = 0;
      terminator = 0x1f;
      break;
    case boringssl_shake256:
      capacity_bytes = 512 / 8;
      *out_required_out_len = 0;
      terminator = 0x1f;
      break;
    default:
      abort();
  }

  OPENSSL_memset(ctx, 0, sizeof(*ctx));
  ctx->rate_bytes = 200 - capacity_bytes;

  do {
    size_t todo = in_len;
    if (todo > ctx->rate_bytes) {
      todo = ctx->rate_bytes;
    }
    for (size_t i = 0; i < todo; i++) {
      ctx->state.u8[i] ^= in[i];
    }
    in += todo;
    in_len -= todo;

    if (todo == ctx->rate_bytes) {
      keccak_f(ctx->state.u64);
      todo = 0;
    }
    if (in_len == 0) {
      ctx->state.u8[todo] ^= terminator;
      ctx->state.u8[ctx->rate_bytes - 1] ^= 0x80;
      keccak_f(ctx->state.u64);
    }
  } while (in_len);
}

void BORINGSSL_keccak(uint8_t *out, size_t out_len, const uint8_t *in,
                      size_t in_len, enum boringssl_keccak_config_t config) {
  struct BORINGSSL_keccak_st ctx;
  size_t required_out_len;
  keccak_init(&ctx, &required_out_len, in, in_len, config);
  if (required_out_len != 0 && out_len != required_out_len) {
    abort();
  }
  BORINGSSL_keccak_squeeze(&ctx, out, out_len);
}

void BORINGSSL_keccak_init(struct BORINGSSL_keccak_st *ctx, const uint8_t *in,
                           size_t in_len,
                           enum boringssl_keccak_config_t config) {
  size_t required_out_len;
  keccak_init(ctx, &required_out_len, in, in_len, config);
  if (required_out_len != 0) {
    abort();
  }
}

void BORINGSSL_keccak_squeeze(struct BORINGSSL_keccak_st *ctx, uint8_t *out,
                              size_t out_len) {
  while (out_len) {
    size_t remaining = ctx->rate_bytes - ctx->offset;
    size_t todo = out_len;
    if (todo > remaining) {
      todo = remaining;
    }
    OPENSSL_memcpy(out, &ctx->state.u8[ctx->offset], todo);
    out += todo;
    out_len -= todo;
    ctx->offset += todo;
    if (ctx->offset == ctx->rate_bytes) {
      keccak_f(ctx->state.u64);
      ctx->offset = 0;
    }
  }
}
