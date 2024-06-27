/* DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005. */
/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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
 * Hudson (tjh@cryptsoft.com).
 *
 */
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

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <openssl/bytestring.h>
#include <openssl/err.h>

#include "internal.h"
#include "../crypto/internal.h"


BSSL_NAMESPACE_BEGIN

// dtls1_bitmap_should_discard returns one if |seq_num| has been seen in
// |bitmap| or is stale. Otherwise it returns zero.
static bool dtls1_bitmap_should_discard(DTLS1_BITMAP *bitmap,
                                        uint64_t seq_num) {
  const size_t kWindowSize = bitmap->map.size();

  if (seq_num > bitmap->max_seq_num) {
    return false;
  }
  uint64_t idx = bitmap->max_seq_num - seq_num;
  return idx >= kWindowSize || bitmap->map[idx];
}

// dtls1_bitmap_record updates |bitmap| to record receipt of sequence number
// |seq_num|. It slides the window forward if needed. It is an error to call
// this function on a stale sequence number.
static void dtls1_bitmap_record(DTLS1_BITMAP *bitmap, uint64_t seq_num) {
  const size_t kWindowSize = bitmap->map.size();

  // Shift the window if necessary.
  if (seq_num > bitmap->max_seq_num) {
    uint64_t shift = seq_num - bitmap->max_seq_num;
    if (shift >= kWindowSize) {
      bitmap->map.reset();
    } else {
      bitmap->map <<= shift;
    }
    bitmap->max_seq_num = seq_num;
  }

  uint64_t idx = bitmap->max_seq_num - seq_num;
  if (idx < kWindowSize) {
    bitmap->map[idx] = true;
  }
}

// reconstruct_epoch finds the largest epoch that ends with the epoch bits from
// |wire_epoch| that is less than or equal to |current_epoch|, to match the
// epoch reconstruction algorithm described in RFC 9147 section 4.2.2.
static uint16_t reconstruct_epoch(uint8_t wire_epoch, uint16_t current_epoch) {
  uint16_t current_epoch_high = current_epoch & 0xfffc;
  uint16_t epoch = (wire_epoch & 0x3) | current_epoch_high;
  if (epoch > current_epoch && current_epoch_high > 0) {
    epoch -= 0x4;
  }
  return epoch;
}

// reconstruct_seqnum returns the smallest sequence number greater than
// |max_seqnum| that ends with |wire_seq & seq_mask|.
static uint64_t reconstruct_seqnum(uint16_t wire_seq, uint64_t seq_mask,
                                   uint64_t max_seqnum) {
  uint64_t seqnum = uint64_t{wire_seq & seq_mask} | (max_seqnum & ~seq_mask);
  if (seqnum < max_seqnum) {
    seqnum += seq_mask + 1;
  }
  return seqnum;
}

