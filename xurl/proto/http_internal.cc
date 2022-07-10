#include "http_internal.h"

using namespace xurl;

bool xe_http_string::copy(const xe_string& src){
	free();

	if(!copy(src))
		return false;
	owner = true;

	return true;
}

void xe_http_string::own(const xe_string& src){
	operator=(src);

	owner = true;
}

void xe_http_string::free(){
	if(owner)
		xe_string::free();
	owner = false;
}

xe_http_string& xe_http_string::operator=(const xe_string& src){
	free();
	xe_string::operator=(src);

	return *this;
}

xe_http_string::~xe_http_string(){
	free();
}

void xe_http_connection::close(int error){
	xe_connection::close(error);

	proto.closed(*this);
}

void xe_http_protocol::redirect(xe_request& request, xe_string&& url){
	request.complete(0);
}

bool xe_http_protocol::available(xe_http_connection& connection, bool available){

	return true;
}

void xe_http_protocol::closed(xe_http_connection& connection){
	xe_delete(&connection);
}

int xe_http_protocol::open(xe_http_internal_data& data, xe_url url, bool redirect){
	if(redirect)
		data.url.free();
	else
		data.free();
	data.url = url;
	data.method = "GET";

	return data.internal_set_header("Host", url.host(), false) ? 0 : XE_ENOMEM;
}

static bool copy_string(xe_http_string& dest, xe_string& src, uint flags){
	if(flags & XE_HTTP_COPY)
		return dest.copy(src);
	if(flags & XE_HTTP_FREE)
		dest.own(src);
	else
		dest = src;
	return true;
}

xe_http_internal_data::xe_http_internal_data(){
	min_version = XE_HTTP_VERSION_0_9;
	max_version = XE_HTTP_VERSION_1_1;
	headers.init();
}

bool xe_http_internal_data::internal_set_method(xe_string method_, uint flags){
	return copy_string(method, method_, flags);
}

