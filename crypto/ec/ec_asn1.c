/* Written by Nils Larsch for the OpenSSL project. */
/* ====================================================================
 * Copyright (c) 2000-2003 The OpenSSL Project.  All rights reserved.
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
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <openssl/ec.h>

#include <limits.h>
#include <string.h>

#include <openssl/bytestring.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>

#include "internal.h"


static const uint8_t kParametersTag =
    CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0;
static const uint8_t kPublicKeyTag =
    CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 1;

EC_KEY *EC_KEY_parse_private_key(CBS *cbs, const EC_GROUP *group) {
  CBS ec_private_key, private_key;
  uint64_t version;
  if (!CBS_get_asn1(cbs, &ec_private_key, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1_uint64(&ec_private_key, &version) ||
      version != 1 ||
      !CBS_get_asn1(&ec_private_key, &private_key, CBS_ASN1_OCTETSTRING)) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return NULL;
  }

  /* Parse the optional parameters field. */
  EC_GROUP *inner_group = NULL;
  EC_KEY *ret = NULL;
  if (CBS_peek_asn1_tag(&ec_private_key, kParametersTag)) {
    /* Per SEC 1, as an alternative to omitting it, one is allowed to specify
     * this field and put in a NULL to mean inheriting this value. This was
     * omitted in a previous version of this logic without problems, so leave it
     * unimplemented. */
    CBS child;
    if (!CBS_get_asn1(&ec_private_key, &child, kParametersTag)) {
      OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
      return NULL;
    }
    inner_group = EC_KEY_parse_parameters(&child);
    if (inner_group == NULL) {
      return NULL;
    }
    if (group == NULL) {
      group = inner_group;
    } else if (EC_GROUP_cmp(group, inner_group, NULL) != 0) {
      /* If a group was supplied externally, it must match. */
      OPENSSL_PUT_ERROR(EC, EC_R_GROUP_MISMATCH);
      goto err;
    }
    if (CBS_len(&child) != 0) {
      OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
      goto err;
    }
  }

  if (group == NULL) {
    OPENSSL_PUT_ERROR(EC, EC_R_MISSING_PARAMETERS);
    goto err;
  }

  ret = EC_KEY_new();
  if (ret == NULL || !EC_KEY_set_group(ret, group)) {
    goto err;
  }

  /* Although RFC 5915 specifies the length of the key, OpenSSL historically
   * got this wrong, so accept any length. See upstream's
   * 30cd4ff294252c4b6a4b69cbef6a5b4117705d22. */
  ret->priv_key =
      BN_bin2bn(CBS_data(&private_key), CBS_len(&private_key), NULL);
  ret->pub_key = EC_POINT_new(group);
  if (ret->priv_key == NULL || ret->pub_key == NULL) {
    goto err;
  }

  if (BN_cmp(ret->priv_key, EC_GROUP_get0_order(group)) >= 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_WRONG_ORDER);
    goto err;
  }

  if (CBS_peek_asn1_tag(&ec_private_key, kPublicKeyTag)) {
    CBS child, public_key;
    uint8_t padding;
    if (!CBS_get_asn1(&ec_private_key, &child, kPublicKeyTag) ||
        !CBS_get_asn1(&child, &public_key, CBS_ASN1_BITSTRING) ||
        /* As in a SubjectPublicKeyInfo, the byte-encoded public key is then
         * encoded as a BIT STRING with bits ordered as in the DER encoding. */
        !CBS_get_u8(&public_key, &padding) ||
        padding != 0 ||
        !EC_POINT_oct2point(group, ret->pub_key, CBS_data(&public_key),
                            CBS_len(&public_key), NULL) ||
        CBS_len(&child) != 0) {
      OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
      goto err;
    }
    /* Compute the public key instead. */
  } else if (!EC_POINT_mul(group, ret->pub_key, ret->priv_key, NULL, NULL,
                           NULL)) {
    goto err;
  }

  if (CBS_len(&ec_private_key) != 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    goto err;
  }

  /* Ensure the resulting key is valid. */
  if (!EC_KEY_check_key(ret)) {
    goto err;
  }

  EC_GROUP_free(inner_group);
  return ret;

err:
  EC_KEY_free(ret);
  EC_GROUP_free(inner_group);
  return NULL;
}

