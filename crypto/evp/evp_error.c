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

#include <openssl/err.h>

#include <openssl/evp.h>

const ERR_STRING_DATA EVP_error_string_data[] = {
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_DigestSignAlgorithm, 0), "EVP_DigestSignAlgorithm"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_DigestVerifyInitFromAlgorithm, 0), "EVP_DigestVerifyInitFromAlgorithm"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_CTX_ctrl, 0), "EVP_PKEY_CTX_ctrl"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_CTX_dup, 0), "EVP_PKEY_CTX_dup"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_copy_parameters, 0), "EVP_PKEY_copy_parameters"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_decrypt, 0), "EVP_PKEY_decrypt"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_decrypt_init, 0), "EVP_PKEY_decrypt_init"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_derive, 0), "EVP_PKEY_derive"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_derive_init, 0), "EVP_PKEY_derive_init"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_derive_set_peer, 0), "EVP_PKEY_derive_set_peer"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_encrypt, 0), "EVP_PKEY_encrypt"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_encrypt_init, 0), "EVP_PKEY_encrypt_init"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_get1_DH, 0), "EVP_PKEY_get1_DH"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_get1_DSA, 0), "EVP_PKEY_get1_DSA"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_get1_EC_KEY, 0), "EVP_PKEY_get1_EC_KEY"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_get1_RSA, 0), "EVP_PKEY_get1_RSA"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_keygen, 0), "EVP_PKEY_keygen"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_keygen_init, 0), "EVP_PKEY_keygen_init"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_new, 0), "EVP_PKEY_new"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_set_type, 0), "EVP_PKEY_set_type"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_sign, 0), "EVP_PKEY_sign"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_sign_init, 0), "EVP_PKEY_sign_init"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_verify, 0), "EVP_PKEY_verify"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_PKEY_verify_init, 0), "EVP_PKEY_verify_init"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_HKDF, 0), "HKDF"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_check_padding_md, 0), "check_padding_md"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_d2i_AutoPrivateKey, 0), "d2i_AutoPrivateKey"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_d2i_PrivateKey, 0), "d2i_PrivateKey"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_do_EC_KEY_print, 0), "do_EC_KEY_print"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_do_rsa_print, 0), "do_rsa_print"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_do_sigver_init, 0), "do_sigver_init"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_eckey_param2type, 0), "eckey_param2type"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_eckey_param_decode, 0), "eckey_param_decode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_eckey_priv_decode, 0), "eckey_priv_decode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_eckey_priv_encode, 0), "eckey_priv_encode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_eckey_pub_decode, 0), "eckey_pub_decode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_eckey_pub_encode, 0), "eckey_pub_encode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_eckey_type2param, 0), "eckey_type2param"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_evp_pkey_ctx_new, 0), "evp_pkey_ctx_new"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_hmac_signctx, 0), "hmac_signctx"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_i2d_PublicKey, 0), "i2d_PublicKey"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_old_ec_priv_decode, 0), "old_ec_priv_decode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_old_rsa_priv_decode, 0), "old_rsa_priv_decode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_ec_ctrl, 0), "pkey_ec_ctrl"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_ec_derive, 0), "pkey_ec_derive"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_ec_keygen, 0), "pkey_ec_keygen"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_ec_paramgen, 0), "pkey_ec_paramgen"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_ec_sign, 0), "pkey_ec_sign"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_rsa_ctrl, 0), "pkey_rsa_ctrl"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_rsa_decrypt, 0), "pkey_rsa_decrypt"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_rsa_encrypt, 0), "pkey_rsa_encrypt"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_pkey_rsa_sign, 0), "pkey_rsa_sign"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_algor_to_md, 0), "rsa_algor_to_md"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_digest_verify_init_from_algorithm, 0), "rsa_digest_verify_init_from_algorithm"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_item_verify, 0), "rsa_item_verify"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_mgf1_to_md, 0), "rsa_mgf1_to_md"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_priv_decode, 0), "rsa_priv_decode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_priv_encode, 0), "rsa_priv_encode"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_pss_to_ctx, 0), "rsa_pss_to_ctx"},
  {ERR_PACK(ERR_LIB_EVP, EVP_F_rsa_pub_decode, 0), "rsa_pub_decode"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_BUFFER_TOO_SMALL), "BUFFER_TOO_SMALL"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_COMMAND_NOT_SUPPORTED), "COMMAND_NOT_SUPPORTED"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_CONTEXT_NOT_INITIALISED), "CONTEXT_NOT_INITIALISED"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_DECODE_ERROR), "DECODE_ERROR"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_DIFFERENT_KEY_TYPES), "DIFFERENT_KEY_TYPES"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_DIFFERENT_PARAMETERS), "DIFFERENT_PARAMETERS"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_DIGEST_AND_KEY_TYPE_NOT_SUPPORTED), "DIGEST_AND_KEY_TYPE_NOT_SUPPORTED"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_DIGEST_DOES_NOT_MATCH), "DIGEST_DOES_NOT_MATCH"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_EXPECTING_AN_DSA_KEY), "EXPECTING_AN_DSA_KEY"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_EXPECTING_AN_EC_KEY_KEY), "EXPECTING_AN_EC_KEY_KEY"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_EXPECTING_AN_RSA_KEY), "EXPECTING_AN_RSA_KEY"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_EXPECTING_A_DH_KEY), "EXPECTING_A_DH_KEY"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_EXPECTING_A_DSA_KEY), "EXPECTING_A_DSA_KEY"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_EXPLICIT_EC_PARAMETERS_NOT_SUPPORTED), "EXPLICIT_EC_PARAMETERS_NOT_SUPPORTED"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_ILLEGAL_OR_UNSUPPORTED_PADDING_MODE), "ILLEGAL_OR_UNSUPPORTED_PADDING_MODE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_CURVE), "INVALID_CURVE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_DIGEST_LENGTH), "INVALID_DIGEST_LENGTH"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_DIGEST_TYPE), "INVALID_DIGEST_TYPE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_KEYBITS), "INVALID_KEYBITS"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_MGF1_MD), "INVALID_MGF1_MD"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_OPERATION), "INVALID_OPERATION"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_PADDING_MODE), "INVALID_PADDING_MODE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_PSS_PARAMETERS), "INVALID_PSS_PARAMETERS"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_PSS_SALTLEN), "INVALID_PSS_SALTLEN"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_SALT_LENGTH), "INVALID_SALT_LENGTH"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_INVALID_TRAILER), "INVALID_TRAILER"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_KDF_PARAMETER_ERROR), "KDF_PARAMETER_ERROR"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_KEYS_NOT_SET), "KEYS_NOT_SET"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_MISSING_PARAMETERS), "MISSING_PARAMETERS"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_NO_DEFAULT_DIGEST), "NO_DEFAULT_DIGEST"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_NO_KEY_SET), "NO_KEY_SET"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_NO_MDC2_SUPPORT), "NO_MDC2_SUPPORT"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_NO_NID_FOR_CURVE), "NO_NID_FOR_CURVE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_NO_OPERATION_SET), "NO_OPERATION_SET"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_NO_PARAMETERS_SET), "NO_PARAMETERS_SET"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE), "OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_OPERATON_NOT_INITIALIZED), "OPERATON_NOT_INITIALIZED"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_PEER_KEY_ERROR), "PEER_KEY_ERROR"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_SHARED_INFO_ERROR), "SHARED_INFO_ERROR"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNKNOWN_DIGEST), "UNKNOWN_DIGEST"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNKNOWN_MASK_DIGEST), "UNKNOWN_MASK_DIGEST"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNKNOWN_MESSAGE_DIGEST_ALGORITHM), "UNKNOWN_MESSAGE_DIGEST_ALGORITHM"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNKNOWN_PUBLIC_KEY_TYPE), "UNKNOWN_PUBLIC_KEY_TYPE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNKNOWN_SIGNATURE_ALGORITHM), "UNKNOWN_SIGNATURE_ALGORITHM"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNSUPPORTED_ALGORITHM), "UNSUPPORTED_ALGORITHM"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNSUPPORTED_MASK_ALGORITHM), "UNSUPPORTED_MASK_ALGORITHM"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNSUPPORTED_MASK_PARAMETER), "UNSUPPORTED_MASK_PARAMETER"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNSUPPORTED_PUBLIC_KEY_TYPE), "UNSUPPORTED_PUBLIC_KEY_TYPE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_UNSUPPORTED_SIGNATURE_TYPE), "UNSUPPORTED_SIGNATURE_TYPE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_WRONG_PUBLIC_KEY_TYPE), "WRONG_PUBLIC_KEY_TYPE"},
  {ERR_PACK(ERR_LIB_EVP, 0, EVP_R_X931_UNSUPPORTED), "X931_UNSUPPORTED"},
  {0, NULL},
};
