#include "xarch/arch.h"
#include "xutil/log.h"
#include "http_internal.h"
#include "xutil/writer.h"
#include "../request_internal.h"

using namespace xurl;

bool xe_http_case_insensitive::operator()(const xe_string_view& a, const xe_string_view& b) const{
	return a.equal_case(b);
}

size_t xe_http_lowercase_hash::operator()(const xe_string_view& str) const{
	return xe_arch_hash_lowercase(str.data(), str.length());
}

xe_http_string::xe_http_string(){
	owner = false;
}

xe_http_string::xe_http_string(xe_http_string&& other): xe_string(std::move(other)){
	owner = other.owner;
	other.owner = false;
}

xe_http_string& xe_http_string::operator=(xe_http_string&& other){
	clear();

	xe_string::operator=(std::move(other));

	owner = other.owner;
	other.owner = false;

	return *this;
}

bool xe_http_string::copy(const xe_string_view& src){
	clear();

	if(!xe_string::copy(src))
		return false;
	owner = true;

	return true;
}

void xe_http_string::own(const xe_string_view& src){
	operator=(src);

	owner = true;
}

void xe_http_string::clear(){
	if(owner)
		xe_string::clear();
	owner = false;
	data_ = null;
	size_ = 0;
}

xe_http_string& xe_http_string::operator=(const xe_string_view& src){
	clear();

	data_ = (char*)src.data();
	size_ = src.size();

	return *this;
}

bool xe_http_string::operator==(const xe_http_string& other) const{
	return equal(other);
}

xe_http_string::~xe_http_string(){
	clear();
}

void xe_http_protocol::redirect(xe_request_internal& request, xe_string&& url){
	request.complete(0);
}

bool xe_http_protocol::available(xe_http_connection& connection, bool available){

	return true;
}

int xe_http_protocol::open(xe_http_internal_data& data, xe_url&& url, bool redirect){
	if(!redirect){
		data.clear();
		data.method = "GET";
	}

	data.url = std::move(url);

	return data.internal_set_header("Host", data.url.host(), 0) ? 0 : XE_ENOMEM;
}

static bool copy_string(xe_http_string& dest, const xe_string_view& src, uint flags){
	if(flags & XE_HTTP_COPY)
		return dest.copy(src);
	if(flags & XE_HTTP_FREE)
		dest.own(src);
	else
		dest = src;
	return true;
}

xe_http_internal_data::xe_http_internal_data(){
	min_version = XE_HTTP_VERSION_1_0;
	max_version = XE_HTTP_VERSION_1_1;
}

bool xe_http_internal_data::internal_set_method(const xe_string_view& method_, uint flags){
	return copy_string(method, method_, flags);
}

bool xe_http_internal_data::internal_set_header(const xe_string_view& rkey, const xe_string_view& rvalue, uint flags){
	xe_http_string key, value;

	key = rkey;

	auto pair = headers.find(key);

	if(pair != headers.end())
		return copy_string(pair -> second, rvalue, flags & (XE_HTTP_COPY_VALUE | XE_HTTP_FREE_VALUE));
	if(!copy_string(key, rkey, flags & (XE_HTTP_COPY_KEY | XE_HTTP_FREE_KEY)))
		return false;
	if(!copy_string(value, rvalue, flags & (XE_HTTP_COPY_VALUE | XE_HTTP_FREE_VALUE)))
		return false;
	return headers.insert(std::move(key), std::move(value));
}

void xe_http_internal_data::clear(){
	url.clear();
	headers.clear();
	method.clear();
}

xe_http_internal_data::~xe_http_internal_data(){
	clear();
}

xe_string_view xe_http_common_data::location() const{
	return ((xe_http_common_specific*)this) -> url.href();
}

bool xe_http_common_data::set_header(const xe_string_view& key, const xe_string_view& value, uint flags){
	return ((xe_http_common_specific*)this) -> internal_set_header(key, value, flags);
}

static constexpr xe_string_view http_prefix = "HTTP/";

enum xe_http_singleconnection_read_state{
	READ_NONE = 0,
	READ_STATUSLINE,
	READ_HEADER,
	READ_BODY
};

