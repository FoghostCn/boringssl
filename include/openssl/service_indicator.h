/* Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_SERVICE_INDICATOR_H
#define OPENSSL_HEADER_SERVICE_INDICATOR_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

// FIPS_service_indicator_before_call and |FIPS_service_indicator_after_call|
// both currently return the same local thread counter which is slowly
// incremented whenever approved services are called. The
// |CALL_SERVICE_AND_CHECK_APPROVED| macro is strongly recommended over calling
// these functions directly.
//
// |FIPS_service_indicator_before_call| is intended to be called immediately
// before an approved service, while |FIPS_service_indicator_after_call| should
// be called immediately after. If the values returned from these two functions
// are not equal, this means that the service called inbetween is deemed to be
// approved. If the values are still the same, this means the counter has not
// been incremented, and the service called is not approved for FIPS.
//
// In non-FIPS builds, |FIPS_service_indicator_before_call| always returns zero
// and |FIPS_service_indicator_after_call| always returns one. Thus calls always
// appear to be approved. This is intended to simplify testing.
OPENSSL_EXPORT uint64_t FIPS_service_indicator_before_call(void);
OPENSSL_EXPORT uint64_t FIPS_service_indicator_after_call(void);

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)

extern "C++" {

enum {
  BORINGSSL_FIPS_NOT_APPROVED = 0,
  BORINGSSL_FIPS_APPROVED = 1,
};

// CALL_SERVICE_AND_CHECK_APPROVED runs |func| and sets |approved| to one of the
// |BORINGSSL_FIPS_*| values, above, depending on whether |func| invoked an
// approved service. The result of |func| becomes the result of this macro.
#define CALL_SERVICE_AND_CHECK_APPROVED(approved, func)         \
  [&]() {                                                       \
    bssl::FIPSIndicatorHelper fips_indicator_helper(&approved); \
    return func;                                                \
  }()

namespace bssl {

// FIPSIndicatorHelper records whether the service indicator counter advanced
// during its lifetime.
class FIPSIndicatorHelper {
 public:
  FIPSIndicatorHelper(int *result)
      : result_(result), before_(FIPS_service_indicator_before_call()) {
    *result_ = BORINGSSL_FIPS_NOT_APPROVED;
  }

  ~FIPSIndicatorHelper() {
    uint64_t after = FIPS_service_indicator_after_call();
    if (after != before_) {
      *result_ = BORINGSSL_FIPS_APPROVED;
    }
  }

  FIPSIndicatorHelper(const FIPSIndicatorHelper&) = delete;
  FIPSIndicatorHelper &operator=(const FIPSIndicatorHelper &) = delete;

 private:
  int *const result_;
  const uint64_t before_;
};

}  // namespace bssl
}  // extern "C++"

#endif  // !BORINGSSL_NO_CXX
#endif  // __cplusplus

#endif  // OPENSSL_HEADER_SERVICE_INDICATOR_H
