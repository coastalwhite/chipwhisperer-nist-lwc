/*
 * Wrapper around the NIST Lightweight Cryptography algorithms for
 * ChipWhisperer SimpleSerial targets.
 *
 * Originally written: 13th of May, 2021
 * By: Gijs J. Burghoorn
 *
 * SimpleSerial Commands used and a small explanation
 * (T -> C): Command sent from Target to Capture board
 * (C -> T): Command sent from Capture to Target board
 *
 * - 'r' (T->C): indicates a computation result with data len > 0,
 *   multiple are sent after eachother when more than 192 bytes need to be sent.
 *   This is than terminated with the 't' command.
 *
 *   One exception being the status command, where there is always 2 bytes being
 *   sent using the 'r' command
 *
 * - 't' (T->C): indicates the termination of a sequence of 'r' commands
 *
 * - 'p' (C->T): Run encryption/decryption on the input buffer. The data
 *   included should be the nonce to be used. Before this command is used the
 *   Input and Key buffers should be sent (look at 'i' and 'k' commands).
 *
 *   Use an even (! scmd & 0x01) subcommand for decryption or an uneven (scmd &
 *   0x01) for encryption.
 *
 *   On success returns a sequence of 'r' commands containing the result data,
 *   followed by a 't' command to terminate data sequence. Then an 'ack'.
 *
 *   On error returns an 'e' command.
 *
 * - 'h' (C->T): Run hashing algorithm on the input buffer (look at 'i'
 *   command). Data included is not used.
 *
 *   On success returns a sequence of 'r' commands containing the result data,
 *   followed by a 't' command to terminate data sequence. Then an 'ack'.
 *
 *   On error returns an 'e' command.
 *
 * - 'i' (C->T): Command to control input buffer. See 'handle_buf' function.
 * - 'k' (C->T): Command to control key. See 'handle_buf' function.
 * - 'a' (C->T): Command to control associated data buffer. See 'handle_buf'
 *   function.
 *
 * - 's' (C->T): Returns the status of the key and input buffers in that order.
 *   0 meaning not ready, 1 meaning ready.
 */

#include "lwc_wrapper.h"
#include "buffer_control.h"

// ChipWhisperer Hardware abstraction layer
#include "hal/hal.h"
#include "simpleserial/simpleserial.h"

// min(x, y) = x if x <= y, y otherwise
unsigned long long min(unsigned long long x, unsigned long long y) {
  if (x < y) {
    return x;
  }

  return y;
}

// Empties out a bytearray buffer
void empty_out_buffer(unsigned long long len, uint8_t *buf) {
  for (unsigned i = 0; i < len; ++i) {
    buf[i] = 0x00;
  }
}

// Return whether nonce len is correct
uint8_t is_nonce_len_correct(unsigned long long len) {
  return len == CRYPTO_NPUBBYTES;
}

// The buffer used for the encryption key
unsigned long long KEY_LEN = 0;
uint8_t KEY[CRYPTO_KEYBYTES];

// Returns whether the key is properly set
uint8_t is_key_set() { return KEY_LEN == CRYPTO_KEYBYTES; }

// The buffer used for the plain/cipher text during
// encryption/decryption/hashing
unsigned long long INPUT_BUF_LEN = 0;
uint8_t INPUT_BUF[MAXSIZE_INPUT_BUFFER];

// Returns whether the input buf is properly set
uint8_t is_input_set() { return INPUT_BUF_LEN > 0; }

// The buffer used for associated data
unsigned long long AD_BUF_LEN = 0;
uint8_t AD_BUF[MAXSIZE_AD_BUFFER];

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
int encrypt(uint8_t *output, unsigned long long *output_len,
            const uint8_t nonce[CRYPTO_NPUBBYTES]) {

  // Start measurement.
  trigger_high();

  int ret = -1;
#if DO_ENCRYPT == 1
  ret = crypto_aead_encrypt(output, output_len, INPUT_BUF, INPUT_BUF_LEN,
                                AD_BUF, AD_BUF_LEN, 0, nonce, KEY);
#endif

  // Stop measurement.
  trigger_low();

  return ret;
}

int decrypt(uint8_t *output, unsigned long long *output_len,
            const uint8_t nonce[CRYPTO_NPUBBYTES]) {

  // Start measurement.
  trigger_high();
  
  int ret = -1;

#if DO_DECRYPT == 1
  ret = crypto_aead_decrypt(output, output_len, 0, INPUT_BUF, INPUT_BUF_LEN,
                                AD_BUF, AD_BUF_LEN, nonce, KEY);
#endif

  // Stop measurement.
  trigger_low();

  return ret;
}

void put_result(unsigned long long result_len, uint8_t *result) {
  unsigned long long begin, end;

  // Put the result on the BUS in 192 byte chunks
  for (begin = 0; begin < result_len; begin += SS_BUS_MAXSIZE) {
    end = min(begin + SS_BUS_MAXSIZE, result_len);
    simpleserial_put('r', end - begin, result + begin);
  }

  // Finalize return with 't'erminate command
  // Along side the 't' command the amount of 'r' commands set (mod 256) gets
  // returned
  uint8_t terminate_output[1] = {begin / SS_BUS_MAXSIZE + 1};
  simpleserial_put('t', 1, terminate_output);
}

