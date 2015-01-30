/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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

#include <openssl/bio.h>

#include <assert.h>
#include <string.h>

#include <openssl/buf.h>
#include <openssl/err.h>
#include <openssl/mem.h>

const BIO_METHOD* bio_s_bio(void);

struct bio_bio_st {
  BIO *peer; /* If peer != NULL, then peer->ptr is also a bio_bio_st,
              * and its "peer" member points back to us.
              * peer != NULL iff init != 0 in the BIO. */

  /* This is for what we write (i.e. reading uses peer's struct): */
  int closed;    /* valid iff peer != NULL */
  size_t len;    /* 0 if peer == NULL or buf == NULL. */
  size_t offset; /* 0 if len == 0 && zero_copy_write_lock == 0. */
  size_t size;   /* maximum number of bytes that can be written to this BIO. */
  uint8_t *buf;  /* "size" elements (if != NULL).
                    buf may be NULL if buf_externally_owned == 1 &&
                    len == 0 && offset == 0. */
  char buf_externally_owned; /* true iff buf is owned by the caller. */

  char zero_copy_read_lock;  /* true iff a zero copy read operation
                              * is in progress. */
  char zero_copy_write_lock; /* true iff a zero copy write operation
                              * is in progress. */

  size_t request; /* valid iff peer != NULL; 0 if len != 0,
                   * otherwise set by peer to number of bytes
                   * it (unsuccessfully) tried to read,
                   * never more than buffer space (size-len) warrants. */
};

static int bio_new(BIO *bio) {
  struct bio_bio_st *b;

  b = OPENSSL_malloc(sizeof *b);
  if (b == NULL) {
    return 0;
  }
  memset(b, 0, sizeof(struct bio_bio_st));

  b->size = 17 * 1024; /* enough for one TLS record (just a default) */
  bio->ptr = b;
  return 1;
}

static void bio_destroy_pair(BIO *bio) {
  struct bio_bio_st *b = bio->ptr;
  BIO *peer_bio;
  struct bio_bio_st *peer_b;

  if (b == NULL) {
    return;
  }

  peer_bio = b->peer;
  if (peer_bio == NULL) {
    return;
  }

  peer_b = peer_bio->ptr;

  assert(peer_b != NULL);
  assert(peer_b->peer == bio);

  peer_b->peer = NULL;
  peer_bio->init = 0;
  assert(peer_b->buf != NULL || peer_b->buf_externally_owned);
  peer_b->len = 0;
  peer_b->offset = 0;

  b->peer = NULL;
  bio->init = 0;
  assert(b->buf != NULL || b->buf_externally_owned);
  b->len = 0;
  b->offset = 0;
}

static int bio_free(BIO *bio) {
  struct bio_bio_st *b;

  if (bio == NULL) {
    return 0;
  }
  b = bio->ptr;

  assert(b != NULL);

  if (b->peer) {
    bio_destroy_pair(bio);
  }

  if (b->buf != NULL && !b->buf_externally_owned) {
    OPENSSL_free(b->buf);
  }

  OPENSSL_free(b);

  return 1;
}

static size_t bio_zero_copy_get_read_buf(struct bio_bio_st* peer_b,
                                         uint8_t** out_read_buf,
                                         size_t* out_buf_offset) {
  size_t max_available;
  if (peer_b->len > peer_b->size - peer_b->offset) {
    /* Only the first half of the ring buffer can be read. */
    max_available = peer_b->size - peer_b->offset;
  } else {
    max_available = peer_b->len;
  }

  *out_read_buf = peer_b->buf;
  *out_buf_offset = peer_b->offset;
  return max_available;
}