enum xe_http_singleconnection_transfer_mode{
	TRANSFER_MODE_NONE = 0,
	TRANSFER_MODE_CONNECTION,
	TRANSFER_MODE_CONTENTLENGTH,
	TRANSFER_MODE_CHUNKS
};

enum xe_http_singleconnection_chunked_state{
	CHUNKED_READ_NONE = 0,
	CHUNKED_READ_SIZE,
	CHUNKED_READ_EXTENSION,
	CHUNKED_READ_DATA,
	CHUNKED_READ_END,
	CHUNKED_READ_TRAILERS
};

enum{
	HEADERBUFFER_SIZE = 128 * 1024,
	MAXIMUM_HEADER_SIZE = 1024 * 1024
};

enum{
	HTTP_KEEPALIVE_TIMEOUT = 60 * 1000
};

static inline bool read_status(xe_string_view& line, uint& out, uint& i){
	size_t result;

	out = 0;
	result = xe_read_integer(XE_DECIMAL, out, line.data() + i, line.length() - i);

	if(result == (size_t)-1 || result == 0)
		return false;
	i += result;

	return true;
}

static inline bool read_digit(xe_string_view& line, uint& out, uint& index){
	if(!xe_char_is_digit(line[index]))
		return false;
	out = xe_digit_to_int(line[index++]);

	return true;
}

static inline bool read_version(xe_string_view& line, uint& off, xe_http_version& version){
	uint major, minor;

	if(!read_digit(line, major, off) ||
		line[off++] != '.' ||
		!read_digit(line, minor, off))
		return false;
	version = (xe_http_version)(major * 10 + minor);

	return version >= XE_HTTP_VERSION_0_9 && version <= XE_HTTP_VERSION_1_1;
}

static bool parse_status_line(xe_string_view& line, xe_http_version& version, uint& status, xe_string_view& reason){
	if(line.length() < xe_string_view("HTTP/0.0 0").length())
		return false;
	uint off = http_prefix.length();

	if(!read_version(line, off, version) ||
		line[off++] != ' ' ||
		!read_status(line, status, off))
		return false;
	if(off < line.length()){
		if(line[off++] != ' ')
			return false;
		reason = line.substring(off);
	}

	return true;
}

static inline void header_parse(xe_string_view& line, xe_string_view& key, xe_string_view& value){
	size_t index = line.index_of(':');

	if(index != (size_t)-1){
		key = line.substring(0, index);
		index++;

		while(index < line.length() && line[index] == ' ')
			index++;
		value = line.substring(index);
	}else{
		key = line;
	}
}

static constexpr xe_cstr http_version_to_string(xe_http_version version){
	switch(version){
		case XE_HTTP_VERSION_0_9:
			return "HTTP/0.9";
		case XE_HTTP_VERSION_1_0:
			return "HTTP/1.0";
		case XE_HTTP_VERSION_1_1:
			return "HTTP/1.1";
		default:
			xe_notreached();

			break;
	}

	return null;
}

static bool build_headers(xe_vector<byte>& headers, xe_http_common_specific& specific, xe_http_version version){
	xe_string_view crlf = "\r\n";
	xe_string_view ws = " ";
	xe_string_view separator = ": ";

	size_t len;

	xe_string_view method = specific.method;
	xe_string_view path = specific.url.path();
	xe_string_view http_ver = http_version_to_string(version);

	auto writer = xe_writer(headers);

	if(version == XE_HTTP_VERSION_0_9)
		method = "GET";
	if(!path.length())
		path = "/";
	len = method.length() + ws.length() + path.length();

	if(version != XE_HTTP_VERSION_0_9){
		len += ws.length() + http_ver.length();

		for(auto& t : specific.headers)
			len += t.first.length() + separator.length() + t.second.length() + crlf.length();
		len += crlf.length();
	}

	len += crlf.length();

	if(!headers.resize(len))
		return false;
	writer.write(method);
	writer.write(ws);
	writer.write(path);

	if(version != XE_HTTP_VERSION_0_9){
		writer.write(ws);
		writer.write(http_ver);
	}

	writer.write(crlf);

	if(version != XE_HTTP_VERSION_0_9){
		for(auto& t : specific.headers){
			writer.write(t.first);
			writer.write(separator);
			writer.write(t.second);
			writer.write(crlf);
		}

		writer.write(crlf);
	}

	xe_assert(writer.pos() == len && headers.size() == len);

	return true;
}

