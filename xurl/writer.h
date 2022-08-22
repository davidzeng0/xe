#pragma once
#include "xstd/slice.h"

class xe_writer{
protected:
	xe_slice<byte> buffer;
	size_t written;
public:
	xe_writer(){
		written = 0;
	}

	xe_writer(xe_slice<byte>& in){
		out(in);
	}

	xe_writer(xe_slice<byte>&& in){
		out(in);
	}

	void out(xe_slice<byte>& in){
		buffer = in;
		written = 0;
	}

	void out(xe_slice<byte>&& in){
		out(in);
	}

	template<typename T>
	size_t write(const T* data, size_t len){
		size_t n = xe_min(len * sizeof(T), buffer.size() - written);

		xe_memcpy(buffer.data() + written, data, n);

		written += n;

		return n;
	}

	template<typename T>
	size_t write(const T& data){
		return write(&data, 1);
	}

	size_t count() const{
		return written;
	}

	void reset(){
		written = 0;
	}

	~xe_writer() = default;
};