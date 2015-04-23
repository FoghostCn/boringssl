/******************************************************************************
 *                                                                            *
 * Copyright 2014 Intel Corporation                                           *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *    http://www.apache.org/licenses/LICENSE-2.0                              *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 * Developers and authors:                                                    *
 * Shay Gueron (1, 2), and Vlad Krasnov (1)                                   *
 * (1) Intel Corporation, Israel Development Center                           *
 * (2) University of Haifa                                                    *
 * Reference:                                                                 *
 * S.Gueron and V.Krasnov, "Fast Prime Field Elliptic Curve Cryptography with *
 *                          256 Bit Primes"                                   *
 *                                                                            *
 ******************************************************************************/

#include <string.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../bn/internal.h"
#include "internal.h"

# define OPENSSL_MALLOC_MAX_NELEMS(type)  (((1U<<(sizeof(int)*8-1))-1)/sizeof(type))

#if BN_BITS2 != 64
# define TOBN(hi,lo)    lo,hi
#else
# define TOBN(hi,lo)    ((BN_ULONG)hi<<32|lo)
#endif

#if defined(__GNUC__)
# define ALIGN32        __attribute((aligned(32)))
#elif defined(_MSC_VER)
# define ALIGN32        __declspec(align(32))
#else
# define ALIGN32
#endif

#define ALIGNPTR(p,N)   ((unsigned char *)p+N-(size_t)p%N)
#define P256_LIMBS      (256/BN_BITS2)

typedef unsigned short u16;

typedef struct {
    BN_ULONG X[P256_LIMBS];
    BN_ULONG Y[P256_LIMBS];
    BN_ULONG Z[P256_LIMBS];
} P256_POINT;

typedef struct {
    BN_ULONG X[P256_LIMBS];
    BN_ULONG Y[P256_LIMBS];
} P256_POINT_AFFINE;

typedef P256_POINT_AFFINE PRECOMP256_ROW[64];

/* Functions implemented in assembly */
/* Modular mul by 2: res = 2*a mod P */
void ecp_nistz256_mul_by_2(BN_ULONG res[P256_LIMBS],
                           const BN_ULONG a[P256_LIMBS]);
/* Modular div by 2: res = a/2 mod P */
void ecp_nistz256_div_by_2(BN_ULONG res[P256_LIMBS],
                           const BN_ULONG a[P256_LIMBS]);
/* Modular mul by 3: res = 3*a mod P */
void ecp_nistz256_mul_by_3(BN_ULONG res[P256_LIMBS],
                           const BN_ULONG a[P256_LIMBS]);
/* Modular add: res = a+b mod P   */
void ecp_nistz256_add(BN_ULONG res[P256_LIMBS],
                      const BN_ULONG a[P256_LIMBS],
                      const BN_ULONG b[P256_LIMBS]);
/* Modular sub: res = a-b mod P   */
void ecp_nistz256_sub(BN_ULONG res[P256_LIMBS],
                      const BN_ULONG a[P256_LIMBS],
                      const BN_ULONG b[P256_LIMBS]);
/* Modular neg: res = -a mod P    */
void ecp_nistz256_neg(BN_ULONG res[P256_LIMBS], const BN_ULONG a[P256_LIMBS]);
/* Montgomery mul: res = a*b*2^-256 mod P */
void ecp_nistz256_mul_mont(BN_ULONG res[P256_LIMBS],
                           const BN_ULONG a[P256_LIMBS],
                           const BN_ULONG b[P256_LIMBS]);
/* Montgomery sqr: res = a*a*2^-256 mod P */
void ecp_nistz256_sqr_mont(BN_ULONG res[P256_LIMBS],
                           const BN_ULONG a[P256_LIMBS]);
/* Convert a number from Montgomery domain, by multiplying with 1 */
void ecp_nistz256_from_mont(BN_ULONG res[P256_LIMBS],
                            const BN_ULONG in[P256_LIMBS]);
/* Convert a number to Montgomery domain, by multiplying with 2^512 mod P*/
void ecp_nistz256_to_mont(BN_ULONG res[P256_LIMBS],
                          const BN_ULONG in[P256_LIMBS]);
/* Functions that perform constant time access to the precomputed tables */
void ecp_nistz256_scatter_w5(P256_POINT *val,
                             const P256_POINT *in_t, int idx);
void ecp_nistz256_gather_w5(P256_POINT *val,
                            const P256_POINT *in_t, int idx);
void ecp_nistz256_scatter_w7(P256_POINT_AFFINE *val,
                             const P256_POINT_AFFINE *in_t, int idx);
void ecp_nistz256_gather_w7(P256_POINT_AFFINE *val,
                            const P256_POINT_AFFINE *in_t, int idx);

/* One converted into the Montgomery domain */
static const BN_ULONG ONE[P256_LIMBS] = {
    TOBN(0x00000000, 0x00000001), TOBN(0xffffffff, 0x00000000),
    TOBN(0xffffffff, 0xffffffff), TOBN(0x00000000, 0xfffffffe)
};

/* Precomputed tables for the default generator */
extern const PRECOMP256_ROW ecp_nistz256_precomputed[37];