int BIO_zero_copy_get_read_buf(BIO* bio, uint8_t** out_read_buf,
                               size_t* out_buf_offset,
                               size_t* out_available_bytes) {
  struct bio_bio_st* b;
  struct bio_bio_st* peer_b;
  size_t max_available;
  *out_available_bytes = 0;

  BIO_clear_retry_flags(bio);

  if (!bio->init) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf, BIO_R_UNINITIALIZED);
    return 0;
  }

  b = bio->ptr;
  if (bio->method != bio_s_bio() || !b || !b->peer) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf,
                      BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }

  peer_b = b->peer->ptr;
  if (!peer_b || !peer_b->peer || peer_b->peer->ptr != b) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf,
                      BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }

  if (peer_b->zero_copy_read_lock) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf, BIO_R_INVALID_ARGUMENT);
    return 0;
  }

  assert(peer_b->buf != NULL ||
         (peer_b->buf_externally_owned && peer_b->len == 0));

  peer_b->request = 0;  /* Is not used by zero-copy API. */

  if (peer_b->buf == NULL) {
    *out_read_buf = NULL;
    *out_buf_offset = 0;
    return 1;
  }

  max_available =
      bio_zero_copy_get_read_buf(peer_b, out_read_buf, out_buf_offset);

  if (max_available > 0) {
    peer_b->zero_copy_read_lock = 1;
  }

  *out_available_bytes = max_available;
  return 1;
}

int BIO_zero_copy_get_read_buf_done(BIO* bio, size_t bytes_read) {
  struct bio_bio_st* b;
  struct bio_bio_st* peer_b;
  size_t max_available;
  size_t dummy_read_offset;
  uint8_t* dummy_read_buf;

  assert(BIO_get_retry_flags(bio) == 0);

  if (!bio->init) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf_done,
                      BIO_R_UNINITIALIZED);
    return 0;
  }

  b = bio->ptr;

  if (bio->method != bio_s_bio() || !b || !b->peer) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf_done,
                      BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }

  peer_b = b->peer->ptr;
  if (!peer_b || !peer_b->peer || peer_b->peer->ptr != b) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf_done,
                      BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }

  if (!peer_b->zero_copy_read_lock) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf_done,
                      BIO_R_INVALID_ARGUMENT);
    return 0;
  }

  assert(peer_b->buf != NULL);

  max_available =
      bio_zero_copy_get_read_buf(peer_b, &dummy_read_buf, &dummy_read_offset);
  if (bytes_read > max_available) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_read_buf_done,
                      BIO_R_INVALID_ARGUMENT);
    return 0;
  }

  peer_b->len -= bytes_read;
  assert(peer_b->len >= 0);
  assert(peer_b->offset + bytes_read <= peer_b->size);

  /* Move read offset. If zero_copy_write_lock == 1 we must advance the
   * offset even if buffer becomes empty, to make sure
   * write_offset = (offset + len) mod size does not change. */
  if (peer_b->offset + bytes_read == peer_b->size ||
      (!peer_b->zero_copy_write_lock && peer_b->len == 0)) {
    peer_b->offset = 0;
  } else {
    peer_b->offset += bytes_read;
  }

  bio->num_read += bytes_read;
  peer_b->zero_copy_read_lock = 0;
  return 1;
}

static size_t bio_zero_copy_get_write_buf(struct bio_bio_st* b,
                                          uint8_t** out_write_buf,
                                          size_t* out_buf_offset) {
  size_t write_offset;
  size_t max_available;

  assert(b->len <= b->size);

  write_offset = b->offset + b->len;

  if (write_offset >= b->size) {
    /* Only the first half of the ring buffer can be written to. */
    write_offset -= b->size;
    /* write up to the start of the ring buffer. */
    max_available = b->offset - write_offset;
  } else {
    /* write up to the end the buffer. */
    max_available = b->size - write_offset;
  }

  *out_write_buf = b->buf;
  *out_buf_offset = write_offset;
  return max_available;
}

