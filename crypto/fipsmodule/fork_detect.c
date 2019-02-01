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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // needed for madvise() and MAP_ANONYMOUS on Linux.
#endif

#include <openssl/base.h>

#include "fork_detect.h"

#if defined(OPENSSL_LINUX)
#include <sys/mman.h>
#include <unistd.h>

#include <openssl/type_check.h>

#include "delocate.h"
#include "../internal.h"


#if defined(MADV_WIPEONFORK)
OPENSSL_STATIC_ASSERT(MADV_WIPEONFORK == 18, "MADV_WIPEONFORK is not 18");
#else
#define MADV_WIPEONFORK 18
#endif

DEFINE_STATIC_ONCE(g_fork_detect_once);
DEFINE_STATIC_MUTEX(g_fork_detect_lock);
DEFINE_BSS_GET(char *, g_fork_detect_addr);
DEFINE_BSS_GET(uint64_t, g_fork_generation);

static void init_fork_detect(void) {
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    return;
  }

  void *addr = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED) {
    return;
  }

  if (madvise(addr, (size_t)page_size, MADV_WIPEONFORK) != 0) {
    munmap(addr, (size_t)page_size);
    return;
  }

  *g_fork_detect_addr_bss_get() = addr;
  **g_fork_detect_addr_bss_get() = 1;
  *g_fork_generation_bss_get() = 1;
}

uint64_t crypto_get_fork_generation(void) {
  // |fork| and threads have a complex interaction.
  //
  // In a single-threaded process, the locking below is a no-op. The lack of
  // other threads ensures the entire function is atomic with respect to |fork|.
  //
  // In a multi-threaded process, the locking is necessary to avoid threading
  // race conditions. It is also possible for another thread to call |fork|
  // concurrently with this function. However, the resulting child process may
  // only call async-signal-safe functions before |exec|. As other threaded
  // code, it is fine for this file's state to be inconsistent in the child. The
  // child may not call into BoringSSL, so it will not observe the
  // inconsistency.
  //
  // This extends to the caller's use of the return value. Multi-threaded
  // processes may |fork| while the caller is using the return value, but the
  // resulting inconsistency is unobservable.

  CRYPTO_once(g_fork_detect_once_bss_get(), init_fork_detect);
  char *addr = *g_fork_detect_addr_bss_get();
  if (addr == NULL) {
    // Our kernel is too old to support |MADV_WIPEONFORK|.
    return 0;
  }

  uint64_t *generation_ptr = g_fork_generation_bss_get();
  struct CRYPTO_STATIC_MUTEX *lock = g_fork_detect_lock_bss_get();

  CRYPTO_STATIC_MUTEX_lock_read(lock);
  char fork_ok = *addr;
  CRYPTO_STATIC_MUTEX_unlock_read(lock);
  if (fork_ok) {
    return *generation_ptr;
  }

  CRYPTO_STATIC_MUTEX_lock_write(lock);
  if (!*addr) {
    *addr = 1;
    (*generation_ptr)++;
  }
  CRYPTO_STATIC_MUTEX_unlock_write(lock);
  return *generation_ptr;
}

#else   // !OPENSSL_LINUX

uint64_t crypto_get_fork_generation(void) { return 0; }

#endif  // OPENSSL_LINUX