/* Recode window to a signed digit, see ecp_nistputil.c for details */
static unsigned int _booth_recode_w5(unsigned int in)
{
    unsigned int s, d;

    s = ~((in >> 5) - 1);
    d = (1 << 6) - in - 1;
    d = (d & s) | (in & ~s);
    d = (d >> 1) + (d & 1);

    return (d << 1) + (s & 1);
}

static unsigned int _booth_recode_w7(unsigned int in)
{
    unsigned int s, d;

    s = ~((in >> 7) - 1);
    d = (1 << 8) - in - 1;
    d = (d & s) | (in & ~s);
    d = (d >> 1) + (d & 1);

    return (d << 1) + (s & 1);
}

static void copy_conditional(BN_ULONG dst[P256_LIMBS],
                             const BN_ULONG src[P256_LIMBS], BN_ULONG move)
{
    BN_ULONG mask1 = 0-move;
    BN_ULONG mask2 = ~mask1;

    dst[0] = (src[0] & mask1) ^ (dst[0] & mask2);
    dst[1] = (src[1] & mask1) ^ (dst[1] & mask2);
    dst[2] = (src[2] & mask1) ^ (dst[2] & mask2);
    dst[3] = (src[3] & mask1) ^ (dst[3] & mask2);
    if (P256_LIMBS == 8) {
        dst[4] = (src[4] & mask1) ^ (dst[4] & mask2);
        dst[5] = (src[5] & mask1) ^ (dst[5] & mask2);
        dst[6] = (src[6] & mask1) ^ (dst[6] & mask2);
        dst[7] = (src[7] & mask1) ^ (dst[7] & mask2);
    }
}

static BN_ULONG is_zero(BN_ULONG in)
{
    in |= (0 - in);
    in = ~in;
    in &= BN_MASK2;
    in >>= BN_BITS2 - 1;
    return in;
}

static BN_ULONG is_equal(const BN_ULONG a[P256_LIMBS],
                         const BN_ULONG b[P256_LIMBS])
{
    BN_ULONG res;

    res = a[0] ^ b[0];
    res |= a[1] ^ b[1];
    res |= a[2] ^ b[2];
    res |= a[3] ^ b[3];
    if (P256_LIMBS == 8) {
        res |= a[4] ^ b[4];
        res |= a[5] ^ b[5];
        res |= a[6] ^ b[6];
        res |= a[7] ^ b[7];
    }

    return is_zero(res);
}

static BN_ULONG is_one(const BN_ULONG a[P256_LIMBS])
{
    BN_ULONG res;

    res = a[0] ^ ONE[0];
    res |= a[1] ^ ONE[1];
    res |= a[2] ^ ONE[2];
    res |= a[3] ^ ONE[3];
    if (P256_LIMBS == 8) {
        res |= a[4] ^ ONE[4];
        res |= a[5] ^ ONE[5];
        res |= a[6] ^ ONE[6];
    }

    return is_zero(res);
}

#if !defined(OPENSSL_NO_ASM)
void ecp_nistz256_point_double(P256_POINT *r, const P256_POINT *a);
void ecp_nistz256_point_add(P256_POINT *r,
                            const P256_POINT *a, const P256_POINT *b);
void ecp_nistz256_point_add_affine(P256_POINT *r,
                                   const P256_POINT *a,
                                   const P256_POINT_AFFINE *b);
#else
/* Point double: r = 2*a */
static void ecp_nistz256_point_double(P256_POINT *r, const P256_POINT *a)
{
    BN_ULONG S[P256_LIMBS];
    BN_ULONG M[P256_LIMBS];
    BN_ULONG Zsqr[P256_LIMBS];
    BN_ULONG tmp0[P256_LIMBS];

    const BN_ULONG *in_x = a->X;
    const BN_ULONG *in_y = a->Y;
    const BN_ULONG *in_z = a->Z;

    BN_ULONG *res_x = r->X;
    BN_ULONG *res_y = r->Y;
    BN_ULONG *res_z = r->Z;

    ecp_nistz256_mul_by_2(S, in_y);

    ecp_nistz256_sqr_mont(Zsqr, in_z);

    ecp_nistz256_sqr_mont(S, S);

    ecp_nistz256_mul_mont(res_z, in_z, in_y);
    ecp_nistz256_mul_by_2(res_z, res_z);

    ecp_nistz256_add(M, in_x, Zsqr);
    ecp_nistz256_sub(Zsqr, in_x, Zsqr);

    ecp_nistz256_sqr_mont(res_y, S);
    ecp_nistz256_div_by_2(res_y, res_y);

    ecp_nistz256_mul_mont(M, M, Zsqr);
    ecp_nistz256_mul_by_3(M, M);

    ecp_nistz256_mul_mont(S, S, in_x);
    ecp_nistz256_mul_by_2(tmp0, S);

    ecp_nistz256_sqr_mont(res_x, M);

    ecp_nistz256_sub(res_x, res_x, tmp0);
    ecp_nistz256_sub(S, S, res_x);

    ecp_nistz256_mul_mont(S, S, M);
    ecp_nistz256_sub(res_y, S, res_y);
}