int BIO_zero_copy_get_write_buf(BIO* bio, uint8_t** out_write_buf,
                                size_t* out_buf_offset,
                                size_t* out_available_bytes) {
  struct bio_bio_st* b;
  struct bio_bio_st* peer_b;
  size_t max_available;

  *out_available_bytes = 0;
  BIO_clear_retry_flags(bio);

  if (!bio->init) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf, BIO_R_UNINITIALIZED);
    return 0;
  }

  b = bio->ptr;

  if (bio->method != bio_s_bio() || !b || !b->peer) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf,
                      BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }
  peer_b = b->peer->ptr;
  if (!peer_b || !peer_b->peer || peer_b->peer->ptr != b) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf,
                      BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }

  assert(b->buf != NULL || (b->buf_externally_owned && b->len == 0));

  if (b->zero_copy_write_lock || b->buf == NULL) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf, BIO_R_INVALID_ARGUMENT);
    return 0;
  }

  b->request = 0;
  if (b->closed) {
    /* Bio is already closed. */
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf, BIO_R_BROKEN_PIPE);
    return 0;
  }

  max_available = bio_zero_copy_get_write_buf(b, out_write_buf, out_buf_offset);
  if (max_available > 0) {
    b->zero_copy_write_lock = 1;
  }

  *out_available_bytes = max_available;
  return 1;
}

int BIO_zero_copy_get_write_buf_done(BIO* bio, size_t bytes_written) {
  struct bio_bio_st* b;
  struct bio_bio_st* peer_b;

  size_t rest;
  size_t dummy_write_offset;
  uint8_t* dummy_write_buf;

  if (!bio->init) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf_done,
                      BIO_R_UNINITIALIZED);
    return 0;
  }

  b = bio->ptr;
  if (bio->method != bio_s_bio() || !b || !b->peer) {
    OPENSSL_PUT_ERROR(BIO, BIO_set_external_buf, BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }
  peer_b = b->peer->ptr;
  if (!peer_b || !peer_b->peer || peer_b->peer->ptr != b) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf_done,
                      BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }

  b->request = 0;
  if (b->closed) {
    /* BIO is already closed. */
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf_done, BIO_R_BROKEN_PIPE);
    return 0;
  }

  if (!b->zero_copy_write_lock || !b->buf) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf_done,
                      BIO_R_INVALID_ARGUMENT);
    return 0;
  }

  rest = bio_zero_copy_get_write_buf(b, &dummy_write_buf, &dummy_write_offset);

  if (bytes_written > rest) {
    OPENSSL_PUT_ERROR(BIO, BIO_zero_copy_get_write_buf_done,
                      BIO_R_INVALID_ARGUMENT);
    return 0;
  }

  bio->num_write += bytes_written;
  /* Move write offset. */
  b->len += bytes_written;
  b->zero_copy_write_lock = 0;

  if (b->len == 0) {
    /* Cleanup in case |b->offset| is not 0 when |b->len| is. If a read on the
     * peer emptied the buffer while it was locked for write, it could not
     * rewind offset. The caller didn't write anything, so rewind offset now to
     * uphold the assertion that |b->offset == 0| if |b->len == 0 &&
     * |b->zero_copy_write_lock| == 0. See also comments in BIO_read and
     * BIO_zero_copy_get_read_buf_done. */
    b->offset = 0;
  }
  return 1;
}