bool xe_http_internal_data::internal_set_header(xe_string rkey, xe_string rvalue, uint flags){
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

void xe_http_internal_data::free(){
	headers.clear();
	url.free();
}

xe_http_internal_data::~xe_http_internal_data(){
	free();
}

xe_string xe_http_common_data::location(){
	return ((xe_http_common_specific*)this) -> url.string();
}

bool xe_http_common_data::set_header(xe_string key, xe_string value, uint flags){
	return ((xe_http_common_specific*)this) -> internal_set_header(key, value, flags);
}

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

static inline bool read_number(xe_string& line, uint& out, uint& i){
	out = 0;

	if(i >= line.length() || !xe_char_is_digit(line[i]))
		return false;
	while(i < line.length() && xe_char_is_digit(line[i])){
		if(out > UINT_MAX / 10)
			return false;
		out = out * 10 + xe_digit_to_int(line[i++]);
	}

	return true;
}

static inline bool read_digit(xe_string& line, uint& out, uint& index){
	if(!xe_char_is_digit(line[index]))
		return false;
	out = xe_digit_to_int(line[index++]);

	return true;
}

static inline bool read_version(xe_string& line, uint& off, uint& version){
	uint major, minor;

	if(!read_digit(line, major, off))
		return false;
	if(line[off++] != '.')
		return false;
	if(!read_digit(line, minor, off))
		return false;
	version = major * 10 + minor;

	return version >= 9 && version <= 11;
}

static inline void header_parse(xe_string& line, xe_string& key, xe_string& value){
	size_t index = line.index_of(':');

	if(index != -1){
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
		default:
		case XE_HTTP_VERSION_1_1:
			return "HTTP/1.1";
	}
}

static bool build_headers(xe_vector<char>& headers, xe_http_common_specific& specific, xe_http_version version){
	xe_string crlf = "\r\n";
	xe_string ws = " ";
	xe_string separator = ": ";

	size_t len;

	xe_string http_ver = http_version_to_string(version);
	xe_string path = specific.url.path();

	if(!path.length())
		path = "/";
	len = specific.method.length() + ws.length() + path.length();

	if(version != XE_HTTP_VERSION_0_9){
		len += ws.length() + http_ver.length();

		for(auto& t : specific.headers)
			len += t.first.length() + separator.length() + t.second.length() + crlf.length();
		len += crlf.length();
	}

	len += crlf.length();

	if(!headers.resize(len))
		return false;
	headers.resize(0);
	headers.append(specific.method);
	headers.append(ws);
	headers.append(path);

	if(version != XE_HTTP_VERSION_0_9){
		headers.append(ws);
		headers.append(http_ver);
	}

	headers.append(crlf);

	if(version != XE_HTTP_VERSION_0_9){
		for(auto& t : specific.headers){
			headers.append(t.first);
			headers.append(separator);
			headers.append(t.second);
			headers.append(crlf);
		}

		headers.append(crlf);
	}

	return true;
}

void xe_http_singleconnection::set_state(xe_connection_state state){
	xe_connection::set_state(state);

	if(!request)
		return;
	if(state == XE_CONNECTION_STATE_RESOLVING)
		request -> set_state(XE_REQUEST_STATE_DNS);
	else if(state == XE_CONNECTION_STATE_CONNECTING)
		request -> set_state(XE_REQUEST_STATE_CONNECTING);
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

ssize_t xe_http_singleconnection::data(xe_ptr data, size_t size){
	int error;

	if(read_state < READ_BODY){
		if(!size)
			return XE_PARTIAL_FILE;
		error = parse_headers((xe_bptr)data, size);
	}else{
		error = write_body((xe_bptr)data, size);
	}

	if(read_state == READ_NONE){
		bodyless = false;
		transfer_active = false;

		if(connection_close)
			return 0;
		if(!error && proto.available(*this, true))
			return 0;
		if(request_active)
			complete(0);
		if(!request_active)
			transferctl(XE_PAUSE_ALL);
	}

	return error ? error : size;
}

void xe_http_singleconnection::close(int error){
	proto.available(*this, false);

	if(request_active)
		complete(error);
	xe_http_connection::close(error);
}

void xe_http_singleconnection::complete(int error){
	xe_request& req = *request;

	request_active = false;
	specific -> connection = null;
	request = null;
	specific = null;

	if(!error && follow){
		follow = false;
		proto.redirect(req, std::move(location));
	}else{
		location.free();
		req.complete(error);
	}

	location.free();
}

int xe_http_singleconnection::send_headers(){
	ssize_t sent = send(client_headers.data() + send_offset, client_headers.size() - send_offset);

	xe_assert(client_headers.size());
	xe_log_trace(this, "sent %zi bytes", sent);

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
		transferctl(XE_RESUME_SEND);
	}

	return 0;
}

int xe_http_singleconnection::start(){
	xe_http_version version = specific -> max_version;
	int err;

	request_active = true;
	transfer_active = true;
	connection_close = true;
	read_state = READ_STATUSLINE;
	header_offset = 0;
	header_total = 0;
	chunked_state = 0;
	transfer_mode = 0;
	send_offset = 0;
	data_len = 0;
	follow = false;
	location.free();

	if(version == XE_HTTP_VERSION_0_9){
		read_state = READ_BODY;
		transfer_mode = TRANSFER_MODE_CONNECTION;
	}else if(specific -> method == "HEAD"){
		bodyless = true;
	}

	if(!build_headers(client_headers, *specific, version))
		return XE_ENOMEM;
	xe_return_error(send_headers());

	request -> set_state(XE_REQUEST_STATE_ACTIVE);
#ifdef XE_DEBUG
	xe_string path = specific -> url.path();

	if(!path.size())
		path = "/";
	if(version != XE_HTTP_VERSION_0_9){
		xe_log_trace(this, "%.*s %.*s %s", specific -> method.length(), specific -> method.c_str(), path.length(), path.c_str(), http_version_to_string(version));

		for(auto& t : specific -> headers)
			xe_log_trace(this, "%.*s: %.*s", t.first.length(), t.first.c_str(), xe_min<size_t>(100, t.second.length()), t.second.c_str());
	}else{
		xe_log_trace(this, "GET %.*s", path.length(), path.c_str());
	}
#endif
	return 0;
}

bool xe_http_singleconnection::parse_status_line(xe_string& line, xe_http_version& version, uint& status, xe_string& reason){
	if(line.length() < xe_string("HTTP/0.0 0").length())
		return false;
	xe_string begin = "HTTP/";

	if(!line.substring(0, begin.length()).equal_case(begin))
		return false;
	uint off = begin.length();

	if(!read_version(line, off, (uint&)version))
		return false;
	if(line[off++] != ' ')
		return false;
	if(!read_number(line, status, off))
		return false;
	if(off < line.length()){
		if(line[off++] != ' ')
			return false;
		reason = line.substring(off);
	}

	return true;
}

int xe_http_singleconnection::handle_header(xe_string& key, xe_string& value){
	if(key.equal_case("Content-Length")){
		if(transfer_mode != TRANSFER_MODE_NONE)
			return 0;
		ulong clen = 0;

		for(size_t i = 0; i < value.length(); i++){
			if(!xe_char_is_digit(value[i]) || clen > ULONG_MAX / 10){
				xe_log_error(this, "invalid content length header");

				return XE_INVALID_RESPONSE;
			}

			clen = 10 * clen + xe_digit_to_int(value[i]);
		}

		data_len = clen;
		transfer_mode = TRANSFER_MODE_CONTENTLENGTH;
	}else if(key.equal_case("Transfer-Encoding")){
		size_t start = 0, index;
		xe_string str;

		while(start < value.length()){
			index = key.index_of(',', start);

			if(index != -1){
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
		location.free();

		if(!location.copy(value))
			return XE_ENOMEM;
		follow = true;

		return 0;
	}

	return 0;
}

inline int xe_http_singleconnection::read_line(xe_bptr& buf, size_t& len, xe_string& line){
	size_t next, line_end;
	xe_bptr line_buf;

	next = xe_string(buf, len).index_of('\n');

	if(next == -1){
		if(len >= HEADERBUFFER_SIZE - header_offset || len >= MAXIMUM_HEADER_SIZE - header_total) /* would fill the buffer and have no space for newline */
			return XE_HEADERS_TOO_LONG;
		if(!header_buffer){
			header_buffer = xe_alloc<byte>(HEADERBUFFER_SIZE);

			if(!header_buffer)
				return XE_ENOMEM;
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

	if(header_offset){
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
	line = xe_string(line_buf, line_end);

	return 0;
}

int xe_http_singleconnection::parse_headers(xe_bptr buf, size_t len){
	xe_string line;
	int err;

	while(len){
		err = read_line(buf, len, line);

		if(err == XE_EAGAIN)
			return 0;
		xe_return_error(err);

		if(line.length()){
			if(read_state == READ_STATUSLINE){
				xe_http_version version;
				uint status;
				xe_string reason;

				read_state = READ_HEADER;

				if(!parse_status_line(line, version, status, reason))
					return XE_INVALID_RESPONSE;
				if((status >= 100 && status < 200) || status == 204 || status == 304)
					bodyless = true;
				xe_log_trace(this, "HTTP/%u.%u %u %.*s", version / 10, version % 10, status, reason.length(), reason.c_str());
				xe_return_error(handle_statusline(version, status, reason));
			}else{
				xe_string key, value;

				header_parse(line, key, value);

				if(!value)
					xe_log_warn(this, "header separator not found");
				xe_log_trace(this, "%.*s: %.*s", key.length(), key.c_str(), value.length(), value.c_str());
				xe_return_error(handle_header(key, value));
				xe_return_error(handle_singleheader(key, value));
			}
		}else{
			break;
		}
	}

	read_state = READ_BODY;

	if(bodyless){
		read_state = READ_NONE;

		return 0;
	}

	switch(transfer_mode){
		case TRANSFER_MODE_NONE:
			transfer_mode = TRANSFER_MODE_CONNECTION;

			break;
		case TRANSFER_MODE_CONTENTLENGTH:
			if(!data_len){
				read_state = READ_NONE;

				return len ? XE_ABORTED : 0;
			}

			break;
		case TRANSFER_MODE_CHUNKS:
			data_len = 0;
			chunked_state = CHUNKED_READ_SIZE;

			break;
	}

	if(len)
		xe_return_error(write_body(buf, len));
	return 0;
}

int xe_http_singleconnection::parse_trailers(xe_bptr buf, size_t len){
	xe_string line;
	int err;

	while(len){
		err = read_line(buf, len, line);

		if(err == XE_EAGAIN)
			break;
		xe_return_error(err);

		if(line.length()){
			xe_string key, value;

			header_parse(line, key, value);

			xe_return_error(handle_trailer(key, value));
		}else{
			read_state = READ_NONE;

			if(len)
				return XE_ABORTED;
			break;
		}
	}

	return 0;
}

int xe_http_singleconnection::chunked_body(xe_bptr buf, size_t len){
	ulong write;

	while(len){
		switch(chunked_state){
			case CHUNKED_READ_SIZE:
				while(len){
					if(xe_char_is_hex(*buf)){
						if(data_len > ULONG_MAX >> 4){
							xe_log_error(this, "chunk size exceeds maximum value");

							return XE_INVALID_RESPONSE;
						}

						data_len = (data_len << 4) + xe_hex_to_int(*buf);
					}else{
						chunked_state = CHUNKED_READ_EXTENSION;

						break;
					}

					buf++;
					len--;
				}

				break;
			case CHUNKED_READ_EXTENSION: {
				size_t index = xe_string(buf, len).index_of('\n');

				if(index != -1){
					len -= index + 1;
					buf += index + 1;

					if(data_len)
						chunked_state = CHUNKED_READ_DATA;
					else{
						header_total = 0;
						chunked_state = CHUNKED_READ_TRAILERS;
					}
				}else{
					len = 0;
				}

				break;
			}

			case CHUNKED_READ_DATA:
				write = data_len;

				if(len < write)
					write = len;
				else
					chunked_state = CHUNKED_READ_END;
				data_len -= write;

				if(!client_write(buf, write))
					return XE_ABORTED;
				len -= write;
				buf += write;

				break;
			case CHUNKED_READ_END: {
				size_t index = xe_string(buf, len).index_of('\n');

				if(index != -1){
					len -= index + 1;
					buf += index + 1;

					chunked_state = CHUNKED_READ_SIZE;
				}else{
					len = 0;
				}

				break;
			}

			case CHUNKED_READ_TRAILERS:
				return parse_trailers(buf, len);
		}
	}

	return 0;
}

bool xe_http_singleconnection::client_write(xe_ptr buf, size_t len){
	if(follow)
		return true;
	xe_log_trace(this, "client write %zu bytes", len);

	if(request -> callbacks.write && request -> callbacks.write(*request, buf, len))
		return false;
	return true;
}

int xe_http_singleconnection::write_body(xe_bptr buf, size_t len){
	switch(transfer_mode){
		case TRANSFER_MODE_CONNECTION:
			if(!len)
				read_state = READ_NONE;
			else if(!client_write(buf, len))
				return XE_ABORTED;
			break;
		case TRANSFER_MODE_CONTENTLENGTH: {
			ulong write = xe_min(data_len, len);

			if(!len)
				return XE_PARTIAL_FILE;
			if(!client_write(buf, write))
				return XE_ABORTED;
			data_len -= write;

			if(!data_len){
				read_state = READ_NONE;

				if(len > write)
					return XE_ABORTED;
			}

			break;
		}

		case TRANSFER_MODE_CHUNKS:
			return chunked_body(buf, len);
	}

	return 0;
}

int xe_http_singleconnection::transferctl(uint flags){
	// TODO chunked transfer
	return xe_connection::transferctl(flags);
}

int xe_http_singleconnection::handle_statusline(xe_http_version version, uint status, xe_string& reason){
	return 0;
}

int xe_http_singleconnection::handle_singleheader(xe_string& key, xe_string& value){
	return 0;
}

int xe_http_singleconnection::handle_trailer(xe_string& key, xe_string& value){
	return 0;
}

int xe_http_singleconnection::open(xe_request& req){
	if(request_active)
		return XE_EINVAL;
	proto.available(*this, false);
	request = &req;
	specific = (xe_http_common_specific*)req.data;
	specific -> connection = this;

	if(state != XE_CONNECTION_STATE_ACTIVE)
		return 0;
	if(peer_closed())
		return XE_ECONNRESET;
	transferctl(XE_RESUME_RECV);

	return start();
}

int xe_http_singleconnection::transferctl(xe_request& request, uint flags){
	return transferctl(flags);
}

void xe_http_singleconnection::end(xe_request& request){
	if(request_active){
		connection_close = true;

		close(XE_ABORTED);
	}
}

xe_http_singleconnection::~xe_http_singleconnection(){
	xe_dealloc(header_buffer);

	client_headers.free();
}

xe_cstr xe_http_singleconnection::class_name(){
	return "xe_http_singleconnection";
}