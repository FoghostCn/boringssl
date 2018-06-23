/* Copyright (c) 2018, Google Inc.
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

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <openssl/bytestring.h>
#include <openssl/ssl.h>

#include "../internal.h"
#include "handshake_util.h"
#include "test_config.h"
#include "test_state.h"

using namespace bssl;

namespace {

bool HandbackReady(SSL *ssl, int ret) {
  return ret < 0 && SSL_get_error(ssl, ret) == SSL_ERROR_HANDBACK;
}

bool Handshaker(const TestConfig *config, int rfd, int wfd,
                       Span<const uint8_t> input, int control) {
  UniquePtr<SSL_CTX> ctx = config->SetupCtx(/*old_ctx=*/nullptr);
  if (!ctx) {
    return false;
  }
  UniquePtr<SSL> ssl = config->NewSSL(ctx.get(), nullptr, false, nullptr);

  // Set |O_NONBLOCK| in order to break out of the loop when we hit
  // |SSL_ERROR_WANT_READ|, so that we can send |kControlMsgWantRead| to the
  // proxy.
  if (fcntl(rfd, F_SETFL, O_NONBLOCK) != 0) {
    perror("fcntl");
    return false;
  }
  SSL_set_rfd(ssl.get(), rfd);
  SSL_set_wfd(ssl.get(), wfd);

  CBS cbs, handoff;
  CBS_init(&cbs, input.data(), input.size());
  if (!CBS_get_asn1_element(&cbs, &handoff, CBS_ASN1_SEQUENCE) ||
      !DeserializeContextState(&cbs, ctx.get()) ||
      !SetTestState(ssl.get(), TestState::Deserialize(&cbs, ctx.get())) ||
      !GetTestState(ssl.get()) ||
      !SSL_apply_handoff(ssl.get(), handoff)) {
    fprintf(stderr, "Handoff application failed.\n");
    return false;
  }

  int ret = 0;
  for (;;) {
    ret = CheckIdempotentError(
        "SSL_do_handshake", ssl.get(),
        [&]() -> int { return SSL_do_handshake(ssl.get()); });
    if (SSL_get_error(ssl.get(), ret) == SSL_ERROR_WANT_READ) {
      // Synchronize with the proxy, i.e. don't let the handshake continue until
      // the proxy has sent more data.
      char msg = kControlMsgWantRead;
      if (write(control, &msg, 1) != 1 ||
          read(control, &msg, 1) != 1 ||
          msg != kControlMsgWriteCompleted) {
        fprintf(stderr, "read via proxy failed\n");
        return false;
      }
      continue;
    }
    if (!config->async || !RetryAsync(ssl.get(), ret)) {
      break;
    }
  }
  if (!HandbackReady(ssl.get(), ret)) {
    ERR_print_errors_fp(stderr);
    return false;
  }

  ScopedCBB output;
  CBB handback;
  Array<uint8_t> bytes;
  if (!CBB_init(output.get(), 1024) ||
      !CBB_add_u24_length_prefixed(output.get(), &handback) ||
      !SSL_serialize_handback(ssl.get(), &handback) ||
      !SerializeContextState(ssl->ctx.get(), output.get()) ||
      !GetTestState(ssl.get())->Serialize(output.get()) ||
      !CBBFinishArray(output.get(), &bytes)) {
    fprintf(stderr, "Handback serialisation failed.\n");
    return false;
  }

  char msg = kControlMsgHandback;
  if (write(control, &msg, 1) == -1 ||
      write(control, bytes.data(), bytes.size()) == -1) {
    perror("write");
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  TestConfig initial_config, resume_config, retry_config;
  if (!ParseConfig(argc - 1, argv + 1, &initial_config, &resume_config,
                   &retry_config)) {
    return 2;
  }
  const TestConfig *config = initial_config.handshaker_resume
      ? &resume_config : &initial_config;

  // read() will return the entire message in one go, because it's a datagram
  // socket.
  bssl::UniquePtr<uint8_t> buf((uint8_t *) OPENSSL_malloc(1024*1024));
  int len = read(kFdControl, buf.get(), 1024*1024);
  if (len == -1) {
    perror("read");
    _exit(2);
  }
  Span<uint8_t> handoff(buf.get(), len);
  if (!Handshaker(config, kFdProxyToHandshaker, kFdHandshakerToProxy, handoff,
                  kFdControl)) {
    char msg = kControlMsgError;
    if (write(kFdControl, &msg, 1) != 1) {
      return 3;
    }
    return 1;
  }
  return 0;
}
