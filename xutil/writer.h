#pragma once
#include "xstd/list.h"
#include "xstd/fla.h"
#include "xstd/string.h"
#include "xutil/endian.h"

template<class xe_container, typename = std::enable_if<std::is_same_v<typename xe_container::value_type, byte>>>
class xe_writer{
protected:
	xe_container& buffer;
	size_t position;

	size_t available_space(size_t requested){
		constexpr bool has_grow = requires(xe_container& c) {
			c.grow(size_t());
		};

		constexpr bool has_resize = requires(xe_container& c) {
			c.resize(size_t());
		};

		if(position > buffer.size())
			return 0;
		if(requested <= buffer.size() - position)
			return requested;
		requested = xe_min(requested, xe_max_value<size_t>() - buffer.size());

		if constexpr(has_grow){
			if(buffer.grow(position + requested) && buffer.resize(position + requested)) return requested;
		}else if constexpr(has_resize){
			if(buffer.resize(position + requested)) return requested;
		}

		return buffer.size() - position;
	}

	template<typename T>
	bool write_val(T v){
		if(available_space(sizeof(T)) < sizeof(T))
			return false;
		*(T*)(buffer.data() + position) = v;

		position += sizeof(T);

		return true;
	}
public:
	xe_writer(xe_container& out): buffer(out){
		position = 0;
	}

	size_t write(xe_cptr data, size_t len){
		size_t n = available_space(len);

		xe_memcpy(buffer.data() + position, data, n);

		position += n;

		return n;
	}

	size_t write(const xe_string_view& str){
		return write(str.data(), str.size());
	}

	size_t write(const xe_slice<byte>& data){
		return write(data.data(), data.size());
	}

	template<size_t N>
	size_t write(const xe_fla<byte, N>& data){
		return write(data.data(), data.size());
	}

	bool w8(byte v){
		return write_val(v);
	}

	bool w16be(ushort v){
		return write_val(xe_htoe(v, XE_BIG_ENDIAN));
	}

	bool w32be(uint v){
		return write_val(xe_htoe(v, XE_BIG_ENDIAN));
	}

	bool w64be(ulong v){
		return write_val(xe_htoe(v, XE_BIG_ENDIAN));
	}

	bool w16le(ushort v){
		return write_val(xe_htoe(v, XE_LITTLE_ENDIAN));
	}

	bool w32le(uint v){
		return write_val(xe_htoe(v, XE_LITTLE_ENDIAN));
	}

	bool w64le(ulong v){
		return write_val(xe_htoe(v, XE_LITTLE_ENDIAN));
	}

	size_t pos() const{
		return position;
	}

	void set_pos(size_t pos){
		position = pos;
	}

	void reset(){
		position = 0;
	}

	~xe_writer() = default;
};