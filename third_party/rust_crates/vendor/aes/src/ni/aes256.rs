use super::{
    arch::*,
    utils::{aesdec8, aesdeclast8, aesenc8, aesenclast8, load8, store8, xor8, U128x8},
};
use crate::{Block, ParBlocks};
use cipher::{
    consts::{U16, U32, U8},
    generic_array::GenericArray,
    BlockCipher, BlockDecrypt, BlockEncrypt, NewBlockCipher,
};

mod expand;
#[cfg(test)]
mod test_expand;

/// AES-256 round keys
type RoundKeys = [__m128i; 15];

/// AES-256 block cipher
#[derive(Clone)]
pub struct Aes256 {
    encrypt_keys: RoundKeys,
    decrypt_keys: RoundKeys,
}

impl Aes256 {
    #[inline(always)]
    pub(crate) fn encrypt8(&self, mut blocks: U128x8) -> U128x8 {
        #[inline]
        #[target_feature(enable = "aes")]
        unsafe fn aesni256_encrypt8(keys: &RoundKeys, blocks: &mut U128x8) {
            xor8(blocks, keys[0]);
            aesenc8(blocks, keys[1]);
            aesenc8(blocks, keys[2]);
            aesenc8(blocks, keys[3]);
            aesenc8(blocks, keys[4]);
            aesenc8(blocks, keys[5]);
            aesenc8(blocks, keys[6]);
            aesenc8(blocks, keys[7]);
            aesenc8(blocks, keys[8]);
            aesenc8(blocks, keys[9]);
            aesenc8(blocks, keys[10]);
            aesenc8(blocks, keys[11]);
            aesenc8(blocks, keys[12]);
            aesenc8(blocks, keys[13]);
            aesenclast8(blocks, keys[14]);
        }
        unsafe { aesni256_encrypt8(&self.encrypt_keys, &mut blocks) };
        blocks
    }

    #[inline(always)]
    pub(crate) fn encrypt(&self, block: __m128i) -> __m128i {
        #[inline]
        #[target_feature(enable = "aes")]
        unsafe fn aesni256_encrypt1(keys: &RoundKeys, mut block: __m128i) -> __m128i {
            block = _mm_xor_si128(block, keys[0]);
            block = _mm_aesenc_si128(block, keys[1]);
            block = _mm_aesenc_si128(block, keys[2]);
            block = _mm_aesenc_si128(block, keys[3]);
            block = _mm_aesenc_si128(block, keys[4]);
            block = _mm_aesenc_si128(block, keys[5]);
            block = _mm_aesenc_si128(block, keys[6]);
            block = _mm_aesenc_si128(block, keys[7]);
            block = _mm_aesenc_si128(block, keys[8]);
            block = _mm_aesenc_si128(block, keys[9]);
            block = _mm_aesenc_si128(block, keys[10]);
            block = _mm_aesenc_si128(block, keys[11]);
            block = _mm_aesenc_si128(block, keys[12]);
            block = _mm_aesenc_si128(block, keys[13]);
            _mm_aesenclast_si128(block, keys[14])
        }
        unsafe { aesni256_encrypt1(&self.encrypt_keys, block) }
    }
}

impl NewBlockCipher for Aes256 {
    type KeySize = U32;

    #[inline]
    fn new(key: &GenericArray<u8, U32>) -> Self {
        let key = unsafe { &*(key as *const _ as *const [u8; 32]) };
        let (encrypt_keys, decrypt_keys) = expand::expand(key);
        Self {
            encrypt_keys,
            decrypt_keys,
        }
    }
}

impl BlockCipher for Aes256 {
    type BlockSize = U16;
    type ParBlocks = U8;
}

impl BlockEncrypt for Aes256 {
    #[inline]
    fn encrypt_block(&self, block: &mut Block) {
        // Safety: `loadu` and `storeu` support unaligned access
        #[allow(clippy::cast_ptr_alignment)]
        unsafe {
            let b = _mm_loadu_si128(block.as_ptr() as *const __m128i);
            let b = self.encrypt(b);
            _mm_storeu_si128(block.as_mut_ptr() as *mut __m128i, b);
        }
    }

    #[inline]
    fn encrypt_par_blocks(&self, blocks: &mut ParBlocks) {
        let b = self.encrypt8(load8(blocks));
        store8(blocks, b);
    }
}

impl BlockDecrypt for Aes256 {
    #[inline]
    fn decrypt_block(&self, block: &mut Block) {
        #[inline]
        #[target_feature(enable = "aes")]
        unsafe fn aes256_decrypt1(block: &mut Block, keys: &RoundKeys) {
            // Safety: `loadu` and `storeu` support unaligned access
            #[allow(clippy::cast_ptr_alignment)]
            let mut b = _mm_loadu_si128(block.as_ptr() as *const __m128i);

            b = _mm_xor_si128(b, keys[14]);
            b = _mm_aesdec_si128(b, keys[13]);
            b = _mm_aesdec_si128(b, keys[12]);
            b = _mm_aesdec_si128(b, keys[11]);
            b = _mm_aesdec_si128(b, keys[10]);
            b = _mm_aesdec_si128(b, keys[9]);
            b = _mm_aesdec_si128(b, keys[8]);
            b = _mm_aesdec_si128(b, keys[7]);
            b = _mm_aesdec_si128(b, keys[6]);
            b = _mm_aesdec_si128(b, keys[5]);
            b = _mm_aesdec_si128(b, keys[4]);
            b = _mm_aesdec_si128(b, keys[3]);
            b = _mm_aesdec_si128(b, keys[2]);
            b = _mm_aesdec_si128(b, keys[1]);
            b = _mm_aesdeclast_si128(b, keys[0]);

            // Safety: `loadu` and `storeu` support unaligned access
            #[allow(clippy::cast_ptr_alignment)]
            _mm_storeu_si128(block.as_mut_ptr() as *mut __m128i, b);
        }

        unsafe { aes256_decrypt1(block, &self.decrypt_keys) }
    }

    #[inline]
    fn decrypt_par_blocks(&self, blocks: &mut ParBlocks) {
        #[inline]
        #[target_feature(enable = "aes")]
        unsafe fn aes256_decrypt8(blocks: &mut ParBlocks, keys: &RoundKeys) {
            let mut b = load8(blocks);
            xor8(&mut b, keys[14]);
            aesdec8(&mut b, keys[13]);
            aesdec8(&mut b, keys[12]);
            aesdec8(&mut b, keys[11]);
            aesdec8(&mut b, keys[10]);
            aesdec8(&mut b, keys[9]);
            aesdec8(&mut b, keys[8]);
            aesdec8(&mut b, keys[7]);
            aesdec8(&mut b, keys[6]);
            aesdec8(&mut b, keys[5]);
            aesdec8(&mut b, keys[4]);
            aesdec8(&mut b, keys[3]);
            aesdec8(&mut b, keys[2]);
            aesdec8(&mut b, keys[1]);
            aesdeclast8(&mut b, keys[0]);
            store8(blocks, b);
        }

        unsafe { aes256_decrypt8(blocks, &self.decrypt_keys) }
    }
}

opaque_debug::implement!(Aes256);
