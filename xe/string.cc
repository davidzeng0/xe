#include "string.h"
#include "arch.h"
#include "mem.h"
#include "common.h"
#include "container/map.h"

static const byte lowercase[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

char xe_string_tolower(char c){
	return lowercase[(byte)c];
}

size_t xe_string_length(xe_cstr str){
	return xe_arch_strlen(str);
}

xe_cstr xe_string_find(xe_cstr ptr, char c, size_t n){
	return (xe_cstr)xe_arch_memchr(ptr, c, n);
}

int xe_string_compareCase(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncasecmp(s1, s2, n);
}

int xe_string_compareCasez(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncasecmpz(s1, s2, n);
}

int xe_string_compare(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncmp(s1, s2, n);
}

int xe_string_comparez(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncmpz(s1, s2, n);
}

xe_string::xe_string(xe_string&& src){
	data_ = src.data_;
	size_ = src.size_;
	src.data_ = null;
	src.size_ = 0;
}

xe_string::xe_string(xe_vector<char>&& src){
	data_ = src.data();
	size_ = src.size();

	src.clear();
}

xe_string& xe_string::operator=(xe_string&& src){
	data_ = src.data_;
	size_ = src.size_;

	src.clear();

	return *this;
}

xe_string::xe_string(xe_cptr string){
	operator=(string);
}

xe_string::xe_string(xe_cptr string, size_t len){
	data_ = (xe_pchar)string;
	size_ = len;
}

size_t xe_string::indexOf(char c, size_t off) const{
	xe_cstr ptr = xe_string_find(data() + off, c, size_ - off);

	if(ptr)
		return ptr - data();
	return -1;
}

size_t xe_string::indexOf(char c) const{
	return indexOf(c, 0);
}

void xe_string::clear(){
	data_ = null;
	size_ = 0;
}

size_t xe_string::hash() const{
	return xe_hash::hash_bytes(c_str(), length());
}

xe_string& xe_string::operator=(xe_cptr string){
	data_ = (xe_pchar)string;
	size_ = length(string);

	return *this;
}

bool xe_string::operator==(const xe_string& o) const{
	return equal(o);
}

bool xe_string::operator==(xe_cptr o) const{
	return equal(o);
}

bool xe_string::equal(const xe_string& o) const{
	return equal(*this, o);
}

bool xe_string::equalCase(const xe_string& o) const{
	return equalCase(*this, o);
}

bool xe_string::equal(xe_cptr o) const{
	return equal(*this, xe_string(o));
}

bool xe_string::equalCase(xe_cptr o) const{
	return equalCase(*this, xe_string(o));
}

size_t xe_string::length(xe_cptr str){
	return xe_string_length((xe_cstr)str);
}

bool xe_string::equal(const xe_string& a, const xe_string& b){
	if(a.length() != b.length())
		return false;
	return a.data_ == b.data_ || !xe_string_compare(a.data_, b.data_, a.length());
}

bool xe_string::equalCase(const xe_string& a, const xe_string& b){
	if(a.length() != b.length())
		return false;
	return a.data_ == b.data_ || !xe_string_compareCase(a.data_, b.data_, a.length());
}

bool xe_string::copy(xe_cptr src, size_t n){
	data_ = xe_alloc<char>(n + 1);

	if(!data())
		return false;
	xe_tmemcpy<char>(data(), src, n);

	data_[n] = 0;
	size_ = n;

	return true;
}

bool xe_string::copy(xe_cptr src){
	return copy(src, length(src));
}

bool xe_string::copy(const xe_string& src){
	return copy(src.data_, src.length());
}

xe_string xe_string::substring(size_t start){
	return substring(start, length());
}

xe_string xe_string::substring(size_t start, size_t end){
	return xe_string(data() + start, end - start);
}