/// This function will handle the 'p' command send from the capture board.
uint8_t handle_ed(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) {
  if (!is_nonce_len_correct(len)) {
    return INVALID_NONCE;
  }

  if (!is_key_set()) {
    return OP_WITHOUT_KEY;
  }

  if (!is_input_set()) {
    return OP_WITHOUT_INPUT;
  }

  uint8_t output[MAXSIZE_INPUT_BUFFER + CRYPTO_ABYTES];
  unsigned long long output_len;

  empty_out_buffer(MAXSIZE_INPUT_BUFFER + CRYPTO_ABYTES, output);

  int ret;
  if (scmd & 0x01) {
    ret = encrypt(output, &output_len, buf);
  } else {
    ret = decrypt(output, &output_len, buf);
  }

  if (ret != 0) {
    return OP_RETURNED_ERROR;
  }

  // Send back the entire buffer
  put_result(output_len, output);

  return SS_ERR_OK;
}

uint8_t handle_hash(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) {
  if (!is_input_set()) {
    return OP_WITHOUT_INPUT;
  }

  uint8_t output[CRYPTO_BYTES];
  empty_out_buffer(CRYPTO_BYTES, output);

  // Start power trace
  trigger_high();

  int ret = -1;
#if DO_HASH == 1
  // Perfrom hash
  ret = crypto_hash(output, INPUT_BUF, INPUT_BUF_LEN);
#endif

  // Stop power trace
  trigger_low();

  if (ret != 0) {
    return OP_RETURNED_ERROR;
  }

  // Send back the entire buffer
  put_result(CRYPTO_BYTES, output);

  return SS_ERR_OK;
}

// General function to interact with a data buffer
//
// Buffer commands:
// 0: Append the 'src' buffer to the 'dest' buffer
// 1: Clear the 'dest' buffer
//
// Note:
// The length of the 'dest' buffer will never exceed the 'MAX_SIZE'
//
// Returns:
// SS_ERR_OK (0x00) when successful.
// BUF_OVERFLOW when 'src_len' + 'dest_len' > MAX_SIZE
// INVALID_BUF_CMD when 'buffer_cmd' not recognized
uint8_t handle_buf(unsigned long long *dest_len, uint8_t *dest,
                   const uint8_t buffer_cmd, const unsigned long long MAX_SIZE,
                   const unsigned long long src_len, const uint8_t *src) {
  enum BUFFER_STATUS_CODE ret =
      buffer_interact(dest, dest_len, buffer_cmd, MAX_SIZE, src, src_len);

  if (ret == BSC_INVALID_COMMAND)
    return INVALID_BUF_CMD;

  if (ret == BSC_OVERFLOW)
    return BUF_OVERFLOW;

  return SS_ERR_OK;
}

// Handle the interactions with the key buffer
uint8_t handle_key_buf(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) {
  return handle_buf(&KEY_LEN, KEY, scmd, CRYPTO_KEYBYTES, len, buf);
}

// Handle the interactions with the associated data buffer
uint8_t handle_ad_buf(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) {
  return handle_buf(&AD_BUF_LEN, AD_BUF, scmd, MAXSIZE_AD_BUFFER, len, buf);
}

// Handle the interactions with the plain/cipher text buffer
uint8_t handle_input_buf(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) {
  return handle_buf(&INPUT_BUF_LEN, INPUT_BUF, scmd, MAXSIZE_INPUT_BUFFER, len,
                    buf);
}

// Returns whether the key buffer is correctly set and the input buffer is
// correctly set
uint8_t handle_status(uint8_t cmd, uint8_t scmd, uint8_t len, uint8_t *buf) {
  uint8_t output[2] = {is_key_set(), is_input_set()};
  simpleserial_put('r', 2, output);

  return SS_ERR_OK;
}

int main(void) {
  // Reset all buffers
  buffer_clear(KEY, &KEY_LEN, CRYPTO_KEYBYTES);
  buffer_clear(INPUT_BUF, &INPUT_BUF_LEN, MAXSIZE_INPUT_BUFFER);
  buffer_clear(AD_BUF, &AD_BUF_LEN, MAXSIZE_AD_BUFFER);

  // Setup the specific chipset.
  platform_init();
  // Setup serial communication line.
  init_uart();
  // Setup measurement trigger.
  trigger_setup();

  simpleserial_init();

  // Insert your handlers here.
  simpleserial_addcmd('p', CRYPTO_NPUBBYTES, handle_ed);
  simpleserial_addcmd('h', 0, handle_hash);

  simpleserial_addcmd('k', 0, handle_key_buf);
  simpleserial_addcmd('i', 0, handle_input_buf);
  simpleserial_addcmd('a', 0, handle_ad_buf);

  simpleserial_addcmd('s', 0, handle_status);

  // What for the capture board to send commands and handle them.
  while (1)
    simpleserial_get();
}