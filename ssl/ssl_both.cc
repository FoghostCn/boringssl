/* Copyright (c) 2017, Google Inc.
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

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <utility>

#include "internal.h"

namespace bssl {

int tls_handshake(SSL_HANDSHAKE *hs, int *out_early_return) {
  SSL *const ssl = hs->ssl;
  for (;;) {
    /* Resolve the operation the handshake was waiting on. */
    switch (hs->wait) {
      case ssl_hs_error:
        OPENSSL_PUT_ERROR(SSL, SSL_R_SSL_HANDSHAKE_FAILURE);
        return -1;

      case ssl_hs_flush: {
        int ret = ssl->method->flush_flight(ssl);
        if (ret <= 0) {
          return ret;
        }
        break;
      }

      case ssl_hs_read_message: {
        int ret = ssl->method->read_message(ssl);
        if (ret <= 0) {
          uint32_t err = ERR_peek_error();
          if ((hs->new_session == NULL || ssl->s3->session_reused) &&
              ERR_GET_LIB(err) == ERR_LIB_SSL &&
              ERR_GET_REASON(err) == SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE) {
            /* Add a dedicated error code to the queue for a handshake_failure alert
             * in response to ClientHello. This matches NSS's client behavior and
             * gives a better error on a (probable) failure to negotiate initial
             * parameters. Note: this error code comes after the original one.
             *
             * See https://crbug.com/446505. */
            OPENSSL_PUT_ERROR(SSL, SSL_R_HANDSHAKE_FAILURE_ON_CLIENT_HELLO);
          }
          return ret;
        }
        break;
      }

      case ssl_hs_read_change_cipher_spec: {
        int ret = ssl->method->read_change_cipher_spec(ssl);
        if (ret <= 0) {
          return ret;
        }
        break;
      }

      case ssl_hs_read_end_of_early_data: {
        if (ssl->s3->hs->can_early_read) {
          /* While we are processing early data, the handshake returns early. */
          *out_early_return = 1;
          return 1;
        }
        hs->wait = ssl_hs_ok;
        break;
      }

      case ssl_hs_x509_lookup:
        ssl->rwstate = SSL_X509_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_channel_id_lookup:
        ssl->rwstate = SSL_CHANNEL_ID_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_private_key_operation:
        ssl->rwstate = SSL_PRIVATE_KEY_OPERATION;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_pending_ticket:
        ssl->rwstate = SSL_PENDING_TICKET;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_certificate_verify:
        ssl->rwstate = SSL_CERTIFICATE_VERIFY;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_early_data_rejected:
        ssl->rwstate = SSL_EARLY_DATA_REJECTED;
        /* Cause |SSL_write| to start failing immediately. */
        hs->can_early_write = 0;
        return -1;

      case ssl_hs_early_return:
        *out_early_return = 1;
        hs->wait = ssl_hs_ok;
        return 1;

      case ssl_hs_ok:
        break;
    }

    /* Run the state machine again. */
    hs->wait = hs->do_tls_handshake(hs);
    if (hs->wait == ssl_hs_error) {
      /* Don't loop around to avoid a stray |SSL_R_SSL_HANDSHAKE_FAILURE| the
       * first time around. */
      return -1;
    }
    if (hs->wait == ssl_hs_ok) {
      /* The handshake has completed. */
      return 1;
    }

    /* Otherwise, loop to the beginning and resolve what was blocking the
     * handshake. */
  }
}

}