/* Point addition: r = a+b */
static void ecp_nistz256_point_add(P256_POINT *r,
                                   const P256_POINT *a, const P256_POINT *b)
{
    BN_ULONG U2[P256_LIMBS], S2[P256_LIMBS];
    BN_ULONG U1[P256_LIMBS], S1[P256_LIMBS];
    BN_ULONG Z1sqr[P256_LIMBS];
    BN_ULONG Z2sqr[P256_LIMBS];
    BN_ULONG H[P256_LIMBS], R[P256_LIMBS];
    BN_ULONG Hsqr[P256_LIMBS];
    BN_ULONG Rsqr[P256_LIMBS];
    BN_ULONG Hcub[P256_LIMBS];

    BN_ULONG res_x[P256_LIMBS];
    BN_ULONG res_y[P256_LIMBS];
    BN_ULONG res_z[P256_LIMBS];

    BN_ULONG in1infty, in2infty;

    const BN_ULONG *in1_x = a->X;
    const BN_ULONG *in1_y = a->Y;
    const BN_ULONG *in1_z = a->Z;

    const BN_ULONG *in2_x = b->X;
    const BN_ULONG *in2_y = b->Y;
    const BN_ULONG *in2_z = b->Z;

    /* We encode infinity as (0,0), which is not on the curve,
     * so it is OK. */
    in1infty = (in1_x[0] | in1_x[1] | in1_x[2] | in1_x[3] |
                in1_y[0] | in1_y[1] | in1_y[2] | in1_y[3]);
    if (P256_LIMBS == 8)
        in1infty |= (in1_x[4] | in1_x[5] | in1_x[6] | in1_x[7] |
                     in1_y[4] | in1_y[5] | in1_y[6] | in1_y[7]);

    in2infty = (in2_x[0] | in2_x[1] | in2_x[2] | in2_x[3] |
                in2_y[0] | in2_y[1] | in2_y[2] | in2_y[3]);
    if (P256_LIMBS == 8)
        in2infty |= (in2_x[4] | in2_x[5] | in2_x[6] | in2_x[7] |
                     in2_y[4] | in2_y[5] | in2_y[6] | in2_y[7]);

    in1infty = is_zero(in1infty);
    in2infty = is_zero(in2infty);

    ecp_nistz256_sqr_mont(Z2sqr, in2_z);        /* Z2^2 */
    ecp_nistz256_sqr_mont(Z1sqr, in1_z);        /* Z1^2 */

    ecp_nistz256_mul_mont(S1, Z2sqr, in2_z);    /* S1 = Z2^3 */
    ecp_nistz256_mul_mont(S2, Z1sqr, in1_z);    /* S2 = Z1^3 */

    ecp_nistz256_mul_mont(S1, S1, in1_y);       /* S1 = Y1*Z2^3 */
    ecp_nistz256_mul_mont(S2, S2, in2_y);       /* S2 = Y2*Z1^3 */
    ecp_nistz256_sub(R, S2, S1);                /* R = S2 - S1 */

    ecp_nistz256_mul_mont(U1, in1_x, Z2sqr);    /* U1 = X1*Z2^2 */
    ecp_nistz256_mul_mont(U2, in2_x, Z1sqr);    /* U2 = X2*Z1^2 */
    ecp_nistz256_sub(H, U2, U1);                /* H = U2 - U1 */

    /*
     * This should not happen during sign/ecdh, so no constant time violation
     */
    if (is_equal(U1, U2) && !in1infty && !in2infty) {
        if (is_equal(S1, S2)) {
            ecp_nistz256_point_double(r, a);
            return;
        } else {
            memset(r, 0, sizeof(*r));
            return;
        }
    }

    ecp_nistz256_sqr_mont(Rsqr, R);             /* R^2 */
    ecp_nistz256_mul_mont(res_z, H, in1_z);     /* Z3 = H*Z1*Z2 */
    ecp_nistz256_sqr_mont(Hsqr, H);             /* H^2 */
    ecp_nistz256_mul_mont(res_z, res_z, in2_z); /* Z3 = H*Z1*Z2 */
    ecp_nistz256_mul_mont(Hcub, Hsqr, H);       /* H^3 */

    ecp_nistz256_mul_mont(U2, U1, Hsqr);        /* U1*H^2 */
    ecp_nistz256_mul_by_2(Hsqr, U2);            /* 2*U1*H^2 */

    ecp_nistz256_sub(res_x, Rsqr, Hsqr);
    ecp_nistz256_sub(res_x, res_x, Hcub);

    ecp_nistz256_sub(res_y, U2, res_x);

    ecp_nistz256_mul_mont(S2, S1, Hcub);
    ecp_nistz256_mul_mont(res_y, R, res_y);
    ecp_nistz256_sub(res_y, res_y, S2);

    copy_conditional(res_x, in2_x, in1infty);
    copy_conditional(res_y, in2_y, in1infty);
    copy_conditional(res_z, in2_z, in1infty);

    copy_conditional(res_x, in1_x, in2infty);
    copy_conditional(res_y, in1_y, in2infty);
    copy_conditional(res_z, in1_z, in2infty);

    memcpy(r->X, res_x, sizeof(res_x));
    memcpy(r->Y, res_y, sizeof(res_y));
    memcpy(r->Z, res_z, sizeof(res_z));
}