int EC_KEY_marshal_private_key(CBB *cbb, const EC_KEY *key,
                               unsigned enc_flags) {
  if (key == NULL || key->group == NULL || key->priv_key == NULL) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  CBB ec_private_key, private_key;
  if (!CBB_add_asn1(cbb, &ec_private_key, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_uint64(&ec_private_key, 1 /* version */) ||
      !CBB_add_asn1(&ec_private_key, &private_key, CBS_ASN1_OCTETSTRING) ||
      !BN_bn2cbb_padded(&private_key,
                        BN_num_bytes(EC_GROUP_get0_order(key->group)),
                        key->priv_key)) {
    OPENSSL_PUT_ERROR(EC, EC_R_ENCODE_ERROR);
    return 0;
  }

  if (!(enc_flags & EC_PKEY_NO_PARAMETERS)) {
    int curve_nid = EC_GROUP_get_curve_name(key->group);
    if (curve_nid == NID_undef) {
      OPENSSL_PUT_ERROR(EC, EC_R_UNKNOWN_GROUP);
      return 0;
    }
    CBB child;
    if (!CBB_add_asn1(&ec_private_key, &child, kParametersTag) ||
        !OBJ_nid2cbb(&child, curve_nid) ||
        !CBB_flush(&ec_private_key)) {
      OPENSSL_PUT_ERROR(EC, EC_R_ENCODE_ERROR);
      return 0;
    }
  }

  /* TODO(fork): replace this flexibility with sensible default? */
  if (!(enc_flags & EC_PKEY_NO_PUBKEY) && key->pub_key != NULL) {
    CBB child, public_key;
    if (!CBB_add_asn1(&ec_private_key, &child, kPublicKeyTag) ||
        !CBB_add_asn1(&child, &public_key, CBS_ASN1_BITSTRING) ||
        /* As in a SubjectPublicKeyInfo, the byte-encoded public key is then
         * encoded as a BIT STRING with bits ordered as in the DER encoding. */
        !CBB_add_u8(&public_key, 0 /* padding */) ||
        !EC_POINT_point2cbb(&public_key, key->group, key->pub_key,
                            POINT_CONVERSION_UNCOMPRESSED, NULL) ||
        !CBB_flush(&ec_private_key)) {
      OPENSSL_PUT_ERROR(EC, EC_R_ENCODE_ERROR);
      return 0;
    }
  }

  if (!CBB_flush(cbb)) {
    OPENSSL_PUT_ERROR(EC, EC_R_ENCODE_ERROR);
    return 0;
  }

  return 1;
}

/* is_unsigned_integer returns one if |cbs| is a valid unsigned DER INTEGER and
 * zero otherwise. */
static int is_unsigned_integer(const CBS *cbs) {
  if (CBS_len(cbs) == 0) {
    return 0;
  }
  uint8_t byte = CBS_data(cbs)[0];
  if (byte & 0x80 ||
      (byte == 0 && CBS_len(cbs) > 1 && (CBS_data(cbs)[1] & 0x80) == 0)) {
    /* Negative or not minimally-encoded. */
    return 0;
  }
  return 1;
}

static int parse_explicit_prime_curve(CBS *out_prime, CBS *out_a, CBS *out_b,
                                      CBS *out_base_x, CBS *out_base_y,
                                      CBS *out_order, CBS *in) {
  /* See RFC 3279, section 2.3.5. Note that RFC 3279 calls this structure an
   * ECParameters while RFC 5480 calls it a SpecifiedECDomain. */
  CBS params, field_id, field_type, curve, base;
  uint64_t version;
  if (!CBS_get_asn1(in, &params, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1_uint64(&params, &version) ||
      version != 1 ||
      !CBS_get_asn1(&params, &field_id, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&field_id, &field_type, CBS_ASN1_OBJECT) ||
      OBJ_cbs2nid(&field_type) != NID_X9_62_prime_field ||
      !CBS_get_asn1(&field_id, out_prime, CBS_ASN1_INTEGER) ||
      !is_unsigned_integer(out_prime) ||
      CBS_len(&field_id) != 0 ||
      !CBS_get_asn1(&params, &curve, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&curve, out_a, CBS_ASN1_OCTETSTRING) ||
      !CBS_get_asn1(&curve, out_b, CBS_ASN1_OCTETSTRING) ||
      /* |curve| has an optional BIT STRING seed which we ignore. */
      !CBS_get_asn1(&params, &base, CBS_ASN1_OCTETSTRING) ||
      !CBS_get_asn1(&params, out_order, CBS_ASN1_INTEGER) ||
      !is_unsigned_integer(out_order)) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return 0;
  }

  /* |params| has additionally may have an optional cofactor which we ignore.
   * With the seed in |curve|, there is already plenty of room for arbitrarily
   * many encodings of any given curve. We'll only parse enough to uniquely
   * determine the curve. */

  /* Require that the base point use uncompressed form. */
  uint8_t form;
  if (!CBS_get_u8(&base, &form) || form != POINT_CONVERSION_UNCOMPRESSED) {
    OPENSSL_PUT_ERROR(EC, EC_R_INVALID_FORM);
    return 0;
  }

  if (CBS_len(&base) % 2 != 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return 0;
  }
  size_t field_len = CBS_len(&base) / 2;
  CBS_init(out_base_x, CBS_data(&base), field_len);
  CBS_init(out_base_y, CBS_data(&base) + field_len, field_len);

  return 1;
}

static int integers_equal(const CBS *a, const uint8_t *b, size_t b_len) {
  /* Remove leading zeros from |a| and |b|. */
  CBS a_copy = *a;
  while (CBS_len(&a_copy) > 0 && CBS_data(&a_copy)[0] == 0) {
    CBS_skip(&a_copy, 1);
  }
  while (b_len > 0 && b[0] == 0) {
    b++;
    b_len--;
  }
  return CBS_mem_equal(&a_copy, b, b_len);
}

EC_GROUP *EC_KEY_parse_parameters(CBS *cbs) {
  if (CBS_peek_asn1_tag(cbs, CBS_ASN1_SEQUENCE)) {
    /* OpenSSL sometimes produces ECPrivateKeys with explicitly-encoded versions
     * of named curves.
     *
     * TODO(davidben): Phase support for this out. */
    CBS prime, a, b, base_x, base_y, order;
    if (!parse_explicit_prime_curve(&prime, &a, &b, &base_x, &base_y, &order,
                                    cbs)) {
      return NULL;
    }

    /* Look for a matching prime curve. */
    unsigned i;
    for (i = 0; OPENSSL_built_in_curves[i].nid != NID_undef; i++) {
      const struct built_in_curve *curve = &OPENSSL_built_in_curves[i];
      const unsigned param_len = curve->data->param_len;
      /* |curve->data->data| is ordered p, a, b, x, y, order, each component
       * zero-padded up to the field length. Although this is compatible with
       * SEC 1 which states that the Field-Element-to-Octet-String conversion
       * also pads, OpenSSL mis-encodes |a| and |b|, so this comparison must be
       * lenient. (This is relevant for P-521 whose |b| has a leading 0.) */
      if (integers_equal(&prime, curve->data->data, param_len) &&
          integers_equal(&a, curve->data->data + param_len, param_len) &&
          integers_equal(&b, curve->data->data + param_len * 2, param_len) &&
          integers_equal(&base_x, curve->data->data + param_len * 3,
                         param_len) &&
          integers_equal(&base_y, curve->data->data + param_len * 4,
                         param_len) &&
          integers_equal(&order, curve->data->data + param_len * 5,
                         param_len)) {
        return EC_GROUP_new_by_curve_name(curve->nid);
      }
    }

    OPENSSL_PUT_ERROR(EC, EC_R_UNKNOWN_GROUP);
    return NULL;
  }

  CBS named_curve;
  if (!CBS_get_asn1(cbs, &named_curve, CBS_ASN1_OBJECT)) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return NULL;
  }
  return EC_GROUP_new_by_curve_name(OBJ_cbs2nid(&named_curve));
}

