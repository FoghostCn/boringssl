/* Copyright (c) 2014, Google Inc.
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

#ifndef OPENSSL_HEADER_RAND_H
#define OPENSSL_HEADER_RAND_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Random number generation. */


/* RAND_bytes writes |len| bytes of random data to |buf| and returns one. */
OPENSSL_EXPORT int RAND_bytes(uint8_t *buf, size_t len);

/* RAND_cleanup frees any resources used by the RNG. This is not safe if other
 * threads might still be calling |RAND_bytes|. */
OPENSSL_EXPORT void RAND_cleanup(void);


/* Obscure functions. */

#if !defined(OPENSSL_WINDOWS)
/* RAND_set_urandom_fd causes the module to use a copy of |fd| for system
 * randomness rather opening /dev/urandom internally. The caller retains
 * ownership of |fd| and is at liberty to close it at any time. This is useful
 * if, due to a sandbox, /dev/urandom isn't available. If used, it must be
 * called before the first call to |RAND_bytes|, and it is mutually exclusive
 * with |RAND_I_promise_not_to_fork|.
 *
 * |RAND_set_urandom_fd| does not buffer any entropy, so it is safe to call
 * |fork| at any time after calling |RAND_set_urandom_fd|. */
OPENSSL_EXPORT void RAND_set_urandom_fd(int fd);

/* RAND_I_promise_not_to_fork enables efficient buffered reading of
 * /dev/urandom. It adds an overhead of a few KB per thread. It must be called
 * before the first call to |RAND_bytes|, and it is mutually exclusive with calls
 * to |RAND_set_urandom_fd|.
 *
 * It has an unusual name because the buffer is unsafe across calls to |fork|.
 * Hence, this function should never be called by libraries. */
OPENSSL_EXPORT void RAND_I_promise_not_to_fork(void);
#endif


/* Deprecated functions */

/* RAND_pseudo_bytes is a wrapper around |RAND_bytes|. */
OPENSSL_EXPORT int RAND_pseudo_bytes(uint8_t *buf, size_t len);

/* RAND_seed does nothing. */
OPENSSL_EXPORT void RAND_seed(const void *buf, int num);

/* RAND_load_file returns a nonnegative number. */
OPENSSL_EXPORT int RAND_load_file(const char *path, long num);

/* RAND_add does nothing. */
OPENSSL_EXPORT void RAND_add(const void *buf, int num, double entropy);

/* RAND_egd returns 255. */
OPENSSL_EXPORT int RAND_egd(const char *);

/* RAND_poll returns one. */
OPENSSL_EXPORT int RAND_poll(void);

/* RAND_status returns one. */
OPENSSL_EXPORT int RAND_status(void);

/* rand_meth_st is typedefed to |RAND_METHOD| in base.h. It isn't used; it
 * exists only to be the return type of |RAND_SSLeay|. It's
 * external so that variables of this type can be initialized. */
struct rand_meth_st {
  void (*seed) (const void *buf, int num);
  int (*bytes) (uint8_t *buf, size_t num);
  void (*cleanup) (void);
  void (*add) (const void *buf, int num, double entropy);
  int (*pseudorand) (uint8_t *buf, size_t num);
  int (*status) (void);
};

/* RAND_SSLeay returns a pointer to a dummy |RAND_METHOD|. */
OPENSSL_EXPORT RAND_METHOD *RAND_SSLeay(void);

/* RAND_set_rand_method does nothing. */
OPENSSL_EXPORT void RAND_set_rand_method(const RAND_METHOD *);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_RAND_H */