int xe_http_singleconnection::init_socket(){
	uint recvbuf_size = specific -> get_recvbuf_size();

	return recvbuf_size ? set_recvbuf_size(xe_min<uint>(recvbuf_size, xe_max_value<int>())) : 0;
}

void xe_http_singleconnection::set_request_state(xe_connection_state state){
	if(state == XE_CONNECTION_STATE_RESOLVING)
		request -> set_state(XE_REQUEST_STATE_DNS);
	else if(state == XE_CONNECTION_STATE_CONNECTING)
		request -> set_state(XE_REQUEST_STATE_CONNECTING);
}

void xe_http_singleconnection::set_state(xe_connection_state state){
	xe_connection::set_state(state);

	if(!request)
		return;
	set_request_state(state);
}

int xe_http_singleconnection::ready(){
	if(request){
		int err = start();

		if(err) complete(err);
	}

	return 0;
}

bool xe_http_singleconnection::readable(){
	return send_offset < client_headers.size();
}

int xe_http_singleconnection::writable(){
	return send_headers();
}

ssize_t xe_http_singleconnection::data(xe_ptr buf, size_t size){
	byte* data = (byte*)buf;
	ssize_t error = 0;

	if(read_state < READ_BODY){
		if(!size)
			return XE_PARTIAL_FILE;
		error = parse_headers(data, size);
	}else{
		error = write_body(data, size);
	}

	if(read_state == READ_NONE){
		transfer_active = false;

		if(connection_close)
			return 0;
		if(!error && proto.available(*this, true))
			return 0;
		if(request_active)
			complete(0);
		if(!request_active){
			/* connection not reused immediately */
			timer.callback = timeout;

			xe_return_error(transferctl(XE_PAUSE_ALL));
			start_timer(HTTP_KEEPALIVE_TIMEOUT, XE_TIMER_PASSIVE);
		}
	}

	return error ?: size;
}

void xe_http_singleconnection::close(int error){
	xe_dealloc(header_buffer);

	proto.available(*this, false);

	if(request)
		complete(error);
	xe_http_connection::close(error);
}

void xe_http_singleconnection::complete(int error){
	xe_request_internal& req = *request;

	request_active = false;
	specific -> connection = null;
	request = null;
	specific = null;

	if(!error && follow){
		follow = false;
		proto.redirect(req, std::move(location));
		location.clear();
	}else{
		location.clear();
		req.complete(error);
	}
}

int xe_http_singleconnection::send_headers(){
	xe_assert(client_headers.size());

	ssize_t sent = send(client_headers.data() + send_offset, client_headers.size() - send_offset);

	if(sent <= 0){
		if(sent == 0)
			return XE_SEND_ERROR;
		if(sent != XE_EAGAIN)
			return sent;
		return 0;
	}

	send_offset += sent;

	if(send_offset >= client_headers.size()){
		client_headers.resize(0);
		send_offset = 0;
	}else{
		return transferctl(XE_RESUME_SEND);
	}

	return 0;
}

inline int xe_http_singleconnection::start(){
	xe_http_version version = specific -> max_version;

	request_active = true;
	transfer_active = true;
	connection_close = true;
	statusline_prefix_checked = false;
	read_state = READ_STATUSLINE;
	header_offset = 0;
	header_total = 0;
	chunked_state = 0;
	transfer_mode = 0;
	send_offset = 0;
	data_len = 0;
	follow = false;
	bodyless = false;
	location.clear();

	if(!build_headers(client_headers, *specific, version))
		return XE_ENOMEM;
	xe_return_error(send_headers());

	request -> set_state(XE_REQUEST_STATE_ACTIVE);

	if(version == XE_HTTP_VERSION_0_9)
		xe_return_error(pretransfer());
	else if(specific -> method == "HEAD")
		bodyless = true;

#ifdef XE_DEBUG
	xe_string_view path = specific -> url.path();

	if(!path.size())
		path = "/";
	if(version != XE_HTTP_VERSION_0_9){
		xe_log_trace(this, "<< %.*s %.*s %s", specific -> method.length(), specific -> method.c_str(), path.length(), path.data(), http_version_to_string(version));

		for(auto& t : specific -> headers)
			xe_log_trace(this, "<< %.*s: %.*s", t.first.length(), t.first.c_str(), xe_min<size_t>(100, t.second.length()), t.second.c_str());
	}else{
		xe_log_trace(this, "<< GET %.*s", path.length(), path.data());
	}
#endif
	return 0;
}

