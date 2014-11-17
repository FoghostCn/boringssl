// Copyright 2012 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package main

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/hmac"
	"crypto/sha256"
	"crypto/subtle"
	"errors"
	"io"
)

// ServerSessionState contains the information that is serialized into
// a session ticket in order to later resume a connection.
type ServerSessionState struct {
	vers                 uint16
	cipherSuite          uint16
	masterSecret         []byte
	handshakeHash        []byte
	certificates         [][]byte
	extendedMasterSecret bool
}

func (s *ServerSessionState) equal(i interface{}) bool {
	s1, ok := i.(*ServerSessionState)
	if !ok {
		return false
	}

	if s.vers != s1.vers ||
		s.cipherSuite != s1.cipherSuite ||
		!bytes.Equal(s.masterSecret, s1.masterSecret) ||
		!bytes.Equal(s.handshakeHash, s1.handshakeHash) ||
		s.extendedMasterSecret != s1.extendedMasterSecret {
		return false
	}

	if len(s.certificates) != len(s1.certificates) {
		return false
	}

	for i := range s.certificates {
		if !bytes.Equal(s.certificates[i], s1.certificates[i]) {
			return false
		}
	}

	return true
}

func (s *ServerSessionState) marshal() []byte {
	length := 2 + 2 + 2 + len(s.masterSecret) + 2 + len(s.handshakeHash) + 2
	for _, cert := range s.certificates {
		length += 4 + len(cert)
	}
	length++

	ret := make([]byte, length)
	x := ret
	x[0] = byte(s.vers >> 8)
	x[1] = byte(s.vers)
	x[2] = byte(s.cipherSuite >> 8)
	x[3] = byte(s.cipherSuite)
	x[4] = byte(len(s.masterSecret) >> 8)
	x[5] = byte(len(s.masterSecret))
	x = x[6:]
	copy(x, s.masterSecret)
	x = x[len(s.masterSecret):]

	x[0] = byte(len(s.handshakeHash) >> 8)
	x[1] = byte(len(s.handshakeHash))
	x = x[2:]
	copy(x, s.handshakeHash)
	x = x[len(s.handshakeHash):]

	x[0] = byte(len(s.certificates) >> 8)
	x[1] = byte(len(s.certificates))
	x = x[2:]

	for _, cert := range s.certificates {
		x[0] = byte(len(cert) >> 24)
		x[1] = byte(len(cert) >> 16)
		x[2] = byte(len(cert) >> 8)
		x[3] = byte(len(cert))
		copy(x[4:], cert)
		x = x[4+len(cert):]
	}

	if s.extendedMasterSecret {
		x[0] = 1
	}
	x = x[1:]

	return ret
}

func (s *ServerSessionState) unmarshal(data []byte) bool {
	if len(data) < 8 {
		return false
	}

	s.vers = uint16(data[0])<<8 | uint16(data[1])
	s.cipherSuite = uint16(data[2])<<8 | uint16(data[3])
	masterSecretLen := int(data[4])<<8 | int(data[5])
	data = data[6:]
	if len(data) < masterSecretLen {
		return false
	}

	s.masterSecret = data[:masterSecretLen]
	data = data[masterSecretLen:]

	if len(data) < 2 {
		return false
	}

	handshakeHashLen := int(data[0])<<8 | int(data[1])
	data = data[2:]
	if len(data) < handshakeHashLen {
		return false
	}

	s.handshakeHash = data[:handshakeHashLen]
	data = data[handshakeHashLen:]

	if len(data) < 2 {
		return false
	}

	numCerts := int(data[0])<<8 | int(data[1])
	data = data[2:]

	s.certificates = make([][]byte, numCerts)
	for i := range s.certificates {
		if len(data) < 4 {
			return false
		}
		certLen := int(data[0])<<24 | int(data[1])<<16 | int(data[2])<<8 | int(data[3])
		data = data[4:]
		if certLen < 0 {
			return false
		}
		if len(data) < certLen {
			return false
		}
		s.certificates[i] = data[:certLen]
		data = data[certLen:]
	}

	if len(data) < 1 {
		return false
	}

	s.extendedMasterSecret = false
	if data[0] == 1 {
		s.extendedMasterSecret = true
	}
	data = data[1:]

	if len(data) > 0 {
		return false
	}

	return true
}

func (c *Conn) encryptTicket(state *ServerSessionState) ([]byte, error) {
	serialized := state.marshal()
	encrypted := make([]byte, aes.BlockSize+len(serialized)+sha256.Size)
	iv := encrypted[:aes.BlockSize]
	macBytes := encrypted[len(encrypted)-sha256.Size:]

	if _, err := io.ReadFull(c.config.rand(), iv); err != nil {
		return nil, err
	}
	block, err := aes.NewCipher(c.config.SessionTicketKey[:16])
	if err != nil {
		return nil, errors.New("tls: failed to create cipher while encrypting ticket: " + err.Error())
	}
	cipher.NewCTR(block, iv).XORKeyStream(encrypted[aes.BlockSize:], serialized)

	mac := hmac.New(sha256.New, c.config.SessionTicketKey[16:32])
	mac.Write(encrypted[:len(encrypted)-sha256.Size])
	mac.Sum(macBytes[:0])

	return encrypted, nil
}

func (c *Conn) decryptTicket(encrypted []byte) (*ServerSessionState, bool) {
	if len(encrypted) < aes.BlockSize+sha256.Size {
		return nil, false
	}

	iv := encrypted[:aes.BlockSize]
	macBytes := encrypted[len(encrypted)-sha256.Size:]

	mac := hmac.New(sha256.New, c.config.SessionTicketKey[16:32])
	mac.Write(encrypted[:len(encrypted)-sha256.Size])
	expected := mac.Sum(nil)

	if subtle.ConstantTimeCompare(macBytes, expected) != 1 {
		return nil, false
	}

	block, err := aes.NewCipher(c.config.SessionTicketKey[:16])
	if err != nil {
		return nil, false
	}
	ciphertext := encrypted[aes.BlockSize : len(encrypted)-sha256.Size]
	plaintext := make([]byte, len(ciphertext))
	cipher.NewCTR(block, iv).XORKeyStream(plaintext, ciphertext)

	state := new(ServerSessionState)
	ok := state.unmarshal(plaintext)
	return state, ok
}
