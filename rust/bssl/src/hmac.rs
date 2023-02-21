/* Copyright (c) 2023, Google Inc.
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
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
use crate::{
    digest::{Md, Sha256, Sha512},
    PanicResultHandler,
};
use bssl_sys::HMAC_CTX;
use core::{marker::PhantomData, ptr};
use foreign_types::ForeignTypeRef;
use libc::{c_uint, c_void, size_t};

/// One shot Hmac SHA-256 operation
pub fn hmac_sha_256(key: &[u8], data: &[u8]) -> Result<[u8; 32], InvalidLength> {
    hmac::<32, Sha256>(key, data)
}

/// One shot Hmac SHA-512 operation
pub fn hmac_sha_512(key: &[u8], data: &[u8]) -> Result<[u8; 64], InvalidLength> {
    hmac::<64, Sha512>(key, data)
}

/// Hmac SHA-256 impl
pub struct HmacSha256(HmacImpl<32, Sha256>);

/// Hmac SHA-512 impl
pub struct HmacSha512(HmacImpl<64, Sha512>);

/// Error output when the provided key material length is invalid
#[derive(Debug)]
pub struct InvalidLength;

/// Error type for when the output of the hmac operation is not equal to the expected value.
#[derive(Debug)]
pub struct MacError;

/// Computes the HMAC as a one-shot operation.
///
/// Calculates the HMAC of data, using the given |key|
/// and with the provided generic hash function |H| and returns the result.
/// It returns the computed hmac or `InvalidLength` of the input key size is too large
#[inline]
fn hmac<const N: usize, M: Md>(key: &[u8], data: &[u8]) -> Result<[u8; N], InvalidLength> {
    let mut buf = [0_u8; N];
    let mut size: c_uint = 0;
    unsafe {
        bssl_sys::HMAC(
            M::get_md().as_ptr(),
            key.as_ptr() as *const c_void,
            key.len(),
            data.as_ptr(),
            data.len(),
            buf.as_mut_ptr(),
            &mut size as *mut c_uint,
        )
    }
    .panic_if_error();
    Ok(buf)
}

/// Trait which defines hmac operations where N is the output size
pub trait Hmac<const N: usize>: Sized {
    /// Create a new hmac from a fixed size key
    fn new(key: [u8; N]) -> Self;

    /// Create new hmac value from variable size key.
    fn new_from_slice(key: &[u8]) -> Result<Self, InvalidLength>;

    /// Update state using the provided data
    fn update(&mut self, data: &[u8]);

    /// Obtain the hmac computation consuming the hmac instance
    fn finalize(self) -> [u8; N];

    /// Check that the tag value is correct for the processed input
    fn verify_slice(self, tag: &[u8]) -> Result<(), MacError>;

    /// Check that the tag value is correct for the processed input
    fn verify(self, tag: [u8; N]) -> Result<(), MacError>;

    /// Check truncated tag correctness using left side bytes of the calculated tag
    fn verify_truncated_left(self, tag: &[u8]) -> Result<(), MacError>;
}

impl Hmac<32> for HmacSha256 {
    fn new(key: [u8; 32]) -> Self {
        Self(HmacImpl::new(key))
    }

    fn new_from_slice(key: &[u8]) -> Result<Self, InvalidLength> {
        HmacImpl::new_from_slice(key).map(Self)
    }

    fn update(&mut self, data: &[u8]) {
        self.0.update(data)
    }

    fn finalize(self) -> [u8; 32] {
        self.0.finalize()
    }

    fn verify_slice(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_slice(tag)
    }

    fn verify(self, tag: [u8; 32]) -> Result<(), MacError> {
        self.0.verify(tag)
    }

    fn verify_truncated_left(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_truncated_left(tag)
    }
}

impl Hmac<64> for HmacSha512 {
    fn new(key: [u8; 64]) -> Self {
        Self(HmacImpl::new(key))
    }

    fn new_from_slice(key: &[u8]) -> Result<Self, InvalidLength> {
        HmacImpl::new_from_slice(key).map(Self)
    }

    fn update(&mut self, data: &[u8]) {
        self.0.update(data)
    }

    fn finalize(self) -> [u8; 64] {
        self.0.finalize()
    }

    fn verify_slice(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_slice(tag)
    }

    fn verify(self, tag: [u8; 64]) -> Result<(), MacError> {
        self.0.verify(tag)
    }

    fn verify_truncated_left(self, tag: &[u8]) -> Result<(), MacError> {
        self.0.verify_truncated_left(tag)
    }
}

/// boringssl implementation of hmac given a generic hash function and a length, where length is the
/// output size of the hash function
/// Note: Until the Rust language can support the `min_const_generics` feature we will have to
/// pass both: https://github.com/rust-lang/rust/issues/60551
struct HmacImpl<const N: usize, M: Md> {
    ctx: *mut HMAC_CTX,
    _marker: PhantomData<M>,
}

impl<const N: usize, M: Md> HmacImpl<N, M> {
    /// Infallible hmac creation from a fixed length key
    fn new(key: [u8; N]) -> Self {
        #[allow(clippy::expect_used)]
        Self::new_from_slice(&key).expect("output length of hash is always a valid hmac key size")
    }

    /// Create new hmac value from variable size key. Panics on allocation failure
    /// returns InvalidLength if the key length is greater than the max message digest block size
    fn new_from_slice(key: &[u8]) -> Result<Self, InvalidLength> {
        (validate_key_len(key.len()))
            .then(|| {
                // Safety:
                // - HMAC_CTX_new panics if allocation fails
                let ctx = unsafe { bssl_sys::HMAC_CTX_new() };
                ctx.panic_if_error();

                // Safety:
                // - HMAC_Init_ex must be called with a context previously created with HMAC_CTX_new,
                //   which is the line above.
                // - HMAC_Init_ex may return an error if key is null but the md is different from
                //   before. This is avoided here since key is guaranteed to be non-null.
                // - HMAC_Init_ex returns 0 on allocation failure in which case we panic
                unsafe {
                    bssl_sys::HMAC_Init_ex(
                        ctx,
                        key.as_ptr() as *const c_void,
                        key.len(),
                        M::get_md().as_ptr(),
                        ptr::null_mut(),
                    )
                }
                .panic_if_error();

                Self {
                    ctx,
                    _marker: Default::default(),
                }
            })
            .ok_or(InvalidLength)
    }

    /// Update state using the provided data, can be called repeatedly
    fn update(&mut self, data: &[u8]) {
        unsafe {
            // Safety: HMAC_Update will always return 1, in case it doesnt we panic
            bssl_sys::HMAC_Update(self.ctx, data.as_ptr(), data.len())
        }
        .panic_if_error()
    }

    /// Obtain the hmac computation consuming the hmac instance
    fn finalize(self) -> [u8; N] {
        let mut buf = [0_u8; N];
        let mut size: c_uint = 0;
        // Safety:
        // - hmac has a fixed size output of N which will never exceed the length of an N
        // length array
        // - on allocation failure we panic
        unsafe { bssl_sys::HMAC_Final(self.ctx, buf.as_mut_ptr(), &mut size as *mut c_uint) }
            .panic_if_error();
        buf
    }

    /// Check that the tag value is correct for the processed input
    fn verify(self, tag: [u8; N]) -> Result<(), MacError> {
        self.verify_slice(&tag)
    }

    /// Check truncated tag correctness using all bytes
    /// of calculated tag.
    ///
    /// Returns `Error` if `tag` is not valid or not equal in length
    /// to MAC's output.
    fn verify_slice(self, tag: &[u8]) -> Result<(), MacError> {
        tag.len().eq(&N).then_some(()).ok_or(MacError)?;
        self.verify_truncated_left(tag)
    }

    /// Check truncated tag correctness using left side bytes
    /// (i.e. `tag[..n]`) of calculated tag.
    ///
    /// Returns `Error` if `tag` is not valid or empty.
    fn verify_truncated_left(self, tag: &[u8]) -> Result<(), MacError> {
        let len = tag.len();
        if len == 0 || len > N {
            return Err(MacError);
        }

        let result = &self.finalize()[..len];

        // Safety:
        // - TODO:
        unsafe {
            bssl_sys::CRYPTO_memcmp(
                result.as_ptr() as *const c_void,
                tag.as_ptr() as *const c_void,
                result.len() as size_t,
            )
        }
        .eq(&0)
        .then_some(())
        .ok_or(MacError)
    }
}

impl<const N: usize, M: Md> Drop for HmacImpl<N, M> {
    fn drop(&mut self) {
        unsafe { bssl_sys::HMAC_CTX_free(self.ctx) }
    }
}

// make sure key len is within a valid range
fn validate_key_len(len: usize) -> bool {
    if len > bssl_sys::EVP_MAX_MD_BLOCK_SIZE as usize {
        return false;
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::digest::{Sha256, Sha512};
    use std::cmp::min;
    use wycheproof::TestResult;

    #[test]
    fn hmac_sha256_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];

        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";

        let mut hmac = HmacSha256::new_from_slice(&key).expect("length is valid");
        hmac.update(data);
        let hmac_result: [u8; 32] = hmac.finalize();

        // let hmac_result =
        //     hmac(Md::sha256(), &key, data, &mut out).expect("Couldn't calculate sha256 hmac");
        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_fixed_size_key_test() {
        let expected_hmac = [
            0x19, 0x8a, 0x60, 0x7e, 0xb4, 0x4b, 0xfb, 0xc6, 0x99, 0x3, 0xa0, 0xf1, 0xcf, 0x2b,
            0xbd, 0xc5, 0xba, 0xa, 0xa3, 0xf3, 0xd9, 0xae, 0x3c, 0x1c, 0x7a, 0x3b, 0x16, 0x96,
            0xa0, 0xb6, 0x8c, 0xf7,
        ];

        let key: [u8; 32] = [0x0b; 32];
        let data = b"Hi There";

        let mut hmac = HmacSha256::new(key);
        hmac.update(data);
        let hmac_result: [u8; 32] = hmac.finalize();
        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_update_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";
        let mut hmac: HmacSha256 = Hmac::new_from_slice(&key).expect("");
        hmac.update(data);
        let result = hmac.finalize();
        assert_eq!(&result, &expected_hmac);
        assert_eq!(result.len(), 32);
    }

    #[test]
    fn hmac_sha256_test_big_buffer() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";
        let hmac_result = hmac_sha_256(&key, data).expect("Couldn't calculate sha256 hmac");
        assert_eq!(&hmac_result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_update_chunks_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let mut hmac = HmacSha256::new_from_slice(&key).expect("key is valid length");
        hmac.update(b"Hi");
        hmac.update(b" There");
        let result = hmac.finalize();
        assert_eq!(&result, &expected_hmac);
    }

    #[test]
    fn hmac_sha256_verify_test() {
        let expected_hmac = [
            0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0xb,
            0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x0, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c,
            0x2e, 0x32, 0xcf, 0xf7,
        ];
        let key: [u8; 20] = [0x0b; 20];
        let data = b"Hi There";
        let mut hmac: HmacSha256 = Hmac::new_from_slice(&key).expect("");
        hmac.update(data);
        assert!(hmac.verify(expected_hmac).is_ok())
    }

    #[test]
    fn hmac_sha_256_wycheproof_test_vectors() {
        run_hmac_test_vectors::<32, Sha256>(HashAlg::Sha256);
    }

    #[test]
    fn hmac_sha_512_wycheproof_test_vectors() {
        run_hmac_test_vectors::<64, Sha512>(HashAlg::Sha512);
    }

    enum HashAlg {
        Sha256,
        Sha512,
    }

    // Tests vectors from Project Wycheproof:
    // https://github.com/google/wycheproof
    fn run_hmac_test_vectors<const N: usize, M: Md>(hash: HashAlg) {
        let test_name = match hash {
            HashAlg::Sha256 => wycheproof::mac::TestName::HmacSha256,
            HashAlg::Sha512 => wycheproof::mac::TestName::HmacSha512,
        };

        let test_set =
            wycheproof::mac::TestSet::load(test_name).expect("should be able to load test set");

        for test_group in test_set.test_groups {
            for test in test_group.tests {
                let key = test.key;
                let msg = test.msg;
                let tag = test.tag;
                let tc_id = test.tc_id;
                let valid = match test.result {
                    TestResult::Valid | TestResult::Acceptable => true,
                    TestResult::Invalid => false,
                };

                if let Some(desc) =
                    run_test::<N, M>(key.as_slice(), msg.as_slice(), tag.as_slice(), valid)
                {
                    panic!(
                        "\n\
                         Failed test {tc_id}: {desc}\n\
                         key:\t{key:?}\n\
                         msg:\t{msg:?}\n\
                         tag:\t{tag:?}\n",
                    );
                }
            }
        }
    }

    fn run_test<const N: usize, M: Md>(
        key: &[u8],
        input: &[u8],
        tag: &[u8],
        valid_data: bool,
    ) -> Option<&'static str> {
        let mut mac: HmacImpl<N, M> = HmacImpl::new_from_slice(key).unwrap();
        mac.update(input);
        let result = mac.finalize();
        let n = tag.len();
        let result_bytes = &result[..n];

        if valid_data {
            if result_bytes != tag {
                return Some("whole message");
            }
        } else {
            return if result_bytes == tag {
                Some("invalid should not match")
            } else {
                None
            };
        }

        // test reading different chunk sizes
        for chunk_size in 1..min(64, input.len()) {
            let mut mac: HmacImpl<N, M> = HmacImpl::new_from_slice(key).unwrap();
            for chunk in input.chunks(chunk_size) {
                mac.update(chunk);
            }
            let res = mac.verify_truncated_left(tag);
            if res.is_err() {
                return Some("chunked message");
            }
        }
        None
    }
}
