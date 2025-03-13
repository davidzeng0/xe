#pragma once
#include "xstd/types.h"
#include "overflow.h"

enum xe_base64_encoding{
	XE_BASE64 = 0,
	XE_BASE64_URL,
	XE_BASE64_PAD,
	XE_BASE64_URL_PAD
};

enum xe_integer_encoding{
	XE_DECIMAL = 10,
	XE_HEX = 0x10
};

size_t xe_base64_encoded_length(xe_base64_encoding encoding, size_t length);
size_t xe_base64_decoded_length(xe_base64_encoding encoding, xe_cptr in, size_t length);

size_t xe_base64_encode(xe_base64_encoding encoding, xe_ptr out, size_t out_len, xe_cptr in, size_t in_len);
size_t xe_base64_decode(xe_base64_encoding encoding, xe_ptr out, size_t out_len, xe_cptr in, size_t in_len);

byte xe_digit_to_int(byte c);
bool xe_char_is_digit(byte c);

byte xe_hex_to_int(byte c);
bool xe_char_is_hex(byte c);

byte xe_char_tolower(byte c);

size_t xe_encoding_to_int(xe_integer_encoding encoding, byte c);

template<typename T>
static inline size_t xe_read_integer(xe_integer_encoding encoding, T& result, xe_cptr vin, size_t in_len){
	size_t i;
	byte* in = (byte*)vin;
	byte digit;

	for(i = 0; i < in_len; i++){
		digit = xe_encoding_to_int(encoding, in[i]);

		if(digit >= encoding)
			break;
		if(xe_overflow_mul<T>(result, encoding) || xe_overflow_add<T>(result, digit))
			return -1;
	}

	return i;
}