enum ssl_open_record_t dtls_open_record(SSL *ssl, uint8_t *out_type,
                                        Span<uint8_t> *out,
                                        size_t *out_consumed,
                                        uint8_t *out_alert, Span<uint8_t> in) {
  *out_consumed = 0;
  if (ssl->s3->read_shutdown == ssl_shutdown_close_notify) {
    return ssl_open_record_close_notify;
  }

  if (in.empty()) {
    return ssl_open_record_partial;
  }

  CBS cbs = CBS(in);

  uint8_t type;
  size_t record_header_len;
  if (!CBS_get_u8(&cbs, &type)) {
    // The record header was incomplete or malformed. Drop the entire packet.
    *out_consumed = in.size();
    return ssl_open_record_discard;
  }
  SSLAEADContext *aead = ssl->s3->aead_read_ctx.get();
  uint64_t sequence;
  uint16_t epoch;
  uint16_t version;
  CBS body;
  // Decode the record header. If the 3 high bits of the type are 001, then the
  // record header is the DTLS 1.3 format. The DTLS 1.3 format should only be
  // used for encrypted records with DTLS 1.3. Plaintext records or DTLS 1.2
  // records use the old record header format.
  if ((type & 0xe0) == 0x20 && !aead->is_null_cipher() &&
      aead->ProtocolVersion() >= TLS1_3_VERSION) {
    // TODO(crbug.com/boringssl/715): Decrypt the sequence number before
    // decoding it.
    if ((type & 0x10) == 0x10) {
      // Connection ID bit set, which we didn't negotiate. Drop the packet.
      *out_consumed = in.size();
      return ssl_open_record_discard;
    }
    // TODO(crbug.com/boringssl/715): Add a runner test that performs many
    // key updates to verify epoch reconstruction works for epochs larger than
    // 3.
    epoch = reconstruct_epoch(type, ssl->d1->r_epoch);
    if ((type & 0x08) == 0x08) {
      // 16-bit sequence number.
      uint16_t seq;
      if (!CBS_get_u16(&cbs, &seq)) {
        // The record header was incomplete or malformed. Drop the entire
        // packet.
        *out_consumed = in.size();
        return ssl_open_record_discard;
      }
      sequence = reconstruct_seqnum(seq, 0xffff, ssl->d1->bitmap.max_seq_num);
    } else {
      // 8-bit sequence number.
      uint8_t seq;
      if (!CBS_get_u8(&cbs, &seq)) {
        // The record header was incomplete or malformed. Drop the entire
        // packet.
        *out_consumed = in.size();
        return ssl_open_record_discard;
      }
      sequence = reconstruct_seqnum(seq, 0xff, ssl->d1->bitmap.max_seq_num);
    }
    record_header_len = in.size() - CBS_len(&cbs);
    if ((type & 0x04) == 0x04) {
      record_header_len += 2;
      // 16-bit length present
      if (!CBS_get_u16_length_prefixed(&cbs, &body)) {
        // The record header was incomplete or malformed. Drop the entire
        // packet.
        *out_consumed = in.size();
        return ssl_open_record_discard;
      }
    } else {
      // No length present - the remaining contents are the whole packet.
      // CBS_get_bytes is used here to advance |cbs| to the end so that future
      // code that computes the number of consumed bytes functions correctly.
      if (!CBS_get_bytes(&cbs, &body, CBS_len(&cbs))) {
        *out_consumed = in.size();
        return ssl_open_record_discard;
      }
    }
    // Epoch/sequence number code expects the epoch to be in the upper 2 bytes
    // of the sequence, so even though that's not the wire encoding, set it that
    // way so that things work.
  } else {
    // TODO(crbug.com/boringssl/715): Discard the packet if we have the old
    // record header and we're handling an encrypted DTLS 1.3 record. Also add
    // a runner test where the runner sends a packet with the wrong record
    // header format.
    uint8_t sequence_bytes[8];
    if (!CBS_get_u16(&cbs, &version) ||
        !CBS_copy_bytes(&cbs, sequence_bytes, sizeof(sequence_bytes))) {
      // The record header was incomplete or malformed. Drop the entire packet.
      *out_consumed = in.size();
      return ssl_open_record_discard;
    }
    record_header_len = in.size() - CBS_len(&cbs) + 2;
    if (!CBS_get_u16_length_prefixed(&cbs, &body) ||
        CBS_len(&body) > SSL3_RT_MAX_ENCRYPTED_LENGTH) {
      // The record header was incomplete or malformed. Drop the entire packet.
      *out_consumed = in.size();
      return ssl_open_record_discard;
    }

    bool version_ok;
    if (aead->is_null_cipher()) {
      // Only check the first byte. Enforcing beyond that can prevent decoding
      // version negotiation failure alerts.
      version_ok = (version >> 8) == DTLS1_VERSION_MAJOR;
    } else {
      version_ok = version == aead->RecordVersion();
    }

    if (!version_ok) {
      // The record header was incomplete or malformed. Drop the entire packet.
      *out_consumed = in.size();
      return ssl_open_record_discard;
    }

    sequence = CRYPTO_load_u64_be(sequence_bytes);
    epoch = static_cast<uint16_t>(sequence >> 48);
  }

  Span<const uint8_t> header = in.subspan(0, record_header_len);
  ssl_do_msg_callback(ssl, 0 /* read */, SSL3_RT_HEADER, header);

  if (epoch != ssl->d1->r_epoch ||
      dtls1_bitmap_should_discard(&ssl->d1->bitmap, sequence)) {
    // Drop this record. It's from the wrong epoch or is a replay. Note that if
    // |epoch| is the next epoch, the record could be buffered for later. For
    // simplicity, drop it and expect retransmit to handle it later; DTLS must
    // handle packet loss anyway.
    *out_consumed = in.size() - CBS_len(&cbs);
    return ssl_open_record_discard;
  }

  // discard the body in-place.
  if (!aead->Open(
          out, type, version, sequence, header,
          MakeSpan(const_cast<uint8_t *>(CBS_data(&body)), CBS_len(&body)))) {
    // Bad packets are silently dropped in DTLS. See section 4.2.1 of RFC 6347.
    // Clear the error queue of any errors decryption may have added. Drop the
    // entire packet as it must not have come from the peer.
    //
    // TODO(davidben): This doesn't distinguish malloc failures from encryption
    // failures.
    ERR_clear_error();
    *out_consumed = in.size() - CBS_len(&cbs);
    return ssl_open_record_discard;
  }
  *out_consumed = in.size() - CBS_len(&cbs);

  // DTLS 1.3 hides the record type inside the encrypted data.
  bool has_padding =
      !aead->is_null_cipher() && aead->ProtocolVersion() >= TLS1_3_VERSION;
  // Check the plaintext length.
  size_t plaintext_limit = SSL3_RT_MAX_PLAIN_LENGTH + (has_padding ? 1 : 0);
  if (out->size() > plaintext_limit) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DATA_LENGTH_TOO_LONG);
    *out_alert = SSL_AD_RECORD_OVERFLOW;
    return ssl_open_record_error;
  }

  if (has_padding) {
    do {
      if (out->empty()) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
        *out_alert = SSL_AD_DECRYPT_ERROR;
        return ssl_open_record_error;
      }
      type = out->back();
      *out = out->subspan(0, out->size() - 1);
    } while (type == 0);
  }

  dtls1_bitmap_record(&ssl->d1->bitmap, sequence);

  // TODO(davidben): Limit the number of empty records as in TLS? This is only
  // useful if we also limit discarded packets.

  if (type == SSL3_RT_ALERT) {
    return ssl_process_alert(ssl, out_alert, *out);
  }

  ssl->s3->warning_alert_count = 0;

  *out_type = type;
  return ssl_open_record_success;
}