/* Point addition when b is known to be affine: r = a+b */
static void ecp_nistz256_point_add_affine(P256_POINT *r,
                                          const P256_POINT *a,
                                          const P256_POINT_AFFINE *b)
{
    BN_ULONG U2[P256_LIMBS], S2[P256_LIMBS];
    BN_ULONG Z1sqr[P256_LIMBS];
    BN_ULONG H[P256_LIMBS], R[P256_LIMBS];
    BN_ULONG Hsqr[P256_LIMBS];
    BN_ULONG Rsqr[P256_LIMBS];
    BN_ULONG Hcub[P256_LIMBS];

    BN_ULONG res_x[P256_LIMBS];
    BN_ULONG res_y[P256_LIMBS];
    BN_ULONG res_z[P256_LIMBS];

    BN_ULONG in1infty, in2infty;

    const BN_ULONG *in1_x = a->X;
    const BN_ULONG *in1_y = a->Y;
    const BN_ULONG *in1_z = a->Z;

    const BN_ULONG *in2_x = b->X;
    const BN_ULONG *in2_y = b->Y;

    /*
     * In affine representation we encode infty as (0,0), which is not on the
     * curve, so it is OK
     */
    in1infty = (in1_x[0] | in1_x[1] | in1_x[2] | in1_x[3] |
                in1_y[0] | in1_y[1] | in1_y[2] | in1_y[3]);
    if (P256_LIMBS == 8)
        in1infty |= (in1_x[4] | in1_x[5] | in1_x[6] | in1_x[7] |
                     in1_y[4] | in1_y[5] | in1_y[6] | in1_y[7]);

    in2infty = (in2_x[0] | in2_x[1] | in2_x[2] | in2_x[3] |
                in2_y[0] | in2_y[1] | in2_y[2] | in2_y[3]);
    if (P256_LIMBS == 8)
        in2infty |= (in2_x[4] | in2_x[5] | in2_x[6] | in2_x[7] |
                     in2_y[4] | in2_y[5] | in2_y[6] | in2_y[7]);

    in1infty = is_zero(in1infty);
    in2infty = is_zero(in2infty);

    ecp_nistz256_sqr_mont(Z1sqr, in1_z);        /* Z1^2 */

    ecp_nistz256_mul_mont(U2, in2_x, Z1sqr);    /* U2 = X2*Z1^2 */
    ecp_nistz256_sub(H, U2, in1_x);             /* H = U2 - U1 */

    ecp_nistz256_mul_mont(S2, Z1sqr, in1_z);    /* S2 = Z1^3 */

    ecp_nistz256_mul_mont(res_z, H, in1_z);     /* Z3 = H*Z1*Z2 */

    ecp_nistz256_mul_mont(S2, S2, in2_y);       /* S2 = Y2*Z1^3 */
    ecp_nistz256_sub(R, S2, in1_y);             /* R = S2 - S1 */

    ecp_nistz256_sqr_mont(Hsqr, H);             /* H^2 */
    ecp_nistz256_sqr_mont(Rsqr, R);             /* R^2 */
    ecp_nistz256_mul_mont(Hcub, Hsqr, H);       /* H^3 */

    ecp_nistz256_mul_mont(U2, in1_x, Hsqr);     /* U1*H^2 */
    ecp_nistz256_mul_by_2(Hsqr, U2);            /* 2*U1*H^2 */

    ecp_nistz256_sub(res_x, Rsqr, Hsqr);
    ecp_nistz256_sub(res_x, res_x, Hcub);
    ecp_nistz256_sub(H, U2, res_x);

    ecp_nistz256_mul_mont(S2, in1_y, Hcub);
    ecp_nistz256_mul_mont(H, H, R);
    ecp_nistz256_sub(res_y, H, S2);

    copy_conditional(res_x, in2_x, in1infty);
    copy_conditional(res_x, in1_x, in2infty);

    copy_conditional(res_y, in2_y, in1infty);
    copy_conditional(res_y, in1_y, in2infty);

    copy_conditional(res_z, ONE, in1infty);
    copy_conditional(res_z, in1_z, in2infty);

    memcpy(r->X, res_x, sizeof(res_x));
    memcpy(r->Y, res_y, sizeof(res_y));
    memcpy(r->Z, res_z, sizeof(res_z));
}
#endif