static int bio_read(BIO *bio, char *buf, int size_) {
  size_t size = size_;
  size_t rest;
  struct bio_bio_st *b, *peer_b;

  BIO_clear_retry_flags(bio);

  if (!bio->init) {
    return 0;
  }

  b = bio->ptr;
  assert(b != NULL);
  assert(b->peer != NULL);
  peer_b = b->peer->ptr;
  assert(peer_b != NULL);
  assert(peer_b->buf != NULL ||
         (peer_b->buf_externally_owned && peer_b->len == 0));

  peer_b->request = 0; /* will be set in "retry_read" situation */

  if (buf == NULL || size == 0 || peer_b->zero_copy_read_lock) {
    return 0;
  }

  if (peer_b->len == 0) {
    if (peer_b->closed) {
      return 0; /* writer has closed, and no data is left */
    } else {
      BIO_set_retry_read(bio); /* buffer is empty */
      if (size <= peer_b->size) {
        peer_b->request = size;
      } else {
        /* don't ask for more than the peer can
         * deliver in one write */
        peer_b->request = peer_b->size;
      }
      return -1;
    }
  }

  assert(peer_b->buf);

  /* we can read */
  if (peer_b->len < size) {
    size = peer_b->len;
  }

  /* now read "size" bytes */
  rest = size;

  assert(rest > 0);
  /* one or two iterations */
  do {
    size_t chunk;

    assert(rest <= peer_b->len);
    if (peer_b->offset + rest <= peer_b->size) {
      chunk = rest;
    } else {
      /* wrap around ring buffer */
      chunk = peer_b->size - peer_b->offset;
    }
    assert(peer_b->offset + chunk <= peer_b->size);

    memcpy(buf, peer_b->buf + peer_b->offset, chunk);

    peer_b->len -= chunk;
    /* If zero_copy_write_lock == 1 we must advance the offset even if buffer
     * becomes empty, to make sure write_offset = (offset + len) % size
     * does not change. */
    if (peer_b->len || peer_b->zero_copy_write_lock) {
      peer_b->offset += chunk;
      assert(peer_b->offset <= peer_b->size);
      if (peer_b->offset == peer_b->size) {
        peer_b->offset = 0;
      }
      buf += chunk;
    } else {
      /* buffer now empty, no need to advance "buf" */
      assert(chunk == rest);
      peer_b->offset = 0;
    }
    rest -= chunk;
  } while (rest);

  return size;
}

static int bio_write(BIO *bio, const char *buf, int num_) {
  size_t num = num_;
  size_t rest;
  struct bio_bio_st *b;

  BIO_clear_retry_flags(bio);

  if (!bio->init || buf == NULL || num == 0) {
    return 0;
  }

  b = bio->ptr;
  assert(b != NULL);
  assert(b->peer != NULL);
  assert(b->buf != NULL || (b->buf_externally_owned && b->len == 0));

  if (b->zero_copy_write_lock) {
    return 0;
  }

  if (b->closed) {
    /* we already closed */
    b->request = 0;
    OPENSSL_PUT_ERROR(BIO, bio_write, BIO_R_BROKEN_PIPE);
    return -1;
  }

  assert(b->len <= b->size);

  if (b->buf == NULL || b->len == b->size) {
    BIO_set_retry_write(bio); /* buffer is full */
    return -1;
  }

  b->request = 0;

  /* we can write */
  if (num > b->size - b->len) {
    num = b->size - b->len;
  }

  /* now write "num" bytes */
  rest = num;

  assert(rest > 0);
  /* one or two iterations */
  do {
    size_t write_offset;
    size_t chunk;

    assert(b->len + rest <= b->size);

    write_offset = b->offset + b->len;
    if (write_offset >= b->size) {
      write_offset -= b->size;
    }
    /* b->buf[write_offset] is the first byte we can write to. */

    if (write_offset + rest <= b->size) {
      chunk = rest;
    } else {
      /* wrap around ring buffer */
      chunk = b->size - write_offset;
    }

    memcpy(b->buf + write_offset, buf, chunk);

    b->len += chunk;

    assert(b->len <= b->size);

    rest -= chunk;
    buf += chunk;
  } while (rest);

  return num;
}

