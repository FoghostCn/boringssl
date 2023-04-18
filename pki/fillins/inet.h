#ifndef PKI_FILLINS_INET_H_
#define PKI_FILLINS_INET_H_

#include <openssl/base.h>

#if defined(OPENSSL_WINDOWS)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif  // OPENSSL_WINDOWS

#endif  // PKI_FILLINS_INET_H_