int xe_http_singleconnection::handle_status_line(xe_http_version version, uint status, const xe_string_view& reason){
	return 0;
}

int xe_http_singleconnection::handle_header(const xe_string_view& key, const xe_string_view& value){
	if(key.equal_case("Content-Length")){
		if(transfer_mode != TRANSFER_MODE_NONE)
			return 0;
		ulong clen = 0;
		size_t read = xe_read_integer(XE_DECIMAL, clen, value.data(), value.length());

		if(read != value.length()){
			xe_log_error(this, read == (size_t)-1 ? "content length overflowed" : "invalid content length");

			return XE_INVALID_RESPONSE;
		}

		data_len = clen;
		transfer_mode = TRANSFER_MODE_CONTENTLENGTH;
	}else if(key.equal_case("Transfer-Encoding")){
		size_t start = 0, index;
		xe_string_view str;

		while(start < value.length()){
			index = key.index_of(',', start);

			if(index != (size_t)-1){
				str = value.substring(start, index);
				index++;

				while(index < value.length() && value[index] == ' ')
					index++;
				start = index;
			}else{
				str = value.substring(start);
				start = value.length();
			}

			if(str.equal_case("chunked"))
				transfer_mode = TRANSFER_MODE_CHUNKS;
		}
	}else if(key.equal_case("Connection")){
		if(value.equal_case("keep-alive"))
			connection_close = false;
	}else if(specific -> get_follow_location() && key.equal_case("Location")){
		location.clear();

		if(!location.copy(value))
			return XE_ENOMEM;
		follow = true;

		return 0;
	}

	return 0;
}

inline int xe_http_singleconnection::read_line(byte*& buf, size_t& len, xe_string_view& line){
	size_t next, line_end;
	byte* line_buf;

	next = xe_string_view((char*)buf, len).index_of('\n');

	if(next == (size_t)-1){
		if(len >= HEADERBUFFER_SIZE - header_offset || len >= MAXIMUM_HEADER_SIZE - header_total){
			/* would fill the buffer and have no space for newline */
			return XE_HEADERS_TOO_LONG;
		}

		if(!header_buffer){
			header_buffer = xe_alloc_aligned<byte>(0, HEADERBUFFER_SIZE);

			if(!header_buffer) return XE_ENOMEM;
		}

		xe_memcpy(header_buffer + header_offset, buf, len);

		header_offset += len;
		header_total += len;

		return XE_EAGAIN;
	}

	line_end = next++;

	if(next > MAXIMUM_HEADER_SIZE - header_total)
		return XE_HEADERS_TOO_LONG;
	header_total += next;

	if(header_offset) [[unlikely]] {
		if(line_end){
			if(line_end > HEADERBUFFER_SIZE - header_offset)
				return XE_HEADERS_TOO_LONG;
			xe_memcpy(header_buffer + header_offset, buf, line_end);
		}

		line_end += header_offset;
		line_buf = header_buffer;
		header_offset = 0;
	}else{
		line_buf = buf;
	}

	buf += next;
	len -= next;

	if(line_end > 0 && line_buf[line_end - 1] == '\r')
		line_end--;
	line = xe_string_view((char*)line_buf, line_end);

	return 0;
}

