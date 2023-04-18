#ifndef EXPERIMENTAL_USERS_AGL_LIBSLEEVI_FILLINS_PATH_SERVICE_H
#define EXPERIMENTAL_USERS_AGL_LIBSLEEVI_FILLINS_PATH_SERVICE_H
#include <openssl/base.h>

#include <string>

namespace bssl {

namespace fillins {

class OPENSSL_EXPORT FilePath {
 public:
 OPENSSL_EXPORT FilePath();
 OPENSSL_EXPORT FilePath(const std::string& path);

 OPENSSL_EXPORT const std::string& value() const;

 OPENSSL_EXPORT FilePath AppendASCII(const std::string &ascii_path_element) const;

 private:
  std::string path_;
};

enum OPENSSL_EXPORT PathKey {
  DIR_SOURCE_ROOT = 0,
};

class OPENSSL_EXPORT PathService {
 public:
  static void Get(PathKey key, FilePath *out);
};

}  // namespace fillins

}  // namespace bssl

#endif  // EXPERIMENTAL_USERS_AGL_LIBSLEEVI_FILLINS_PATH_SERVICE_H
