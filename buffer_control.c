#include "buffer_control.h"

enum BUFFER_STATUS_CODE
buffer_interact(uint8_t *output_buf, unsigned long long *output_len,
                const uint8_t cmd, const unsigned long long buf_size,
                const uint8_t *input_buf, const unsigned long long input_len) {
  // Check for invalid commands
  if (cmd > 2)
    return BSC_INVALID_COMMAND;

  // Run proper command
  switch (cmd) {
  case BC_CLEAR:
    return buffer_clear(output_buf, output_len, buf_size);
  case BC_APPEND:
    return buffer_append(output_buf, output_len, buf_size, input_buf,
                         input_len);
  }

  return BSC_INVALID_COMMAND;
}

enum BUFFER_STATUS_CODE buffer_clear(uint8_t *output_buf,
                                     unsigned long long *output_len,
                                     const unsigned long long buf_size) {
  // Set the entire buffer to zeros
  for (unsigned long long i = 0; i < buf_size; ++i) {
    output_buf[i] = 0;
  }

  // Set the length to 0
  *output_len = 0;

  return BSC_OK;
}

enum BUFFER_STATUS_CODE buffer_append(uint8_t *output_buf,
                                      unsigned long long *output_len,
                                      const unsigned long long buf_size,
                                      const uint8_t *input_buf,
                                      const unsigned long long input_len) {
  // Check if we are not having any overflows
  if ((*output_len) + input_len > buf_size) {
    return BSC_OVERFLOW;
  }

  // Copy the data from the input buffer to the proper location in the output
  // buffer.
  for (unsigned long long i = 0; i < input_len; ++i) {
    output_buf[(*output_len) + i] = input_buf[i];
  }

  *output_len += input_len;

  return BSC_OK;
}