int xe_http_singleconnection::parse_headers(byte* buf, size_t len){
	xe_string_view line;
	int err;

	while(true){
		if(!len)
			return 0;
		if(read_state == READ_STATUSLINE && !statusline_prefix_checked){
			size_t min_len = xe_min(http_prefix.length() - header_offset, len);
			xe_string_view data((char*)buf, min_len);

			if(!http_prefix.substring(header_offset, min_len + header_offset).equal_case(data)){
				if(specific -> min_version == XE_HTTP_VERSION_0_9){
					xe_log_verbose(this, "not a valid status line, assuming http 0.9");
					xe_return_error(handle_status_line(XE_HTTP_VERSION_0_9, 200, "OK"));

					goto write_header_buffer;
				}else{
					goto invalid_status_line;
				}
			}

			if(header_offset + min_len >= http_prefix.length()) statusline_prefix_checked = true;
		}

		err = read_line(buf, len, line);

		if(err == XE_EAGAIN)
			return 0;
		xe_return_error(err);

		if(read_state == READ_STATUSLINE){
			xe_http_version version;
			uint status;
			xe_string_view reason;

			read_state = READ_HEADER;

			if(!parse_status_line(line, version, status, reason))
				goto invalid_status_line;
			xe_log_trace(this, ">> %s %u %.*s", http_version_to_string(version), status, reason.length(), reason.data());
			xe_return_error(handle_status_line(version, status, reason));

			if((status >= 100 && status < 200) || status == 204 || status == 304) bodyless = true;
		}else{
			if(!line.length())
				break;
			xe_string_view key, value;

			header_parse(line, key, value);

			if(!value)
				xe_log_warn(this, "header separator not found");
			xe_log_trace(this, ">> %.*s: %.*s", key.length(), key.data(), xe_min<size_t>(100, value.length()), value.data());
			xe_return_error(handle_header(key, value));
		}
	}

	xe_return_error(pretransfer());

	if(read_state != READ_NONE && len)
		return write_body(buf, len);
	return 0;
write_header_buffer:
	xe_return_error(pretransfer());

	if(read_state == READ_NONE)
		return 0;
	if(header_offset){
		xe_return_error(write_body(header_buffer, header_offset));

		if(read_state == READ_NONE) return 0;
	}

	return len ? write_body(buf, len) : 0;
invalid_status_line:
	xe_log_error(this, "invalid http status line");

	return XE_INVALID_RESPONSE;
}

int xe_http_singleconnection::parse_trailers(byte* buf, size_t len){
	xe_string_view line;
	int err;

	while(len){
		err = read_line(buf, len, line);

		if(err == XE_EAGAIN)
			break;
		xe_return_error(err);

		if(line.length()){
			xe_string_view key, value;

			header_parse(line, key, value);

			if(!value)
				xe_log_warn(this, "header separator not found");
			xe_log_trace(this, ">> %.*s: %.*s", key.length(), key.data(), xe_min<size_t>(100, value.length()), value.data());
			xe_return_error(handle_trailer(key, value));
		}else{
			xe_return_error(posttransfer());

			return len ? XE_ECANCELED : 0;
		}
	}

	return 0;
}

bool xe_http_singleconnection::chunked_save(byte* buf, size_t len){
	if(!header_buffer){
		header_buffer = xe_alloc<byte>(HEADERBUFFER_SIZE);

		if(!header_buffer) return false;
	}

	if(len > HEADERBUFFER_SIZE - header_offset)
		return false;
	xe_memcpy(header_buffer + header_offset, buf, len);

	header_offset += len;

	return true;
}

