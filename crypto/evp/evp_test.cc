/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2015 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 */

#include <openssl/evp.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

OPENSSL_MSVC_PRAGMA(warning(push))
OPENSSL_MSVC_PRAGMA(warning(disable: 4702))

#include <map>
#include <string>
#include <utility>
#include <vector>

OPENSSL_MSVC_PRAGMA(warning(pop))

#include <gtest/gtest.h>

#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/rsa.h>

#include "../test/file_test.h"
#include "../test/test_util.h"


// evp_test dispatches between multiple test types. PrivateKey tests take a key
// name parameter and single block, decode it as a PEM private key, and save it
// under that key name. Decrypt, Sign, and Verify tests take a previously
// imported key name as parameter and test their respective operations.

static const EVP_MD *GetDigest(FileTest *t, const std::string &name) {
  if (name == "MD5") {
    return EVP_md5();
  } else if (name == "SHA1") {
    return EVP_sha1();
  } else if (name == "SHA224") {
    return EVP_sha224();
  } else if (name == "SHA256") {
    return EVP_sha256();
  } else if (name == "SHA384") {
    return EVP_sha384();
  } else if (name == "SHA512") {
    return EVP_sha512();
  }
  ADD_FAILURE() << "Unknown digest: " << name;
  return nullptr;
}

static int GetKeyType(FileTest *t, const std::string &name) {
  if (name == "RSA") {
    return EVP_PKEY_RSA;
  }
  if (name == "EC") {
    return EVP_PKEY_EC;
  }
  if (name == "DSA") {
    return EVP_PKEY_DSA;
  }
  if (name == "Ed25519") {
    return EVP_PKEY_ED25519;
  }
  ADD_FAILURE() << "Unknown key type: " << name;
  return EVP_PKEY_NONE;
}

static int GetRSAPadding(FileTest *t, int *out, const std::string &name) {
  if (name == "PKCS1") {
    *out = RSA_PKCS1_PADDING;
    return true;
  }
  if (name == "PSS") {
    *out = RSA_PKCS1_PSS_PADDING;
    return true;
  }
  if (name == "OAEP") {
    *out = RSA_PKCS1_OAEP_PADDING;
    return true;
  }
  ADD_FAILURE() << "Unknown RSA padding mode: " << name;
  return false;
}

using KeyMap = std::map<std::string, bssl::UniquePtr<EVP_PKEY>>;

