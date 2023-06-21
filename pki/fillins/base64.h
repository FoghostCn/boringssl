#ifndef BSSL_FILLINS_BASE64_H
#define BSSL_FILLINS_BASE64_H

#include <openssl/base.h>

#include <string>
#include <string_view>
#include <vector>

namespace bssl {

namespace fillins {

OPENSSL_EXPORT bool Base64Encode(const std::string_view &input,
                                 std::string *output);

OPENSSL_EXPORT bool Base64Decode(const std::string_view &input,
                                 std::string *output);

}  // namespace fillins

}  // namespace bssl

#endif  // BSSL_FILLINS_BASE64_H
