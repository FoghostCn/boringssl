#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

// populated by cmake
${INCLUDES}

pub fn init() {
    unsafe { CRYPTO_library_init(); }
}