static SSLAEADContext *get_write_aead(const SSL *ssl, uint16_t epoch) {
  if (epoch == 0) {
    return ssl->d1->initial_aead_write_ctx.get();
  }

  if (epoch < ssl->d1->w_epoch) {
    assert(epoch + 1 == ssl->d1->w_epoch);
    return ssl->d1->last_aead_write_ctx.get();
  }

  assert(epoch == ssl->d1->w_epoch);
  return ssl->s3->aead_write_ctx.get();
}

static bool use_dtls13_record_header(const SSL *ssl, uint16_t epoch) {
  // Plaintext records in DTLS 1.3 also use the DTLSPlaintext structure for
  // backwards compatibility.
  return ssl->s3->have_version && ssl_protocol_version(ssl) > TLS1_2_VERSION &&
         epoch > 0;
}

size_t dtls_record_header_write_len(const SSL *ssl, uint16_t epoch) {
  if (!use_dtls13_record_header(ssl, epoch)) {
    // 13 is the value of the former DTLS1_RT_HEADER_LENGTH constant.
    return 13;
  }
  // The DTLS 1.3 has a variable length record header. We never send Connection
  // ID, we always send 16-bit sequence numbers, and we send a length. (Length
  // can be omitted, but only for the last record of a packet. Since we send
  // multiple records in one packet, it's easier to implement always sending the
  // length.)
  return 5;
}

size_t dtls_max_seal_overhead(const SSL *ssl,
                              uint16_t epoch) {
  size_t ret = dtls_record_header_write_len(ssl, epoch) +
               get_write_aead(ssl, epoch)->MaxOverhead();
  if (use_dtls13_record_header(ssl, epoch)) {
    // Add 1 byte for the encrypted record type.
    ret++;
  }
  return ret;
}

