/********************************************************************************************
* SIDH: an efficient supersingular isogeny cryptography library
*
* Abstract: internal header file for P503
*********************************************************************************************/

#ifndef UTILS_H_
#define UTILS_H_

#include <stdbool.h>
#include "openssl/base.h"
#include "../crypto/internal.h"
#include "sike/sike.h"

// Conversion macro from number of bits to number of bytes
#define BITS_TO_BYTES(nbits)      (((nbits)+7)/8)

// Bit size of the field
#define BITS_FIELD             503
// Byte size of the field
#define FIELD_BYTESZ            BITS_TO_BYTES(BITS_FIELD)
// Number of 64-bit words of a 503-bit field element
#define NWORDS64_FIELD          ((BITS_FIELD+63)/64)
// Number of 64-bit words of a 256-bit element
#define NBITS_ORDER             256
#define NWORDS64_ORDER          ((NBITS_ORDER+63)/64)
// Number of elements in Alice's strategy
#define A_max                   125
// Number of elements in Bob's strategy
#define B_max                   159
// Word size size
#define RADIX                   sizeof(crypto_word_t)*8
// Byte size of a limb
#define LSZ                     sizeof(crypto_word_t)

#if defined(OPENSSL_64_BIT)
    // Number of words of a 503-bit field element
    #define NWORDS_FIELD    8
    // Number of "0" digits in the least significant part of p503 + 1
    #define p503_ZERO_WORDS 3
    // log_2(RADIX)
    #define LOG2RADIX       6
#else
    // Number of words of a 503-bit field element
    #define NWORDS_FIELD    16
    // Number of "0" digits in the least significant part of p503 + 1
    #define p503_ZERO_WORDS 7
    // log_2(RADIX)
    #define LOG2RADIX       5
#endif

// Extended datatype support
#if !defined(BORINGSSL_HAS_UINT128)
    typedef uint64_t uint128_t[2];
#endif

// The following functions return 1 (TRUE) if condition is true, 0 (FALSE) otherwise
// Digit multiplication
#define MUL(multiplier, multiplicand, hi, lo) digit_x_digit((multiplier), (multiplicand), &(lo));

// If mask |x|==0xff.ff set |x| to 1, otherwise 0
#define M2B(x) ((x)>>(RADIX-1))

// Digit addition with carry
#define ADDC(carryIn, addend1, addend2, carryOut, sumOut)                   \
do {                                                                        \
  crypto_word_t tempReg = (addend1) + (crypto_word_t)(carryIn);             \
  (sumOut) = (addend2) + tempReg;                                           \
  (carryOut) = M2B(constant_time_lt_w(tempReg, (crypto_word_t)(carryIn)) |  \
                   constant_time_lt_w((sumOut), tempReg));                  \
} while(0)

// Digit subtraction with borrow
#define SUBC(borrowIn, minuend, subtrahend, borrowOut, differenceOut)           \
do {                                                                            \
    crypto_word_t tempReg = (minuend) - (subtrahend);                           \
    crypto_word_t borrowReg = M2B(constant_time_lt_w((minuend), (subtrahend))); \
    borrowReg |= ((borrowIn) & constant_time_is_zero_w(tempReg));               \
    (differenceOut) = tempReg - (crypto_word_t)(borrowIn);                      \
    (borrowOut) = borrowReg;                                                    \
} while(0)

/* Old GCC 4.9 (jessie) doesn't implement {0} initialization properly,
   which violates C11 as described in 6.7.9, 21 (similarily C99, 6.7.8).
   Defines below are used to work around the bug, and provide a way
   to initialize f2elem_t and point_proj_t structs.
   Bug has been fixed in GCC6 (debian stretch).
*/
#define F2ELM_INIT {{ {0}, {0} }}
#define POINT_PROJ_INIT {{ F2ELM_INIT, F2ELM_INIT }}

// Datatype for representing 503-bit field elements (512-bit max.)
// Elements over GF(p503) are encoded in 63 octets in little endian format
// (i.e., the least significant octet is located in the lowest memory address).
typedef crypto_word_t felm_t[NWORDS_FIELD];

// An element in F_{p^2}, is composed of two coefficients from F_p, * i.e.
// Fp2 element = c0 + c1*i in F_{p^2}
// Datatype for representing double-precision 2x503-bit field elements (512-bit max.)
// Elements (a+b*i) over GF(p503^2), where a and b are defined over GF(p503), are
// encoded as {a, b}, with a in the lowest memory portion.
typedef struct {
    felm_t c0;
    felm_t c1;
} fp2;

// Our F_{p^2} element type is a pointer to the struct.
typedef fp2 f2elm_t[1];

// Datatype for representing double-precision 2x503-bit
// field elements in contiguous memory.
typedef crypto_word_t dfelm_t[2*NWORDS_FIELD];

// Constants used during SIKEp503 computation.
struct params_t {
    // Stores P503 prime
    const uint64_t prime[NWORDS64_FIELD];
    // Stores P503 + 1
    const uint64_t prime_p1[NWORDS64_FIELD];
    // Stores P503 * 2
    const uint64_t prime_x2[NWORDS64_FIELD];
    // Alice's generator values {XPA0 + XPA1*i, XQA0, XRA0 + XRA1*i}
    // in GF(p503^2), expressed in Montgomery representation
    const uint64_t A_gen[5*NWORDS64_FIELD];
    // Bob's generator values {XPB0 + XPB1*i, XQB0, XRB0 + XRB1*i}
    // in GF(p503^2), expressed in Montgomery representation
    const uint64_t B_gen[5*NWORDS64_FIELD];
    // Montgomery constant mont_R2 = (2^512)^2 mod p503
    const uint64_t mont_R2[NWORDS64_FIELD];
    // Value 'one' in Montgomery representation
    const uint64_t mont_one[NWORDS64_FIELD];
    // Fixed parameters for isogeny tree computation
    const unsigned int A_strat[A_max-1];
    const unsigned int B_strat[B_max-1];
};

// Point representation in projective XZ Montgomery coordinates.
typedef struct {
    f2elm_t X;
    f2elm_t Z;
} point_proj;
typedef point_proj point_proj_t[1];

#endif // UTILS_H_
