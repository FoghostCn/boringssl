package main

import (
	"crypto/hmac"
	"crypto/sha256"
)

// See SP 800-90Ar1, section 10.1.2
type HMACDRBGSHA256 struct {
	k, v [32]byte
}

func NewHMACDRBG(entropy, nonce, personalisation []byte) *HMACDRBGSHA256 {
	ret := new(HMACDRBGSHA256)
	ret.init(entropy, nonce, personalisation)
	return ret
}

func (drbg *HMACDRBGSHA256) init(entropy, nonce, personalisation []byte) {
	for i := range drbg.k {
		drbg.k[i] = 0
	}
	for i := range drbg.v {
		drbg.v[i] = 1
	}

	seed := make([]byte, 0, len(entropy)+len(nonce)+len(personalisation))
	seed = append(seed, entropy...)
	seed = append(seed, nonce...)
	seed = append(seed, personalisation...)
	drbg.update(seed)
}

func (drbg *HMACDRBGSHA256) update(data []byte) {
	buf := make([]byte, 0, len(drbg.v)+1+len(data))
	buf = append(buf, drbg.v[:]...)
	buf = append(buf, 0)
	buf = append(buf, data...)

	mac := hmac.New(sha256.New, drbg.k[:])
	mac.Write(buf)
	mac.Sum(drbg.k[:0])

	mac = hmac.New(sha256.New, drbg.k[:])
	mac.Write(drbg.v[:])
	mac.Sum(drbg.v[:0])

	if len(data) > 0 {
		copy(buf, drbg.v[:])
		buf[len(drbg.v)] = 1

		mac = hmac.New(sha256.New, drbg.k[:])
		mac.Write(buf)
		mac.Sum(drbg.k[:0])

		mac = hmac.New(sha256.New, drbg.k[:])
		mac.Write(drbg.v[:])
		mac.Sum(drbg.v[:0])
	}
}

func (drbg *HMACDRBGSHA256) Reseed(entropy, additionalInput []byte) {
	buf := make([]byte, 0, len(entropy)+len(additionalInput))
	buf = append(buf, entropy...)
	buf = append(buf, additionalInput...)
	drbg.update(buf)
}

func (drbg *HMACDRBGSHA256) Generate(out []byte, additionalInput []byte) {
	if len(additionalInput) > 0 {
		drbg.update(additionalInput)
	}

	done := 0
	for done < len(out) {
		mac := hmac.New(sha256.New, drbg.k[:])
		mac.Write(drbg.v[:])
		mac.Sum(drbg.v[:0])

		remaining := len(out) - done
		if remaining > len(drbg.v) {
			remaining = len(drbg.v)
		}
		copy(out[done:], drbg.v[:remaining])
		done += remaining
	}

	drbg.update(additionalInput)
}
