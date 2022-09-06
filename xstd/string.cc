#include "string.h"
#include "xutil/mem.h"

static size_t xe_index_of(const xe_string_view& string, char c, size_t off){
	xe_cptr ptr = xe_memchr(string.data() + off, c, string.length() - off);

	return ptr ? (xe_cstr)ptr - string.data() : -1;
}

bool xe_string_view::equal(const xe_string_view& a, const xe_string_view& b){
	if(a.length() != b.length())
		return false;
	return a.data_ == b.data_ || !xe_strncmp(a.data_, b.data_, a.length());
}

bool xe_string_view::equal_case(const xe_string_view& a, const xe_string_view& b){
	if(a.length() != b.length())
		return false;
	return a.data_ == b.data_ || !xe_strncasecmp(a.data_, b.data_, a.length());
}

size_t xe_string_view::index_of(char c, size_t off) const{
	return xe_index_of(*this, c, off);
}

size_t xe_string_view::index_of(char c) const{
	return index_of(c, 0);
}

bool xe_string_view::operator==(const xe_string_view& o) const{
	return equal(o);
}

bool xe_string_view::operator==(xe_cstr o) const{
	return equal(o);
}

bool xe_string_view::equal(const xe_string_view& o) const{
	return equal(o, *this);
}

bool xe_string_view::equal_case(const xe_string_view& o) const{
	return equal_case(o, *this);
}

bool xe_string_view::equal(xe_cstr o) const{
	return xe_string_view(o).equal(*this);
}

bool xe_string_view::equal_case(xe_cstr o) const{
	return xe_string_view(o).equal_case(*this);
}

xe_string::xe_string(xe_string&& src){
	data_ = src.data_;
	size_ = src.size_;
	src.data_ = null;
	src.size_ = 0;
}

xe_string& xe_string::operator=(xe_string&& src){
	clear();

	data_ = src.data_;
	size_ = src.size_;
	src.data_ = null;
	src.size_ = 0;

	return *this;
}

size_t xe_string::index_of(char c, size_t off) const{
	return xe_index_of(*this, c, off);
}

size_t xe_string::index_of(char c) const{
	return index_of(c, 0);
}

bool xe_string::operator==(const xe_string_view& o) const{
	return equal(o);
}

bool xe_string::operator==(xe_cstr o) const{
	return equal(o);
}

bool xe_string::equal(const xe_string_view& o) const{
	return o.equal(*this);
}

bool xe_string::equal_case(const xe_string_view& o) const{
	return o.equal_case(*this);
}

bool xe_string::equal(xe_cstr o) const{
	return xe_string_view(o).equal(*this);
}

bool xe_string::equal_case(xe_cstr o) const{
	return xe_string_view(o).equal_case(*this);
}

bool xe_string::copy(xe_cstr src, size_t n){
	char* data = xe_alloc<char>(n + 1);

	if(!data)
		return false;
	clear();
	xe_tmemcpy<char>(data, src, n);

	data[n] = 0;
	data_ = data;
	size_ = n;

	return true;
}

bool xe_string::copy(xe_cstr src){
	return copy(src, xe_strlen(src));
}

bool xe_string::copy(const xe_string_view& src){
	return copy(src.data(), src.length());
}