static bool ImportKey(FileTest *t, KeyMap *key_map,
                      EVP_PKEY *(*parse_func)(CBS *cbs),
                      int (*marshal_func)(CBB *cbb, const EVP_PKEY *key)) {
  std::vector<uint8_t> input;
  if (!t->GetBytes(&input, "Input")) {
    return false;
  }

  CBS cbs;
  CBS_init(&cbs, input.data(), input.size());
  bssl::UniquePtr<EVP_PKEY> pkey(parse_func(&cbs));
  if (!pkey) {
    return false;
  }

  std::string key_type;
  if (!t->GetAttribute(&key_type, "Type")) {
    return false;
  }
  EXPECT_EQ(GetKeyType(t, key_type), EVP_PKEY_id(pkey.get()));

  // The key must re-encode correctly.
  bssl::ScopedCBB cbb;
  uint8_t *der;
  size_t der_len;
  if (!CBB_init(cbb.get(), 0) ||
      !marshal_func(cbb.get(), pkey.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  bssl::UniquePtr<uint8_t> free_der(der);

  std::vector<uint8_t> output = input;
  if (t->HasAttribute("Output") &&
      !t->GetBytes(&output, "Output")) {
    return false;
  }
  EXPECT_EQ(Bytes(output), Bytes(der, der_len)) << "Re-encoding the key did not match.";

  // Save the key for future tests.
  const std::string &key_name = t->GetParameter();
  EXPECT_EQ(0u, key_map->count(key_name)) << "Duplicate key: " << key_name;
  (*key_map)[key_name] = std::move(pkey);
  return true;
}

static bool TestEVP(FileTest *t, KeyMap *key_map) {
  if (t->GetType() == "PrivateKey") {
    return ImportKey(t, key_map, EVP_parse_private_key,
                     EVP_marshal_private_key);
  }

  if (t->GetType() == "PublicKey") {
    return ImportKey(t, key_map, EVP_parse_public_key, EVP_marshal_public_key);
  }

  int (*key_op_init)(EVP_PKEY_CTX *ctx);
  int (*key_op)(EVP_PKEY_CTX *ctx, uint8_t *out, size_t *out_len,
                const uint8_t *in, size_t in_len) = nullptr;
  int (*verify_op)(EVP_PKEY_CTX * ctx, const uint8_t *sig, size_t sig_len,
                   const uint8_t *in, size_t in_len) = nullptr;
  if (t->GetType() == "Decrypt") {
    key_op_init = EVP_PKEY_decrypt_init;
    key_op = EVP_PKEY_decrypt;
  } else if (t->GetType() == "Sign") {
    key_op_init = EVP_PKEY_sign_init;
    key_op = EVP_PKEY_sign;
  } else if (t->GetType() == "SignMessage") {
    key_op_init = EVP_PKEY_sign_init;
    key_op = EVP_PKEY_sign_message;
  } else if (t->GetType() == "Verify") {
    key_op_init = EVP_PKEY_verify_init;
    verify_op = EVP_PKEY_verify;
  } else if (t->GetType() == "VerifyMessage") {
    key_op_init = EVP_PKEY_verify_init;
    verify_op = EVP_PKEY_verify_message;
  } else {
    ADD_FAILURE() << "Unknown test " << t->GetType();
    return false;
  }

  // Load the key.
  const std::string &key_name = t->GetParameter();
  if (key_map->count(key_name) == 0) {
    ADD_FAILURE() << "Could not find key " << key_name;
    return false;
  }
  EVP_PKEY *key = (*key_map)[key_name].get();

  std::vector<uint8_t> input;
  if (!t->GetBytes(&input, "Input")) {
    return false;
  }

  // Set up the EVP_PKEY_CTX.
  bssl::UniquePtr<EVP_PKEY_CTX> ctx(EVP_PKEY_CTX_new(key, nullptr));
  if (!ctx || !key_op_init(ctx.get())) {
    return false;
  }
  if (t->HasAttribute("Digest")) {
    const EVP_MD *digest = GetDigest(t, t->GetAttributeOrDie("Digest"));
    if (digest == nullptr ||
        !EVP_PKEY_CTX_set_signature_md(ctx.get(), digest)) {
      return false;
    }
  }
  if (t->HasAttribute("RSAPadding")) {
    int padding;
    if (!GetRSAPadding(t, &padding, t->GetAttributeOrDie("RSAPadding")) ||
        !EVP_PKEY_CTX_set_rsa_padding(ctx.get(), padding)) {
      return false;
    }
  }
  if (t->HasAttribute("PSSSaltLength") &&
      !EVP_PKEY_CTX_set_rsa_pss_saltlen(
          ctx.get(), atoi(t->GetAttributeOrDie("PSSSaltLength").c_str()))) {
    return false;
  }
  if (t->HasAttribute("MGF1Digest")) {
    const EVP_MD *digest = GetDigest(t, t->GetAttributeOrDie("MGF1Digest"));
    if (digest == nullptr ||
        !EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), digest)) {
      return false;
    }
  }

  if (verify_op != nullptr) {
    std::vector<uint8_t> output;
    if (!t->GetBytes(&output, "Output") ||
        !verify_op(ctx.get(), output.data(), output.size(), input.data(),
                   input.size())) {
      return false;
    }
    return true;
  }

  size_t len;
  std::vector<uint8_t> actual, output;
  if (!key_op(ctx.get(), nullptr, &len, input.data(), input.size())) {
    return false;
  }
  actual.resize(len);
  if (!key_op(ctx.get(), actual.data(), &len, input.data(), input.size())) {
    return false;
  }
  actual.resize(len);
  if (!t->GetBytes(&output, "Output")) {
    return false;
  }

  EXPECT_EQ(Bytes(output), Bytes(actual));
  return true;
}

TEST(EVPTest, TestVectors) {
  KeyMap key_map;
  FileTestGTest("crypto/evp/evp_tests.txt", [&](FileTest *t) {
    bool result = TestEVP(t, &key_map);
    if (t->HasAttribute("Error")) {
      ASSERT_FALSE(result) << "Operation unexpectedly succeeded.";
      uint32_t err = ERR_peek_error();
      EXPECT_EQ(t->GetAttributeOrDie("Error"), ERR_reason_error_string(err));
    } else {
      ASSERT_TRUE(result) << "Operation unexpectedly failed.";
    }
  });
}