static int bio_make_pair(BIO* bio1, BIO* bio2,
                         size_t writebuf1_len, uint8_t* ext_writebuf1,
                         size_t writebuf2_len, uint8_t* ext_writebuf2) {
  struct bio_bio_st *b1, *b2;

  assert(bio1 != NULL);
  assert(bio2 != NULL);

  b1 = bio1->ptr;
  b2 = bio2->ptr;

  if (b1->peer != NULL || b2->peer != NULL) {
    OPENSSL_PUT_ERROR(BIO, bio_make_pair, BIO_R_IN_USE);
    return 0;
  }

  assert(b1->buf_externally_owned == 0);
  assert(b2->buf_externally_owned == 0);

  if (b1->buf == NULL) {
    if (writebuf1_len) {
      b1->size = writebuf1_len;
    }
    if (!ext_writebuf1) {
      b1->buf_externally_owned = 0;
      b1->buf = OPENSSL_malloc(b1->size);
      if (b1->buf == NULL) {
        OPENSSL_PUT_ERROR(BIO, bio_make_pair, ERR_R_MALLOC_FAILURE);
        return 0;
      }
    } else {
      b1->buf = ext_writebuf1;
      b1->buf_externally_owned = 1;
    }
    b1->len = 0;
    b1->offset = 0;
  }

  if (b2->buf == NULL) {
    if (writebuf2_len) {
      b2->size = writebuf2_len;
    }
    if (!ext_writebuf2) {
      b2->buf_externally_owned = 0;
      b2->buf = OPENSSL_malloc(b2->size);
      if (b2->buf == NULL) {
        OPENSSL_PUT_ERROR(BIO, bio_make_pair, ERR_R_MALLOC_FAILURE);
        return 0;
      }
    } else {
      b2->buf = ext_writebuf2;
      b2->buf_externally_owned = 1;
    }
    b2->len = 0;
    b2->offset = 0;
  }

  b1->peer = bio2;
  b1->closed = 0;
  b1->request = 0;
  b1->zero_copy_read_lock = 0;
  b1->zero_copy_write_lock = 0;
  b2->peer = bio1;
  b2->closed = 0;
  b2->request = 0;
  b2->zero_copy_read_lock = 0;
  b2->zero_copy_write_lock = 0;

  bio1->init = 1;
  bio2->init = 1;

  return 1;
}

static long bio_ctrl(BIO *bio, int cmd, long num, void *ptr) {
  long ret;
  struct bio_bio_st *b = bio->ptr;

  assert(b != NULL);

  switch (cmd) {
    /* specific CTRL codes */

    case BIO_C_GET_WRITE_BUF_SIZE:
      ret = (long)b->size;
      break;

    case BIO_C_GET_WRITE_GUARANTEE:
      /* How many bytes can the caller feed to the next write
       * without having to keep any? */
      if (b->peer == NULL || b->closed || b->buf == NULL) {
        ret = 0;
      } else {
        ret = (long)b->size - b->len;
      }
      break;

    case BIO_C_GET_READ_REQUEST:
      /* If the peer unsuccessfully tried to read, how many bytes
       * were requested?  (As with BIO_CTRL_PENDING, that number
       * can usually be treated as boolean.) */
      ret = (long)b->request;
      break;

    case BIO_C_RESET_READ_REQUEST:
      /* Reset request.  (Can be useful after read attempts
       * at the other side that are meant to be non-blocking,
       * e.g. when probing SSL_read to see if any data is
       * available.) */
      b->request = 0;
      ret = 1;
      break;

    case BIO_C_SHUTDOWN_WR:
      /* similar to shutdown(..., SHUT_WR) */
      b->closed = 1;
      ret = 1;
      break;

    /* standard CTRL codes follow */

    case BIO_CTRL_GET_CLOSE:
      ret = bio->shutdown;
      break;

    case BIO_CTRL_SET_CLOSE:
      bio->shutdown = (int)num;
      ret = 1;
      break;

    case BIO_CTRL_PENDING:
      if (b->peer != NULL) {
        struct bio_bio_st *peer_b = b->peer->ptr;
        ret = (long)peer_b->len;
      } else {
        ret = 0;
      }
      break;

    case BIO_CTRL_WPENDING:
      ret = 0;
      if (b->buf != NULL) {
        ret = (long)b->len;
      }
      break;

    case BIO_CTRL_FLUSH:
      ret = 1;
      break;

    case BIO_CTRL_EOF: {
      BIO *other_bio = ptr;

      if (other_bio) {
        struct bio_bio_st *other_b = other_bio->ptr;
        assert(other_b != NULL);
        ret = other_b->len == 0 && other_b->closed;
      } else {
        ret = 1;
      }
    } break;

    default:
      ret = 0;
  }
  return ret;
}