size_t dtls_seal_prefix_len(const SSL *ssl, uint16_t epoch) {
  return dtls_record_header_write_len(ssl, epoch) +
         get_write_aead(ssl, epoch)->ExplicitNonceLen();
}

bool dtls_seal_record(SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out,
                      uint8_t type, const uint8_t *in, size_t in_len,
                      uint16_t epoch) {
  const size_t prefix = dtls_seal_prefix_len(ssl, epoch);
  if (buffers_alias(in, in_len, out, max_out) &&
      (max_out < prefix || out + prefix != in)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_OUTPUT_ALIASES_INPUT);
    return false;
  }

  // Determine the parameters for the current epoch.
  SSLAEADContext *aead = get_write_aead(ssl, epoch);
  uint64_t *seq = &ssl->s3->write_sequence;
  if (epoch < ssl->d1->w_epoch) {
    seq = &ssl->d1->last_write_sequence;
  }
  // TODO(crbug.com/boringssl/715): If epoch is initial or handshake, the value
  // of seq is probably wrong for a retransmission.

  const size_t record_header_len = dtls_record_header_write_len(ssl, epoch);

  // Ensure the sequence number update does not overflow.
  const uint64_t kMaxSequenceNumber = (uint64_t{1} << 48) - 1;
  if (*seq + 1 > kMaxSequenceNumber) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return false;
  }

  uint16_t record_version = ssl->s3->aead_write_ctx->RecordVersion();
  uint64_t seq_with_epoch = (uint64_t{epoch} << 48) | *seq;

  bool dtls13_header = use_dtls13_record_header(ssl, epoch);
  uint8_t *extra_in = NULL;
  size_t extra_in_len = 0;
  if (dtls13_header) {
    extra_in = &type;
    extra_in_len = 1;
  }

  size_t ciphertext_len;
  if (!aead->CiphertextLen(&ciphertext_len, in_len, extra_in_len)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RECORD_TOO_LARGE);
    return false;
  }
  if (max_out < record_header_len + ciphertext_len) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BUFFER_TOO_SMALL);
    return false;
  }

  if (dtls13_header) {
    // The first byte of the DTLS 1.3 record header has the following format:
    // 0 1 2 3 4 5 6 7
    // +-+-+-+-+-+-+-+-+
    // |0|0|1|C|S|L|E E|
    // +-+-+-+-+-+-+-+-+
    //
    // We set C=0 (no Connection ID), S=1 (16-bit sequence number), L=1 (length
    // is present), which is a mask of 0x2c. The E E bits are the low-order two
    // bits of the epoch.
    //
    // +-+-+-+-+-+-+-+-+
    // |0|0|1|0|1|1|E E|
    // +-+-+-+-+-+-+-+-+
    out[0] = 0x2c | (epoch & 0x3);
    out[1] = *seq >> 8;
    out[2] = *seq & 0xff;
    out[3] = ciphertext_len >> 8;
    out[4] = ciphertext_len & 0xff;
    // DTLS 1.3 uses the sequence number without the epoch for the AEAD.
    seq_with_epoch = *seq;
  } else {
    out[0] = type;
    out[1] = record_version >> 8;
    out[2] = record_version & 0xff;
    CRYPTO_store_u64_be(&out[3], seq_with_epoch);
    out[11] = ciphertext_len >> 8;
    out[12] = ciphertext_len & 0xff;
  }
  Span<const uint8_t> header = MakeConstSpan(out, record_header_len);


  if (!aead->SealScatter(out + record_header_len, out + prefix,
                         out + prefix + in_len, type, record_version,
                         seq_with_epoch, header, in, in_len, extra_in,
                         extra_in_len)) {
    return false;
  }

  // TODO(crbug.com/boringssl/715): Perform record number encryption (RFC 9147
  // section 4.2.3).

  (*seq)++;
  *out_len = record_header_len + ciphertext_len;
  ssl_do_msg_callback(ssl, 1 /* write */, SSL3_RT_HEADER, header);
  return true;
}

BSSL_NAMESPACE_END