/* r = in^-1 mod p */
static void ecp_nistz256_mod_inverse(BN_ULONG r[P256_LIMBS],
                                     const BN_ULONG in[P256_LIMBS])
{
    /*
     * The poly is ffffffff 00000001 00000000 00000000 00000000 ffffffff
     * ffffffff ffffffff We use FLT and used poly-2 as exponent
     */
    BN_ULONG p2[P256_LIMBS];
    BN_ULONG p4[P256_LIMBS];
    BN_ULONG p8[P256_LIMBS];
    BN_ULONG p16[P256_LIMBS];
    BN_ULONG p32[P256_LIMBS];
    BN_ULONG res[P256_LIMBS];
    int i;

    ecp_nistz256_sqr_mont(res, in);
    ecp_nistz256_mul_mont(p2, res, in);         /* 3*p */

    ecp_nistz256_sqr_mont(res, p2);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(p4, res, p2);         /* f*p */

    ecp_nistz256_sqr_mont(res, p4);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(p8, res, p4);         /* ff*p */

    ecp_nistz256_sqr_mont(res, p8);
    for (i = 0; i < 7; i++)
        ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(p16, res, p8);        /* ffff*p */

    ecp_nistz256_sqr_mont(res, p16);
    for (i = 0; i < 15; i++)
        ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(p32, res, p16);       /* ffffffff*p */

    ecp_nistz256_sqr_mont(res, p32);
    for (i = 0; i < 31; i++)
        ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, in);

    for (i = 0; i < 32 * 4; i++)
        ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, p32);

    for (i = 0; i < 32; i++)
        ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, p32);

    for (i = 0; i < 16; i++)
        ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, p16);

    for (i = 0; i < 8; i++)
        ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, p8);

    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, p4);

    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, p2);

    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_sqr_mont(res, res);
    ecp_nistz256_mul_mont(res, res, in);

    memcpy(r, res, sizeof(res));
}

/*
 * ecp_nistz256_bignum_to_field_elem copies the contents of |in| to |out| and
 * returns one if it fits. Otherwise it returns zero.
 */
static int ecp_nistz256_bignum_to_field_elem(BN_ULONG out[P256_LIMBS],
                                             const BIGNUM *in)
{
    return bn_copy_words(out, in, P256_LIMBS);
}

static int ecp_nistz256_field_elem_to_bignum(BIGNUM *out,
                                             const BN_ULONG in[P256_LIMBS])
{
  if (!bn_wexpand(out, P256_LIMBS)) {
    return 0;
  }
  out->top = P256_LIMBS;
  memcpy(out->d, in, sizeof(BN_ULONG) * P256_LIMBS);
  bn_correct_top(out); // TODO: not constant time?
  return 1;
}


