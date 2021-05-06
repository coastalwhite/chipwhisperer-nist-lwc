#include "simpleserial/simpleserial.h"
#include "hal/hal.h"

#include "lwc/api.h"
#include "lwc/encrypt.h"
#include "lwc/decrypt.h"
#include "lwc/hash.h"

#define AD_MAXSIZE 1024
#define IN_MAXSIZE 1024

uint8_t ISSET_KEY = 0;
uint8_t KEY[CRYPTO_KEYBYTES];

uint8_t IN_LEN = 0;
uint8_t IN[IN_MAXSIZE];

uint8_t AD_LEN = 0;
uint8_t AD[AD_MAXSIZE];

// We use the two functions from the lwc library here:
// crypto_aead_encrypt and crypto_aead_decrypt
//
// These both use parameters:
// c (clen)   --- ByteArray: cipher text (length)
// m (mlen)   --- ByteArray: plain text (length)
// ad (adlen) --- ByteArray: associated data (length)
// npub       --- ByteArray: nonce
// k          --- ByteArray: key
// nsec       --- Not used
//
uint8_t handle_encrypt(uint8_t len, uint8_t *buf) {
      size_t CIPHERTEXT_LENGTH = len + CRYPTO_ABYTES;
      uint8_t cipher_text[CIPHERTEXT_LENGTH];
      for (size_t i = 0; i < CIPHERTEXT_LENGTH; ++i) {
        cipher_text[i] = 0x00;
      }

      // Start measurement.
      trigger_high();

      int ret = crypto_aead_encrypt(
              out, outLen,
              m, mlen,
              ad, adlen,
              nsec,
              npub,
              k
      );

      // Stop measurement.
      trigger_low();
}

uint8_t handle_decrypt(uint8_t len, uint8_t *buf) {
      size_t PLAINTEXT_LENGTH = len - CRYPTO_ABYTES;
      uint8_t plain_text[PLAINTEXT_LENGTH];
      for (size_t i = 0; i < PLAINTEXT_LENGTH; ++i) {
        plain_text[i] = 0x00;
      }

      // Start measurement.
      trigger_high();

      int ret = crypto_aead_decrypt(
              m, mlen,
              nsec,
              c, clen,
              ad, adlen,
              npub,
              k
      );
      
      // Stop measurement.
      trigger_low();
}

/// This function will handle the 'p' command send from the capture board.
uint8_t handle_ed(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf)
{
  if (len > 192) {
    return SS_ERR_LEN;
  }

  if (ISSET_KEY == 0) {
    return SS_ERR_CMD;
  }


  int ret;
  if (scmd & 0x01) {
      ret = encrypt(len, buf);
  } else {
      ret = decrypt(len, buf);
      simpleserial_put('t', 1, buff);
  }

  simpleserial_put('r', 1, buff);

  return SS_ERR_OK;
}

uint8_t handle_hash(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) 
{
    if (len > 192) {
        return SS_ERR_LEN;
    }

    uint8_t out[CRYPTO_BYTES];
    for (uint8_t i = 0; i < CRYPTO_BYTES; ++i) {
        out[i] = 0;
    }

    // Start power trace
    trigger_high();

    // Perfrom hash
    int ret = crypto_hash(
        out,
        in,
        len
    );

    // Stop power trace
    trigger_low();

    simpleserial_put('r', CRYPTO_BYTES, out);
}

uint8_t set_key(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) {
    // If the buffer length is invalid.
    if (len > 192) {
        return SS_ERR_LEN;
    }

    // If more bytes than are requested are trying to be written.
    // Thus the 'scmd' is wrong.
    if (scmd * 192 + len > CRYPTO_KEYBYTES) {
        return SS_ERR_CMD;
    }

    for (uint8_t i = 0; i < len; ++i) {
        KEY[scmd * 192 + i] = buf[i];
    }
    
    ISSET_KEY = 1;

    return SS_ERR_OK;
}

uint8_t set_associated_data() {
    //TODO: Implement associated data.
    return 0;
}

int main(void) {
  // Reset Crypto Bytes
  for (size_t i = 0; i < CRYPTO_KEYBYTES; ++i) {
      CRYPTO_KEYBYTES[i] = 0x00;
  }

  // Setup the specific chipset.
  platform_init();
  // Setup serial communication line.
  init_uart();
  // Setup measurement trigger.
  trigger_setup();

  simpleserial_init();

  // Insert your handlers here.
  simpleserial_addcmd('p', 0, handle_ed);
  simpleserial_addcmd('h', 0, handle_hash);
  simpleserial_addcmd('k', 0, set_key);

  // What for the capture board to send commands and handle them.
  while (1)
    simpleserial_get();
}