static int bio_puts(BIO *bio, const char *str) {
  return bio_write(bio, str, strlen(str));
}

static const BIO_METHOD methods_biop = {
    BIO_TYPE_BIO, "BIO pair",             bio_write, bio_read,
    bio_puts,     NULL /* no bio_gets */, bio_ctrl,  bio_new,
    bio_free,     NULL /* no bio_callback_ctrl */
};

const BIO_METHOD *bio_s_bio(void) { return &methods_biop; }

int BIO_new_bio_pair(BIO** bio1_p, size_t writebuf1,
                     BIO** bio2_p, size_t writebuf2) {
  return BIO_new_bio_pair_external_buf(bio1_p, writebuf1, NULL, bio2_p,
                                       writebuf2, NULL);
}

int BIO_new_bio_pair_external_buf(BIO** out1, size_t ext_writebuf1_len,
                                  uint8_t* ext_writebuf1, BIO** out2,
                                  size_t ext_writebuf2_len,
                                  uint8_t* ext_writebuf2) {
  BIO *bio1 = NULL, *bio2 = NULL;
  int ret = 0;

  /* External buffers must have sizes greater than 0. */
  if ((ext_writebuf1 && !ext_writebuf1_len) ||
      (ext_writebuf2 && !ext_writebuf2_len)) {
    goto err;
  }

  bio1 = BIO_new(bio_s_bio());
  if (bio1 == NULL) {
    goto err;
  }
  bio2 = BIO_new(bio_s_bio());
  if (bio2 == NULL) {
    goto err;
  }

  if (!bio_make_pair(bio1, bio2, ext_writebuf1_len, ext_writebuf1,
                     ext_writebuf2_len, ext_writebuf2)) {
    goto err;
  }
  ret = 1;

err:
  if (ret == 0) {
    if (bio1) {
      BIO_free(bio1);
      bio1 = NULL;
    }
    if (bio2) {
      BIO_free(bio2);
      bio2 = NULL;
    }
  }

  *out1 = bio1;
  *out2 = bio2;
  return ret;
}

int BIO_set_external_buf(BIO* bio, size_t buf_len, uint8_t* buf) {
  struct bio_bio_st* b = bio->ptr;

  if (bio->method != bio_s_bio() || !b || !b->peer || !b->peer->ptr) {
    OPENSSL_PUT_ERROR(BIO, BIO_set_external_buf, BIO_R_UNSUPPORTED_METHOD);
    return 0;
  }

  if (!b->buf_externally_owned || b->len != 0 || b->zero_copy_write_lock ||
      (buf == NULL && buf_len != 0) || (buf != NULL && buf_len != b->size)) {
    OPENSSL_PUT_ERROR(BIO, BIO_set_external_buf, BIO_R_INVALID_ARGUMENT);
    return 0;
  }

  assert(!b->zero_copy_read_lock); /* Follows from |b->len| == 0. */
  assert(b->offset == 0);

  b->buf = buf;

  /* |b->size| is NOT updated to ensure that get_read_request reports the
   * correct number of requested bytes in case a read was attempted on a BIO
   * with a NULL buffer. Thus, |buf_len| is only used for sanity checking. */

  return 1;
}

size_t BIO_ctrl_get_read_request(BIO *bio) {
  return BIO_ctrl(bio, BIO_C_GET_READ_REQUEST, 0, NULL);
}

size_t BIO_ctrl_get_write_guarantee(BIO *bio) {
  return BIO_ctrl(bio, BIO_C_GET_WRITE_GUARANTEE, 0, NULL);
}

int BIO_shutdown_wr(BIO *bio) {
  return BIO_ctrl(bio, BIO_C_SHUTDOWN_WR, 0, NULL);
}
