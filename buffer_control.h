#ifndef __LWC_NIST_BUFFER_CONTROL_H__
#define __LWC_NIST_BUFFER_CONTROL_H__

#include <stdint.h>

enum BUFFER_COMMAND {
  BC_APPEND = 0,
  BC_CLEAR = 1,
};

enum BUFFER_STATUS_CODE {
  BSC_OK = 0,
  BSC_OVERFLOW = 1,
  BSC_INVALID_COMMAND = 2,
};

enum BUFFER_STATUS_CODE buffer_interact(
        uint8_t *output_buf, unsigned long long *output_len,
        const uint8_t cmd, const unsigned long long buf_size,
        const uint8_t *input_buf, const unsigned long long input_len
);

enum BUFFER_STATUS_CODE buffer_clear(
        uint8_t *output_kuf, unsigned long long *output_len,
        const unsigned long long buf_size
);

enum BUFFER_STATUS_CODE buffer_append(
        uint8_t *output_buf, unsigned long long *output_len,
        const unsigned long long buf_size,
        const uint8_t *input_buf, const unsigned long long input_len
);

#endif