EC_KEY *d2i_ECPrivateKey(EC_KEY **out, const uint8_t **inp, long len) {
  /* If supplied, take the group from |*out|. */
  const EC_GROUP *group = NULL;
  if (out != NULL && *out != NULL) {
    group = EC_KEY_get0_group(*out);
  }

  if (len < 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return NULL;
  }
  CBS cbs;
  CBS_init(&cbs, *inp, (size_t)len);
  EC_KEY *ret = EC_KEY_parse_private_key(&cbs, group);
  if (ret == NULL) {
    return NULL;
  }
  if (out != NULL) {
    EC_KEY_free(*out);
    *out = ret;
  }
  *inp = CBS_data(&cbs);
  return ret;
}

int i2d_ECPrivateKey(const EC_KEY *key, uint8_t **outp) {
  uint8_t *der;
  size_t der_len;
  CBB cbb;
  if (!CBB_init(&cbb, 0) ||
      !EC_KEY_marshal_private_key(&cbb, key, 0) ||
      !CBB_finish(&cbb, &der, &der_len)) {
    OPENSSL_PUT_ERROR(EC, EC_R_ENCODE_ERROR);
    return -1;
  }
  if (der_len > INT_MAX) {
    OPENSSL_PUT_ERROR(EC, ERR_R_OVERFLOW);
    OPENSSL_free(der);
    return -1;
  }
  if (outp != NULL) {
    if (*outp == NULL) {
      *outp = der;
      der = NULL;
    } else {
      memcpy(*outp, der, der_len);
      *outp += der_len;
    }
  }
  OPENSSL_free(der);
  return (int)der_len;
}