/* r = sum(scalar[i]*point[i]) */
static void ecp_nistz256_windowed_mul(const EC_GROUP *group,
                                      P256_POINT *r,
                                      const BIGNUM **scalar,
                                      const EC_POINT **point,
                                      size_t num, BN_CTX *ctx)
{
    size_t i;
    int j;
    unsigned int idx;
    unsigned char (*p_str)[33] = NULL;
    const unsigned int window_size = 5;
    const unsigned int mask = (1 << (window_size + 1)) - 1;
    unsigned int wvalue;
    P256_POINT *temp;           /* place for 5 temporary points */
    const BIGNUM **scalars = NULL;
    P256_POINT (*table)[16] = NULL;
    void *table_storage = NULL;

    if ((num * 16 + 6) > OPENSSL_MALLOC_MAX_NELEMS(P256_POINT)
        || (table_storage =
            OPENSSL_malloc((num * 16 + 5) * sizeof(P256_POINT) + 64)) == NULL
        || (p_str =
            OPENSSL_malloc(num * 33 * sizeof(unsigned char))) == NULL
        || (scalars = OPENSSL_malloc(num * sizeof(BIGNUM *))) == NULL) {
        OPENSSL_PUT_ERROR(EC, ecp_nistz256_windowed_mul, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    table = (void *)ALIGNPTR(table_storage, 64);
    temp = (P256_POINT *)(table + num);

    for (i = 0; i < num; i++) {
        P256_POINT *row = table[i];

        if ((BN_num_bits(scalar[i]) > 256) || BN_is_negative(scalar[i])) {
            BIGNUM *mod;

            if ((mod = BN_CTX_get(ctx)) == NULL)
                goto err;
            if (!BN_nnmod(mod, scalar[i], &group->order, ctx)) {
                OPENSSL_PUT_ERROR(EC, ecp_nistz256_windowed_mul, ERR_R_BN_LIB);
                goto err;
            }
            scalars[i] = mod;
        } else
            scalars[i] = scalar[i];

        for (j = 0; j < bn_get_top(scalars[i]) * BN_BYTES; j += BN_BYTES) {
            BN_ULONG d = bn_get_words(scalars[i])[j / BN_BYTES];

            p_str[i][j + 0] = (unsigned char)d;
            p_str[i][j + 1] = (unsigned char)(d >> 8);
            p_str[i][j + 2] = (unsigned char)(d >> 16);
            p_str[i][j + 3] = (unsigned char)(d >>= 24);
            if (BN_BYTES == 8) {
                d >>= 8;
                p_str[i][j + 4] = (unsigned char)d;
                p_str[i][j + 5] = (unsigned char)(d >> 8);
                p_str[i][j + 6] = (unsigned char)(d >> 16);
                p_str[i][j + 7] = (unsigned char)(d >> 24);
            }
        }
        for (; j < 33; j++)
            p_str[i][j] = 0;

        if (!ecp_nistz256_bignum_to_field_elem(temp[0].X, &point[i]->X)
            || !ecp_nistz256_bignum_to_field_elem(temp[0].Y, &point[i]->Y)
            || !ecp_nistz256_bignum_to_field_elem(temp[0].Z, &point[i]->Z)) {
            OPENSSL_PUT_ERROR(EC, ecp_nistz256_windowed_mul,
                              EC_R_COORDINATES_OUT_OF_RANGE);
            goto err;
        }

        /*
	 * row[0] is implicitly (0,0,0) (the point at infinity), therefore it
	 * is not stored. All other values are actually stored with an offset
	 * of -1 in table.
         */

        ecp_nistz256_scatter_w5  (row, &temp[0], 1);
        ecp_nistz256_point_double(&temp[1], &temp[0]);              /*1+1=2  */
        ecp_nistz256_scatter_w5  (row, &temp[1], 2);
        ecp_nistz256_point_add   (&temp[2], &temp[1], &temp[0]);    /*2+1=3  */
        ecp_nistz256_scatter_w5  (row, &temp[2], 3);
        ecp_nistz256_point_double(&temp[1], &temp[1]);              /*2*2=4  */
        ecp_nistz256_scatter_w5  (row, &temp[1], 4);
        ecp_nistz256_point_double(&temp[2], &temp[2]);              /*2*3=6  */
        ecp_nistz256_scatter_w5  (row, &temp[2], 6);
        ecp_nistz256_point_add   (&temp[3], &temp[1], &temp[0]);    /*4+1=5  */
        ecp_nistz256_scatter_w5  (row, &temp[3], 5);
        ecp_nistz256_point_add   (&temp[4], &temp[2], &temp[0]);    /*6+1=7  */
        ecp_nistz256_scatter_w5  (row, &temp[4], 7);
        ecp_nistz256_point_double(&temp[1], &temp[1]);              /*2*4=8  */
        ecp_nistz256_scatter_w5  (row, &temp[1], 8);
        ecp_nistz256_point_double(&temp[2], &temp[2]);              /*2*6=12 */
        ecp_nistz256_scatter_w5  (row, &temp[2], 12);
        ecp_nistz256_point_double(&temp[3], &temp[3]);              /*2*5=10 */
        ecp_nistz256_scatter_w5  (row, &temp[3], 10);
        ecp_nistz256_point_double(&temp[4], &temp[4]);              /*2*7=14 */
        ecp_nistz256_scatter_w5  (row, &temp[4], 14);
        ecp_nistz256_point_add   (&temp[2], &temp[2], &temp[0]);    /*12+1=13*/
        ecp_nistz256_scatter_w5  (row, &temp[2], 13);
        ecp_nistz256_point_add   (&temp[3], &temp[3], &temp[0]);    /*10+1=11*/
        ecp_nistz256_scatter_w5  (row, &temp[3], 11);
        ecp_nistz256_point_add   (&temp[4], &temp[4], &temp[0]);    /*14+1=15*/
        ecp_nistz256_scatter_w5  (row, &temp[4], 15);
        ecp_nistz256_point_add   (&temp[2], &temp[1], &temp[0]);    /*8+1=9  */
        ecp_nistz256_scatter_w5  (row, &temp[2], 9);
        ecp_nistz256_point_double(&temp[1], &temp[1]);              /*2*8=16 */
        ecp_nistz256_scatter_w5  (row, &temp[1], 16);
    }

    idx = 255;

    wvalue = p_str[0][(idx - 1) / 8];
    wvalue = (wvalue >> ((idx - 1) % 8)) & mask;

    /*
     * We gather to temp[0], because we know it's position relative
     * to table
     */
    ecp_nistz256_gather_w5(&temp[0], table[0], _booth_recode_w5(wvalue) >> 1);
    memcpy(r, &temp[0], sizeof(temp[0]));

    while (idx >= 5) {
        for (i = (idx == 255 ? 1 : 0); i < num; i++) {
            unsigned int off = (idx - 1) / 8;

            wvalue = p_str[i][off] | p_str[i][off + 1] << 8;
            wvalue = (wvalue >> ((idx - 1) % 8)) & mask;

            wvalue = _booth_recode_w5(wvalue);

            ecp_nistz256_gather_w5(&temp[0], table[i], wvalue >> 1);

            ecp_nistz256_neg(temp[1].Y, temp[0].Y);
            copy_conditional(temp[0].Y, temp[1].Y, (wvalue & 1));

            ecp_nistz256_point_add(r, r, &temp[0]);
        }

        idx -= window_size;

        ecp_nistz256_point_double(r, r);
        ecp_nistz256_point_double(r, r);
        ecp_nistz256_point_double(r, r);
        ecp_nistz256_point_double(r, r);
        ecp_nistz256_point_double(r, r);
    }

    /* Final window */
    for (i = 0; i < num; i++) {
        wvalue = p_str[i][0];
        wvalue = (wvalue << 1) & mask;

        wvalue = _booth_recode_w5(wvalue);

        ecp_nistz256_gather_w5(&temp[0], table[i], wvalue >> 1);

        ecp_nistz256_neg(temp[1].Y, temp[0].Y);
        copy_conditional(temp[0].Y, temp[1].Y, wvalue & 1);

        ecp_nistz256_point_add(r, r, &temp[0]);
    }

 err:
    if (table_storage)
        OPENSSL_free(table_storage);
    if (p_str)
        OPENSSL_free(p_str);
    if (scalars)
        OPENSSL_free(/*const_cast*/(void*)scalars);
}

/* Coordinates of G, for which we have precomputed tables */
const static BN_ULONG def_xG[P256_LIMBS] = {
    TOBN(0x79e730d4, 0x18a9143c), TOBN(0x75ba95fc, 0x5fedb601),
    TOBN(0x79fb732b, 0x77622510), TOBN(0x18905f76, 0xa53755c6)
};

const static BN_ULONG def_yG[P256_LIMBS] = {
    TOBN(0xddf25357, 0xce95560a), TOBN(0x8b4ab8e4, 0xba19e45c),
    TOBN(0xd2e88688, 0xdd21f325), TOBN(0x8571ff18, 0x25885d85)
};


static int ecp_nistz256_mult_precompute(EC_GROUP *group, BN_CTX *ctx)
{
  /* We only use a static precomputed table so there's nothing to precompute
   * on a per-EC_GROUP-instance basis.*/
  (void)group;
  (void)ctx;
  return 1;
}

static int ecp_nistz256_set_from_affine(EC_POINT *out, const EC_GROUP *group,
                                        const P256_POINT_AFFINE *in,
                                        BN_CTX *ctx)
{
    BIGNUM *x, *y;
    BN_ULONG d_x[P256_LIMBS], d_y[P256_LIMBS];
    int ret = 0;

    x = BN_new();
    if (!x)
        return 0;
    y = BN_new();
    if (!y) {
        BN_free(x);
        return 0;
    }
    memcpy(d_x, in->X, sizeof(d_x));
    bn_set_static_words(x, d_x, P256_LIMBS);

    memcpy(d_y, in->Y, sizeof(d_y));
    bn_set_static_words(y, d_y, P256_LIMBS);

    ret = EC_POINT_set_affine_coordinates_GFp(group, out, x, y, ctx);

    if (x)
        BN_free(x);
    if (y)
        BN_free(y);

    return ret;
}


/* r = scalar*G + sum(scalars[i]*points[i]) */
static int ecp_nistz256_points_mul(const EC_GROUP *group,
                                   EC_POINT *r,
                                   const BIGNUM *scalar,
                                   size_t num,
                                   const EC_POINT *points[],
                                   const BIGNUM *scalars[], BN_CTX *ctx)
{
    int i = 0, ret = 0, p_is_infinity = 0;
    size_t j;
    unsigned char p_str[33] = { 0 };
    unsigned int idx = 0;
    const unsigned int window_size = 7;
    const unsigned int mask = (1 << (window_size + 1)) - 1;
    unsigned int wvalue;
    ALIGN32 union {
        P256_POINT p;
        P256_POINT_AFFINE a;
    } t, p;
    BIGNUM *tmp_scalar;

    if ((num + 1) == 0 || (num + 1) > OPENSSL_MALLOC_MAX_NELEMS(void *)) {
        OPENSSL_PUT_ERROR(EC, ecp_nistz256_points_mul, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    if (group->meth != r->meth) {
        OPENSSL_PUT_ERROR(EC, ecp_nistz256_points_mul,
                          EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }

    if ((scalar == NULL) && (num == 0))
        return EC_POINT_set_to_infinity(group, r);

    for (j = 0; j < num; j++) {
        if (group->meth != points[j]->meth) {
            OPENSSL_PUT_ERROR(EC, ecp_nistz256_points_mul,
                              EC_R_INCOMPATIBLE_OBJECTS);
            return 0;
        }
    }

    if (scalar) {

        if ((BN_num_bits(scalar) > 256)
            || BN_is_negative(scalar)) {
            if ((tmp_scalar = BN_CTX_get(ctx)) == NULL)
                goto err;

            // TODO: constant-timedness? p256-64.c has a comment about this.
            if (!BN_nnmod(tmp_scalar, scalar, &group->order, ctx)) {
                OPENSSL_PUT_ERROR(EC, ecp_nistz256_points_mul,
                                  ERR_R_BN_LIB);
                goto err;
            }
            scalar = tmp_scalar;
        }

        for (i = 0; i < bn_get_top(scalar) * BN_BYTES; i += BN_BYTES) {
            BN_ULONG d = bn_get_words(scalar)[i / BN_BYTES];

            p_str[i + 0] = (unsigned char)d;
            p_str[i + 1] = (unsigned char)(d >> 8);
            p_str[i + 2] = (unsigned char)(d >> 16);
            p_str[i + 3] = (unsigned char)(d >>= 24);
            if (BN_BYTES == 8) {
                d >>= 8;
                p_str[i + 4] = (unsigned char)d;
                p_str[i + 5] = (unsigned char)(d >> 8);
                p_str[i + 6] = (unsigned char)(d >> 16);
                p_str[i + 7] = (unsigned char)(d >> 24);
            }
        }

        for (; i < 33; i++)
            p_str[i] = 0;

#if defined(ECP_NISTZ256_AVX2) && defined(OPENSSL_X86_64)
        if (ecp_nistz_avx2_eligible()) {
            ecp_nistz256_avx2_mul_g(&p.p, p_str, ecp_nistz256_precomputed);
        } else
#endif
        {
            /* First window */
            wvalue = (p_str[0] << 1) & mask;
            idx += window_size;

            wvalue = _booth_recode_w7(wvalue);

            ecp_nistz256_gather_w7(&p.a, ecp_nistz256_precomputed[0],
                                   wvalue >> 1);

            ecp_nistz256_neg(p.p.Z, p.p.Y);
            copy_conditional(p.p.Y, p.p.Z, wvalue & 1);

            memcpy(p.p.Z, ONE, sizeof(ONE));

            for (i = 1; i < 37; i++) {
                unsigned int off = (idx - 1) / 8;
                wvalue = p_str[off] | p_str[off + 1] << 8;
                wvalue = (wvalue >> ((idx - 1) % 8)) & mask;
                idx += window_size;

                wvalue = _booth_recode_w7(wvalue);

                ecp_nistz256_gather_w7(&t.a, ecp_nistz256_precomputed[i],
                                       wvalue >> 1);

                ecp_nistz256_neg(t.p.Z, t.a.Y);
                copy_conditional(t.a.Y, t.p.Z, wvalue & 1);

                ecp_nistz256_point_add_affine(&p.p, &p.p, &t.a);
            }
        }
    } else
        p_is_infinity = 1;

    if (num) {
        P256_POINT *out = &t.p;
        if (p_is_infinity)
            out = &p.p;

        ecp_nistz256_windowed_mul(group, out, scalars, points, num, ctx);

        if (!p_is_infinity)
            ecp_nistz256_point_add(&p.p, &p.p, out);
    }

    if (!ecp_nistz256_field_elem_to_bignum(&r->X, p.p.X) ||
        !ecp_nistz256_field_elem_to_bignum(&r->Y, p.p.Y) ||
        !ecp_nistz256_field_elem_to_bignum(&r->Z, p.p.Z)) {
      // TODO: set error code?
      goto err;
    }

    ret = 1;

 err:
    return ret;
}

static int ecp_nistz256_get_affine(const EC_GROUP *group,
                                   const EC_POINT *point,
                                   BIGNUM *x, BIGNUM *y, BN_CTX *ctx)
{
    BN_ULONG z_inv2[P256_LIMBS];
    BN_ULONG z_inv3[P256_LIMBS];
    BN_ULONG x_aff[P256_LIMBS];
    BN_ULONG y_aff[P256_LIMBS];
    BN_ULONG point_x[P256_LIMBS], point_y[P256_LIMBS], point_z[P256_LIMBS];

    if (EC_POINT_is_at_infinity(group, point)) {
        OPENSSL_PUT_ERROR(EC, ecp_nistz256_get_affine,
                          EC_R_POINT_AT_INFINITY);
        return 0;
    }

    if (!ecp_nistz256_bignum_to_field_elem(point_x, &point->X) ||
        !ecp_nistz256_bignum_to_field_elem(point_y, &point->Y) ||
        !ecp_nistz256_bignum_to_field_elem(point_z, &point->Z)) {
        OPENSSL_PUT_ERROR(EC, ecp_nistz256_get_affine,
                          EC_R_COORDINATES_OUT_OF_RANGE);
        return 0;
    }

    ecp_nistz256_mod_inverse(z_inv3, point_z);
    ecp_nistz256_sqr_mont(z_inv2, z_inv3);
    ecp_nistz256_mul_mont(x_aff, z_inv2, point_x);

    if (x != NULL) {
        if (!bn_wexpand(x, P256_LIMBS)) {
          return 0;
        }
        x->top = P256_LIMBS;
        ecp_nistz256_from_mont(bn_get_words(x), x_aff);
        bn_correct_top(x); // TODO: not constant time?
    }

    if (y != NULL) {
        ecp_nistz256_mul_mont(z_inv3, z_inv3, z_inv2);
        ecp_nistz256_mul_mont(y_aff, z_inv3, point_y);
        if (!bn_wexpand(y, P256_LIMBS)) {
          return 0;
        }
        y->top = P256_LIMBS;
        ecp_nistz256_from_mont(bn_get_words(y), y_aff);
        bn_correct_top(y); // todo: not constant time?
    }

    return 1;
}

const EC_METHOD *EC_GFp_nistz256_method(void)
{
    static const EC_METHOD ret = {
        ec_GFp_mont_group_extra_finish,
        ec_GFp_mont_group_extra_copy,
        ec_GFp_mont_group_set_curve,
        ecp_nistz256_get_affine,
        ecp_nistz256_points_mul,                    /* mul */
        ecp_nistz256_mult_precompute,               /* precompute_mult */
        ec_GFp_mont_field_mul,
        ec_GFp_mont_field_sqr,
        ec_GFp_mont_field_encode,
        ec_GFp_mont_field_decode,
        ec_GFp_mont_field_set_to_one
    };

    return &ret;
}
