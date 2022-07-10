#pragma once
#include "xe/types.h"

namespace xurl{

enum xe_base64_encoding{
	XE_BASE64 = 0,
	XE_BASE64_URL,
	XE_BASE64_PAD,
	XE_BASE64_URL_PAD
};

enum xe_integer_encoding{
	XE_DECIMAL = 0,
	XE_HEX
};

size_t xe_base64_encoded_length(xe_base64_encoding encoding, size_t length);
size_t xe_base64_decoded_length(xe_base64_encoding encoding, xe_cptr in, size_t length);

size_t xe_base64_encode(xe_base64_encoding encoding, xe_ptr out, size_t out_len, xe_cptr in, size_t in_len);
size_t xe_base64_decode(xe_base64_encoding encoding, xe_ptr out, size_t out_len, xe_cptr in, size_t in_len);

byte xe_digit_to_int(char c);
bool xe_char_is_digit(char c);

byte xe_hex_to_int(char c);
bool xe_char_is_hex(char c);

size_t xe_read_integer(xe_integer_encoding encoding, size_t& result, xe_cptr in, size_t in_len);

}