int i2d_ECParameters(const EC_KEY *key, uint8_t **outp) {
  if (key == NULL || key->group == NULL) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return -1;
  }

  int curve_nid = EC_GROUP_get_curve_name(key->group);
  if (curve_nid == NID_undef) {
    OPENSSL_PUT_ERROR(EC, EC_R_UNKNOWN_GROUP);
    return -1;
  }

  uint8_t *der;
  size_t der_len;
  CBB cbb;
  if (!CBB_init(&cbb, 0) ||
      !OBJ_nid2cbb(&cbb, curve_nid) ||
      !CBB_finish(&cbb, &der, &der_len)) {
    OPENSSL_PUT_ERROR(EC, EC_R_ENCODE_ERROR);
    return -1;
  }
  if (der_len > INT_MAX) {
    OPENSSL_PUT_ERROR(EC, ERR_R_OVERFLOW);
    OPENSSL_free(der);
    return -1;
  }
  if (outp != NULL) {
    if (*outp == NULL) {
      *outp = der;
      der = NULL;
    } else {
      memcpy(*outp, der, der_len);
      *outp += der_len;
    }
  }
  OPENSSL_free(der);
  return (int)der_len;
}

EC_KEY *d2i_ECParameters(EC_KEY **key, const uint8_t **inp, long len) {
  if (len < 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return NULL;
  }
  CBS cbs;
  CBS_init(&cbs, *inp, (size_t)len);
  EC_GROUP *group = EC_KEY_parse_parameters(&cbs);
  if (group == NULL) {
    return NULL;
  }

  EC_KEY *ret = EC_KEY_new();
  if (ret == NULL || !EC_KEY_set_group(ret, group)) {
    EC_GROUP_free(group);
    EC_KEY_free(ret);
    return NULL;
  }
  EC_GROUP_free(group);

  if (key != NULL) {
    EC_KEY_free(*key);
    *key = ret;
  }
  *inp = CBS_data(&cbs);
  return ret;
}

EC_KEY *o2i_ECPublicKey(EC_KEY **keyp, const uint8_t **inp, long len) {
  EC_KEY *ret = NULL;

  if (keyp == NULL || *keyp == NULL || (*keyp)->group == NULL) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return NULL;
  }
  ret = *keyp;
  if (ret->pub_key == NULL &&
      (ret->pub_key = EC_POINT_new(ret->group)) == NULL) {
    OPENSSL_PUT_ERROR(EC, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  if (!EC_POINT_oct2point(ret->group, ret->pub_key, *inp, len, NULL)) {
    OPENSSL_PUT_ERROR(EC, ERR_R_EC_LIB);
    return NULL;
  }
  *inp += len;
  return ret;
}

int i2o_ECPublicKey(const EC_KEY *key, uint8_t **outp) {
  size_t buf_len = 0;
  int new_buffer = 0;

  if (key == NULL) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  buf_len = EC_POINT_point2oct(key->group, key->pub_key,
                               POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);

  if (outp == NULL || buf_len == 0) {
    /* out == NULL => just return the length of the octet string */
    return buf_len;
  }

  if (*outp == NULL) {
    *outp = OPENSSL_malloc(buf_len);
    if (*outp == NULL) {
      OPENSSL_PUT_ERROR(EC, ERR_R_MALLOC_FAILURE);
      return 0;
    }
    new_buffer = 1;
  }
  if (!EC_POINT_point2oct(key->group, key->pub_key,
                          POINT_CONVERSION_UNCOMPRESSED, *outp, buf_len,
                          NULL)) {
    OPENSSL_PUT_ERROR(EC, ERR_R_EC_LIB);
    if (new_buffer) {
      OPENSSL_free(*outp);
      *outp = NULL;
    }
    return 0;
  }

  if (!new_buffer) {
    *outp += buf_len;
  }
  return buf_len;
}