int xe_http_singleconnection::chunked_body(byte* buf, size_t len){
	ulong write;
	size_t result;

	while(len){
		switch(chunked_state){
			case CHUNKED_READ_SIZE:
				result = xe_read_integer(XE_HEX, data_len, buf, len);

				if(result == (size_t)-1){
					xe_log_error(this, "chunk size overflowed");

					return XE_INVALID_RESPONSE;
				}

				buf += result;
				len -= result;

				if(len){
					xe_log_trace(this, ">> chunk %lu", data_len);

					chunked_state = CHUNKED_READ_EXTENSION;
				}

				break;
			case CHUNKED_READ_DATA:
				write = data_len;

				if(len < write)
					write = len;
				else
					chunked_state = CHUNKED_READ_END;
				data_len -= write;

				if(recv_paused){
					if(!chunked_save(buf, write))
						return XE_ENOMEM;
				}else if(!client_write(buf, write)){
					return XE_ECANCELED;
				}

				len -= write;
				buf += write;

				break;
			case CHUNKED_READ_EXTENSION:
			case CHUNKED_READ_END:
				result = xe_string_view((char*)buf, len).index_of('\n');

				if(result == (size_t)-1){
					len = 0;

					break;
				}

				len -= result + 1;
				buf += result + 1;

				if(chunked_state == CHUNKED_READ_EXTENSION){
					if(data_len)
						chunked_state = CHUNKED_READ_DATA;
					else{
						header_total = 0;
						chunked_state = CHUNKED_READ_TRAILERS;
					}
				}else{
					chunked_state = CHUNKED_READ_SIZE;
				}

				break;
			case CHUNKED_READ_TRAILERS:
				return parse_trailers(buf, len);
		}
	}

	return 0;
}

bool xe_http_singleconnection::client_write(xe_ptr buf, size_t len){
	if(follow)
		return true;
	xe_log_trace(this, "<< client %zu", len);

	return !request -> write(buf, len);
}

int xe_http_singleconnection::pretransfer(){
	if(bodyless){
		xe_return_error(posttransfer());

		return 0;
	}

	read_state = READ_BODY;

	switch(transfer_mode){
		case TRANSFER_MODE_NONE:
			transfer_mode = TRANSFER_MODE_CONNECTION;

			break;
		case TRANSFER_MODE_CONTENTLENGTH:
			if(!data_len)
				xe_return_error(posttransfer());
			break;
		case TRANSFER_MODE_CHUNKS:
			data_len = 0;
			chunked_state = CHUNKED_READ_SIZE;

			break;
	}

	return 0;
}

int xe_http_singleconnection::posttransfer(){
	read_state = READ_NONE;

	return 0;
}

int xe_http_singleconnection::write_body(byte* buf, size_t len){
	switch(transfer_mode){
		case TRANSFER_MODE_CONNECTION:
			if(!len)
				xe_return_error(posttransfer());
			else if(!client_write(buf, len))
				return XE_ECANCELED;
			break;
		case TRANSFER_MODE_CONTENTLENGTH: {
			ulong write;

			if(!data_len) return XE_ECANCELED;
			if(!len) return XE_PARTIAL_FILE;

			write = xe_min(data_len, len);

			if(!client_write(buf, write))
				return XE_ECANCELED;
			data_len -= write;

			if(!data_len){
				xe_return_error(posttransfer());

				if(len > write) return XE_ECANCELED;
			}

			break;
		}

		case TRANSFER_MODE_CHUNKS:
			return chunked_body(buf, len);
	}

	return 0;
}

int xe_http_singleconnection::transferctl(uint flags){
	if(transfer_mode == TRANSFER_MODE_CHUNKS && read_state == READ_BODY && header_offset){
		if(!client_write(header_buffer, header_offset))
			return XE_ECANCELED;
		header_offset = 0;
	}

	xe_return_error(xe_connection::transferctl(flags));

	return 0;
}

int xe_http_singleconnection::handle_trailer(const xe_string_view& key, const xe_string_view& value){
	return 0;
}

int xe_http_singleconnection::open(xe_request_internal& req){
	int err;

	if(request_active)
		return XE_STATE;
	proto.available(*this, false);
	request = &req;
	specific = (xe_http_common_specific*)req.data;
	specific -> connection = this;

	if(state != XE_CONNECTION_STATE_ACTIVE){
		set_request_state(state);

		return 0;
	}

	if(timer.active())
		stop_timer();
	if((err = transferctl(XE_RESUME_RECV)))
		goto err;
	if((err = start()))
		goto err;
	return 0;
err:
	request_active = false;
	specific -> connection = null;
	specific = null;
	request = null;

	return err;
}

int xe_http_singleconnection::transferctl(xe_request_internal& request, uint flags){
	return transferctl(flags);
}

void xe_http_singleconnection::end(xe_request_internal& req){
	close(XE_ECANCELED);
}

xe_cstr xe_http_singleconnection::class_name(){
	return "xe_http_singleconnection";
}