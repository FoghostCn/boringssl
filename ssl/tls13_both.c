/* Copyright (c) 2016, Google Inc.
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

#include <string.h>

#include "internal.h"

int tls13_handshake(SSL *ssl) {
  ssl->rwstate = SSL_NOTHING;

  if (ssl->hs->handshake_interrupt & HS_NEED_WRITE) {
    if (!tls13_handshake_write(ssl, ssl->hs->out_message)) {
      ssl->rwstate = SSL_WRITING;
      return 0;
    }
    ssl->hs->handshake_interrupt &= ~HS_NEED_WRITE;
  }
  if (ssl->hs->handshake_interrupt & HS_NEED_FLUSH) {
    return 1;
  }
  if (ssl->hs->handshake_interrupt & HS_NEED_READ) {
    if (!tls13_handshake_read(ssl, ssl->hs->in_message)) {
      ssl->rwstate = SSL_READING;
      return 0;
    }
    ssl->hs->handshake_interrupt &= ~HS_NEED_READ;
  }

  if (ssl->server) {
    result = tls13_server_handshake(ssl);
  } else {
    result = tls13_client_handshake(ssl);
  }

  return result;
}

int assemble_handshake_message(SSL_HS_MESSAGE *out, uint8_t type, uint8_t *data,
                               size_t length) {
  if (out->raw != NULL) {
    OPENSSL_free(out->raw);
    out->raw = NULL;
  }

  out->type = type;
  out->length = length;
  out->raw = OPENSSL_malloc(SSL3_HM_HEADER_LENGTH + out->length);
  if (out->raw == NULL) {
    return -1;
  }
  uint8_t *p = out->raw;
  *(p++) = out->type;
  l2n3(out->length, p);
  out->data = p;
  memcpy(out->data, data, out->length);
  out->offset = 0;

  return 1;
}

int tls13_handshake_read(SSL *ssl, SSL_HS_MESSAGE *msg) {
  if (msg->offset < SSL3_HM_HEADER_LENGTH) {
    if (msg->raw != NULL) {
      OPENSSL_free(msg->raw);
      msg->raw = NULL;
    }

    if (msg->offset == 0) {
      msg->data = OPENSSL_malloc(SSL3_HM_HEADER_LENGTH);
    }

    size_t length = SSL3_HM_HEADER_LENGTH;
    int n = ssl3_read_bytes(ssl, SSL3_RT_HANDSHAKE, &msg->data[msg->offset],
                            length - msg->offset, 0);
    if (n < 0) {
      return -1;
    }

    msg->offset += n;
    if (msg->offset < length) {
      return 0;
    }

    uint8_t *p = msg->data;
    msg->type = *(p++);
    n2l3(p, msg->length);
    msg->raw = OPENSSL_malloc(SSL3_HM_HEADER_LENGTH + msg->length);
    if (msg->raw == NULL) {
      return -1;
    }
    memcpy(msg->raw, msg->data, SSL3_HM_HEADER_LENGTH);
    OPENSSL_free(msg->data);
    msg->data = &msg->raw[SSL3_HM_HEADER_LENGTH];
  }

  size_t length = SSL3_HM_HEADER_LENGTH + msg->length;
  int n = ssl3_read_bytes(ssl, SSL3_RT_HANDSHAKE, &msg->raw[msg->offset],
                          length - msg->offset, 0);
  if (n < 0) {
    return -1;
  }

  msg->offset += n;
  ssl3_update_handshake_hash(ssl, msg->raw, length);
  if (msg->offset < length) {
    return 0;
  }

  if (ssl->msg_callback) {
    ssl->msg_callback(0, ssl->version, SSL3_RT_HANDSHAKE, &msg->raw,
                      length, ssl, ssl->msg_callback_arg);
  }

  msg->offset = 0;
  return 1;
}

int tls13_handshake_write(SSL *ssl, SSL_HS_MESSAGE *msg) {
  size_t length = SSL3_HM_HEADER_LENGTH + msg->length;
  int n = ssl3_write_bytes(ssl, SSL3_RT_HANDSHAKE, &msg->raw[msg->offset],
                           length - msg->offset);
  if (n < 0) {
    return -1;
  }

  msg->offset += n;
  ssl3_update_handshake_hash(ssl, msg->raw, length);
  if (msg->offset < length) {
    return 0;
  }

  if (ssl->msg_callback) {
    ssl->msg_callback(1, ssl->version, SSL3_RT_HANDSHAKE, &msg->raw,
                      length, ssl, ssl->msg_callback_arg);
  }

  msg->offset = 0;
  return 1;
}
