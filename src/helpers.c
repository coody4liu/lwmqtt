#include <string.h>

#include "helpers.h"

// TODO: Make overflow safe?
lwmqtt_string_t lwmqtt_str(const char *str) { return (lwmqtt_string_t){(uint16_t)strlen(str), (char *)str}; }

int lwmqtt_strcmp(lwmqtt_string_t *a, const char *b) {
  // get length of b
  long len = strlen(b);

  // otherwise check if length matches
  if (len != a->len) {
    return -1;
  }

  // compare memory
  return strncmp(a->data, b, (size_t)len);
}

lwmqtt_err_t lwmqtt_read_data(uint8_t **buf, uint8_t *buf_end, uint8_t **data, size_t len) {
  // check zero length
  if (len == 0) {
    *data = NULL;
    return LWMQTT_SUCCESS;
  }

  // check buffer size
  if ((size_t)(buf_end - (*buf)) < len) {
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  // read data
  *data = *buf;

  // advance pointer
  *buf += len;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_write_data(uint8_t **buf, uint8_t *buf_end, uint8_t *data, size_t len) {
  // check zero length
  if (len == 0) {
    return LWMQTT_SUCCESS;
  }

  // check buffer size
  if ((size_t)(buf_end - (*buf)) < len) {
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  // write data
  memcpy(*buf, data, len);

  // advance pointer
  *buf += len;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_read_num(uint8_t **buf, uint8_t *buf_end, uint16_t *num) {
  // check buffer size
  if ((size_t)(buf_end - (*buf)) < 2) {
    *num = 0;
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  // read two byte integer
  *num = (uint16_t)256 * (*buf)[0] + (*buf)[1];

  // adjust pointer
  *buf += 2;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_write_num(uint8_t **buf, uint8_t *buf_end, uint16_t num) {
  // check buffer size
  if ((size_t)(buf_end - (*buf)) < 2) {
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  // write bytes
  (*buf)[0] = (uint8_t)(num / 256);
  (*buf)[1] = (uint8_t)(num % 256);

  // adjust pointer
  *buf += 2;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_read_string(uint8_t **buf, uint8_t *buf_end, lwmqtt_string_t *str) {
  // read length
  uint16_t len;
  lwmqtt_err_t err = lwmqtt_read_num(buf, buf_end, &len);
  if (err != LWMQTT_SUCCESS) {
    return err;
  }

  // read data
  err = lwmqtt_read_data(buf, buf_end, (uint8_t **)&str->data, len);
  if (err != LWMQTT_SUCCESS) {
    return err;
  }

  // set length
  str->len = len;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_write_string(uint8_t **buf, uint8_t *buf_end, lwmqtt_string_t str) {
  // write string length
  lwmqtt_err_t err = lwmqtt_write_num(buf, buf_end, str.len);
  if (err != LWMQTT_SUCCESS) {
    return err;
  }

  // write data
  err = lwmqtt_write_data(buf, buf_end, (uint8_t *)str.data, str.len);
  if (err != LWMQTT_SUCCESS) {
    return err;
  }

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_read_byte(uint8_t **buf, uint8_t *buf_end, uint8_t *byte) {
  // check buffer size
  if ((size_t)(buf_end - (*buf)) < 1) {
    *byte = 0;
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  // read byte
  *byte = (*buf)[0];

  // adjust pointer
  *buf += 1;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_write_byte(uint8_t **buf, uint8_t *buf_end, uint8_t byte) {
  // check buffer size
  if ((size_t)(buf_end - (*buf)) < 1) {
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  // write byte
  (*buf)[0] = byte;

  // adjust pointer
  *buf += 1;

  return LWMQTT_SUCCESS;
}

int lwmqtt_varnum_length(uint32_t varnum) {
  if (varnum < 128) {
    return 1;
  } else if (varnum < 16384) {
    return 2;
  } else if (varnum < 2097151) {
    return 3;
  } else if (varnum < 268435455) {
    return 4;
  } else {
    return -1;
  }
}

lwmqtt_err_t lwmqtt_read_varnum(uint8_t **buf, uint8_t *buf_end, uint32_t *varnum) {
  // prepare last byte
  uint8_t byte;

  // prepare multiplier
  uint32_t multiplier = 1;

  // prepare length
  size_t len = 0;

  // initialize number
  *varnum = 0;

  // decode variadic number
  do {
    // increment length
    len++;

    // return error if buffer is to small
    if ((size_t)(buf_end - (*buf)) < len) {
      return LWMQTT_BUFFER_TOO_SHORT;
    }

    // return error if the length has overflowed
    if (len > 4) {
      return LWMQTT_VARNUM_OVERFLOW;
    }

    // read byte
    byte = (*buf)[len - 1];

    // add byte to number
    *varnum += (byte & 127) * multiplier;

    // increase multiplier
    multiplier *= 128;
  } while ((byte & 128) != 0);

  // adjust pointer
  *buf += len;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t lwmqtt_write_varnum(uint8_t **buf, uint8_t *buf_end, uint32_t varnum) {
  // init len counter
  size_t len = 0;

  // encode variadic number
  do {
    // check overflow
    if (len == 4) {
      return LWMQTT_VARNUM_OVERFLOW;
    }

    // return error if buffer is to small
    if ((size_t)(buf_end - (*buf)) < len + 1) {
      return LWMQTT_BUFFER_TOO_SHORT;
    }

    // calculate current byte
    uint8_t byte = (uint8_t)(varnum % 128);

    // change remaining length
    varnum /= 128;

    // set the top bit of this byte if there are more to encode
    if (varnum > 0) {
      byte |= 0x80;
    }

    // write byte
    (*buf)[len++] = byte;
  } while (varnum > 0);

  // adjust pointer
  *buf += len;

  return LWMQTT_SUCCESS;
}
