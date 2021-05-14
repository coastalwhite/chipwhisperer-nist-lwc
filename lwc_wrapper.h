#ifndef __LWC_WRAPPER_H__
#define __LWC_WRAPPER_H__

// If we are developing, just set the HAL_TYPE to a random one
#ifndef HAL_TYPE
#define HAL_TYPE 8
#endif

// Use the SimpleSerial V2 Protocol
#define SS_VER 2

#include <stdint.h>

// Import dummy header files for development
#include "targets/dummy/api.h"
#include "targets/dummy/crypto_aead.h"
#include "targets/dummy/crypto_hash.h"

// If we are actually compiling, include the "api.h" of the target we are
// compiling
#ifdef PLATFORM
#include "api.h"
#else
#define DO_ENCRYPT 1
#define DO_DECRYPT 1
#define DO_HASH 1
#endif

#define SS_BUS_MAXSIZE 192

#define MAXSIZE_INPUT_BUFFER 256
#define MAXSIZE_AD_BUFFER 256

enum ERROR_CODES {
  INVALID_BUF_CMD = 0x10,
  OP_WITHOUT_KEY = 0x11,
  OP_WITHOUT_INPUT = 0x12,
  BUF_OVERFLOW = 0x13,
  INVALID_NONCE = 0x14,
  OP_RETURNED_ERROR = 0x15,
};

int encrypt(uint8_t *output, unsigned long long *output_len,
            const uint8_t nonce[CRYPTO_NPUBBYTES]);

int decrypt(uint8_t *output, unsigned long long *output_len,
            const uint8_t nonce[CRYPTO_NPUBBYTES]);

#endif
