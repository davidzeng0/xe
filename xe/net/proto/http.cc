#include "../request.h"
#include "http.h"
#include "../conn.h"

using namespace xe_net;
using namespace xe_hash;

struct xe_http_host{
	xe_string hostname;

	union{
		uint num;

		struct{
			ushort port;
			ushort secure;
		};
	};

	size_t hash() const{
		return hash_combine(hash_int(num), hostname.hash());
	}

	bool operator==(const xe_http_host& other) const{
		return num == other.num && hostname == other.hostname;
	}
};

class xe_option_string : public xe_string{
public:
	bool clone;

	xe_option_string(){
		clone = false;
	}

	xe_option_string(xe_cstr string): xe_string(string){
		clone = false;
	}

	xe_option_string(const xe_string& string): xe_string(string){
		clone = false;
	}

	xe_option_string& operator=(xe_cstr string){
		free();

		xe_string::operator=(string);

		clone = false;

		return *this;
	}

	xe_option_string& operator=(const xe_string& other){
		free();

		xe_string::operator=(other);

		clone = false;

		return *this;
	}

	bool copy(xe_string& string, bool clone_){
		free();

		if(clone_){
			clone = clone_;

			return xe_string::copy(string);
		}

		clone = clone_;
		data_ = string.data();
		size_ = string.size();

		return true;
	}

	void free(){
		if(!clone){
			data_ = null;
			size_ = 0;
		}else{
			xe_string::free();
		}
	}

	bool operator==(const xe_option_string& other) const{
		return equalCase(other);
	}
};

class xe_http_connection;
struct xe_http_connection_list{
	xe_http_connection* list;

	bool has();

	xe_http_connection& get();

	void add(xe_http_connection& conn);
	void remove(xe_http_connection& conn);
};

class xe_http : public xe_protocol{
public:
	xe_map<xe_http_host, xe_http_connection_list*> connections;

	xe_http(xe_net_ctx& net);

	int start(xe_request& request);

	void pause(xe_request& request, bool paused);
	void end(xe_request& request);

	int open(xe_request& request, xe_url url);

	bool matches(const xe_string& scheme) const;

	int redirect(xe_request& request, xe_string url);

	~xe_http();
};

class xe_http_data : public xe_protocol_data, public xe_net_data{
public:
	xe_url url;
	xe_option_string method;
	xe_http_connection* connection;

	xe_map<xe_option_string, xe_option_string> headers;

	struct xe_callbacks{
		int (*response)(xe_request& request, uint status, xe_string reason);
		int (*header)(xe_request& request, xe_string key, xe_string value);
	} callbacks;

	uint redirects;

	bool ssl_verify;

	int set_method(xe_string& string, int flags);
	int set_header(xe_string& rkey, xe_string& rvalue, int flags);
	int set_callback(xe_ptr function, xe_ptr value, int flags);
	void free();

	xe_http_data();

	int open(xe_url url, bool redir = false);
	int set(int option, xe_ptr value, int flags);
	int set(int option, xe_ptr value1, xe_ptr value2, int flags);

	~xe_http_data();
};

class xe_http_connection : public xe_connection_handler{
public:
	enum xe_constants{
		HEADERBUFFER_SIZE = 131072,
		MAXIMUM_HEADER_SIZE = 1024 * 1024
	};

	enum xe_flags{
		FLAG_NONE = 0x0,
		FLAG_REQUEST_ACTIVE = 0x1,
		FLAG_TRANSFER_ACTIVE = 0x2,
		FLAG_CONNECTION_CLOSE = 0x4,
		FLAG_BODYLESS = 0x8
	};

	enum xe_state{
		READ_NONE = 0,
		READ_HEADER_STATUS,
		READ_HEADER,
		READ_BODY
	};

	enum xe_transfer_mode{
		MODE_NONE = 0,
		MODE_CONN,
		MODE_CONTENTLENGTH,
		MODE_CHUNKS
	};

	enum xe_transfer_encoding{
		ENCODING_NONE = 0,
		ENCODING_CHUNKED = 1
	};

	enum xe_chunked_state{
		CHUNKED_READ_NULL = 0,
		CHUNKED_READ_SIZE,
		CHUNKED_READ_EXTENSION,
		CHUNKED_READ_DATA,
		CHUNKED_READ_END,
		CHUNKED_READ_TRAILERS
	};

	xe_http& http;
	xe_http_connection_list& list;

	xe_http_connection* next;
	xe_http_connection* prev;

	xe_connection* connection;
	xe_request* request;
	xe_http_data* data;

	xe_vector<char> send_headers;
	size_t send_offset;

	byte* header_buffer;
	uint header_total;
	uint header_offset;

	uint flags;
	uint read_state;
	uint chunked_state;
	uint transfer_mode;
	uint transfer_encoding;

	ulong content_length;

	xe_string location;

	bool follow;
	bool in_list;

	xe_http_connection(xe_request* request, xe_http& http, xe_http_connection_list& list):
		http(http),
		list(list),
		request(request){
		data = (xe_http_data*)request -> data;
		data -> connection = this;
	}

	bool read_number(xe_string& line, uint& out, uint& i){
		out = 0;

		if(i >= line.length() || !xe_cisi(line[i]))
			return false;
		while(i < line.length() && xe_cisi(line[i])){
			if(out > UINT_MAX / 10)
				return false;
			out = out * 10 + xe_ctoi(line[i++]);
		}

		return true;
	}

	bool read_digit(xe_string& line, uint& out, uint& index){
		if(!xe_cisi(line[index]))
			return false;
		out = xe_ctoi(line[index++]);

		return true;
	}

	bool read_version(xe_string& line, uint& off, uint& major, uint& minor){
		uint version;

		if(!read_digit(line, major, off))
			return false;
		if(line[off++] != '.')
			return false;
		if(!read_digit(line, minor, off))
			return false;
		version = major * 10 + minor;

		return version >= 9 && version <= 11;
	}

	int write_status_line(xe_string& line){
		if(line.length() < xe_string("HTTP/0.0 0").length())
			return XE_INVALID_RESPONSE;
		xe_string begin = "http/";
		xe_string reason;

		if(!line.substring(0, begin.length()).equalCase(begin))
			return XE_INVALID_RESPONSE;
		uint major, minor, status;
		uint off = begin.length();

		if(!read_version(line, off, major, minor))
			return XE_INVALID_RESPONSE;
		if(line[off++] != ' ')
			return XE_INVALID_RESPONSE;
		if(!read_number(line, status, off))
			return XE_INVALID_RESPONSE;
		if(off < line.length()){
			if(line[off++] != ' ')
				return XE_INVALID_RESPONSE;
			reason = line.substring(off);
		}

		if(status >= 100 && status < 200 || status == 204 || status == 304)
			flags |= FLAG_BODYLESS;
		xe_log_debug("xe_http", this, "HTTP/%u.%u %u %.*s", major, minor, status, reason.length(), reason.c_str());

		if(data -> callbacks.response && data -> callbacks.response(*request, status, reason))
			return XE_ABORTED;
		return 0;
	}

	void header_parse(xe_string& line, xe_string& key, xe_string& value){
		size_t index = line.indexOf(':');

		if(index != -1){
			key = line.substring(0, index);
			index++;

			while(index < line.length() && line[index] == ' ')
				index++;
			value = line.substring(index);
		}else{
			key = line;

			xe_log_warn("xe_http", this, "header separator not found");
		}
	}

	int handle_header(xe_string& key, xe_string& value){
		if(key.equalCase("Content-Length")){
			if(transfer_mode != MODE_NONE)
				return 0;
			ulong clen = 0;

			for(size_t i = 0; i < value.length(); i++){
				if(!xe_cisi(value[i]) || clen > ULONG_MAX / 10){
					xe_log_error("xe_http", this, "content length header exceeds maximum value");

					return XE_INVALID_RESPONSE;
				}

				clen = 10 * clen + xe_ctoi(value[i]);
			}

			content_length = clen;
			transfer_mode = MODE_CONTENTLENGTH;
		}else if(key.equalCase("Transfer-Encoding")){
			size_t start = 0, index;
			xe_string str;

			while(start < value.length()){
				index = key.indexOf(',', start);

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

				if(str.equalCase("chunked")){
					transfer_mode = MODE_CHUNKS;
					transfer_encoding |= ENCODING_CHUNKED;
				}
			}
		}else if(key.equalCase("Connection")){
			if(value.equalCase("keep-alive"))
				flags &= ~FLAG_CONNECTION_CLOSE;
		}else if(data -> follow_location && key.equalCase("Location")){
			location.free();

			if(!location.copy(value))
				return XE_ENOMEM;
			follow = true;

			return 0;
		}

		return 0;
	}

	int write_header(xe_bptr buf, size_t len){
		size_t next, line_end;

		int err;

		while(true){
			next = xe_string(buf, len).indexOf('\n');

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

				break;
			}else{
				xe_bptr line_buf;

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
				if(line_end){
					xe_string line(line_buf, line_end);

					if(read_state == READ_HEADER_STATUS){
						if((err = write_status_line(line))){
							if(err == XE_INVALID_RESPONSE)
								xe_log_error("xe_http", this, "bad header line");
							return err;
						}

						read_state = READ_HEADER;
					}else{
						xe_string key, value;

						header_parse(line, key, value);
						xe_log_debug("xe_http", this, "%.*s: %.*s", key.length(), key.c_str(), value.length(), value.c_str());

						if((err = handle_header(key, value)))
							return err;
						if(data -> callbacks.header && data -> callbacks.header(*request, key, value))
							return XE_ABORTED;
					}
				}else{
					read_state = READ_BODY;

					if(flags & FLAG_BODYLESS)
						goto finish;
					switch(transfer_mode){
						case MODE_NONE:
							transfer_mode = MODE_CONN;

							break;
						case MODE_CONTENTLENGTH:
							if(!content_length)
								goto finish;
							break;
						case MODE_CHUNKS:
							content_length = 0;
							chunked_state = CHUNKED_READ_SIZE;

							break;
					}

					if(len){
						err = write_body(buf, len);

						if(err)
							return err;
					}

					break;

					finish:

					flags &= FLAG_CONNECTION_CLOSE;
					read_state = READ_NONE;

					finished(0);

					break;
				}
			}
		}

		return 0;
	}

	int write_trailer(xe_bptr buf, size_t len){
		size_t next, line_end;

		int err;

		while(true){
			next = xe_string(buf, len).indexOf('\n');

			if(next == -1){
				if(len >= HEADERBUFFER_SIZE - header_offset || len >= MAXIMUM_HEADER_SIZE - header_total)
					return XE_HEADERS_TOO_LONG;
				if(!header_buffer){
					header_buffer = xe_alloc<byte>(HEADERBUFFER_SIZE);

					if(!header_buffer)
						return XE_ENOMEM;
				}

				xe_memcpy(header_buffer + header_offset, buf, len);

				header_offset += len;
				header_total += len;

				break;
			}else{
				xe_bptr line_buf;

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
				if(line_end){
					xe_string line(line_buf, line_end);
					xe_string key, value;

					header_parse(line, key, value);

					if(data -> callbacks.header && data -> callbacks.header(*request, key, value))
						return XE_ABORTED;
				}else{
					flags &= FLAG_CONNECTION_CLOSE;
					read_state = READ_NONE;

					finished(0);

					if(len)
						return XE_ABORTED;
					break;
				}
			}
		}

		return 0;
	}

	int chunked_body(xe_bptr buf, size_t len){
		ulong write;

		while(len){
			switch(chunked_state){
				case CHUNKED_READ_SIZE:
					while(len){
						int d = xe_hex(*buf);

						if(d >= 0){
							if(content_length > ULONG_MAX >> 4){
								xe_log_error("xe_http", this, "chunk size exceeds maximum value");

								return XE_INVALID_RESPONSE;
							}

							content_length = (content_length << 4) + d;
						}else{
							chunked_state = CHUNKED_READ_EXTENSION;

							break;
						}

						buf++;
						len--;
					}

					break;
				case CHUNKED_READ_EXTENSION: {
					size_t index = xe_string(buf, len).indexOf('\n');

					if(index != -1){
						len -= index + 1;
						buf += index + 1;

						if(content_length)
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
					write = content_length;

					if(len < write)
						write = len;
					else
						chunked_state = CHUNKED_READ_END;
					content_length -= write;

					if(!client_write(buf, write))
						return XE_ABORTED;
					len -= write;
					buf += write;

					break;
				case CHUNKED_READ_END: {
					size_t index = xe_string(buf, len).indexOf('\n');

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
					return write_trailer(buf, len);
			}
		}

		return 0;
	}

	bool client_write(xe_ptr buf, size_t len){
		if(follow)
			return true;
		xe_log_trace("xe_http", this, "client write %zu bytes", len);

		if(request -> callbacks.write && request -> callbacks.write(*request, buf, len))
			return false;
		return true;
	}

	int write_body(xe_bptr buf, size_t len){
		switch(transfer_mode){
			case MODE_CONN:
				if(!len)
					finished(0);
				else if(!client_write(buf, len))
					return XE_ABORTED;
				break;
			case MODE_CONTENTLENGTH: {
				ulong write = xe_min(content_length, len);

				if(!len)
					return XE_PARTIAL_FILE;
				if(!client_write(buf, write))
					return XE_ABORTED;
				content_length -= write;

				if(!content_length){
					flags &= FLAG_CONNECTION_CLOSE;
					read_state = READ_NONE;

					finished(0);

					if(len > write)
						return XE_ABORTED;
					return 0;
				}

				break;
			}

			case MODE_CHUNKS:
				return chunked_body(buf, len);
		}

		return 0;
	}

	bool generate_headers(){
		xe_string crlf = "\r\n";
		xe_string ws = " ";
		xe_string separator = ": ";

		size_t len;
		xe_vector<char> header = send_headers;

		xe_string version = "HTTP/1.1";
		xe_string path = data -> url.path();

		if(!path.length())
			path = "/";
		len = version.length() + ws.length() + data -> method.length() + ws.length() + path.length() + crlf.length();

		for(auto& t : data -> headers)
			len += t.first.length() + separator.length() + t.second.length() + crlf.length();
		len += crlf.length();

		if(!header.resize(len))
			return false;
		header.resize(0);
		header.append(data -> method);
		header.append(ws);
		header.append(path);
		header.append(ws);
		header.append(version);
		header.append(crlf);

		for(auto& t : data -> headers){
			header.append(t.first);
			header.append(separator);
			header.append(t.second);
			header.append(crlf);
		}

		header.append(crlf);
		send_headers = header;

		return true;
	}

	void state_change(int state){
		if(state == XE_CONNECTION_STATE_RESOLVING)
			request -> set_state(XE_REQUEST_STATE_RESOLVING);
		else if(state == XE_CONNECTION_STATE_CONNECTING)
			request -> set_state(XE_REQUEST_STATE_CONNECTING);
	}

	int ready(){
		return open(*request);
	}

	int writable(){
		int sent = connection -> send(send_headers.data() + send_offset, send_headers.size() - send_offset);

		if(sent == 0)
			return XE_SEND_ERROR;
		if(sent < 0){
			if(sent != XE_EAGAIN)
				return sent;
			return 0;
		}

		send_offset += sent;

		if(send_offset >= send_headers.size()){
			send_headers.resize(0);

			return connection -> poll_writable(false);
		}

		return 0;
	}

	ssize_t write(xe_ptr data, size_t size){
		if(!(flags & FLAG_TRANSFER_ACTIVE))
			return XE_ABORTED;
		int error;

		if(read_state > READ_NONE && read_state < READ_BODY){
			if(!size)
				return XE_PARTIAL_FILE;
			error = write_header((xe_bptr)data, size);
		}else{
			error = write_body((xe_bptr)data, size);
		}

		if(error)
			return error;
		if(read_state == READ_NONE){
			if(flags & FLAG_CONNECTION_CLOSE)
				return 0;
			else{
				keepalive();
				list_add();
			}
		}

		return size;
	}

	void list_add(){
		if(in_list)
			return;
		list.add(*this);
		in_list = true;
	}

	void list_remove(){
		if(!in_list)
			return;
		list.remove(*this);
		in_list = false;
	}

	void finished(int error){
		if(!(flags & FLAG_CONNECTION_CLOSE))
			list_add();
		xe_request* req = request;
		bool redirected = false;

		if(req){
			request = null;
			data -> connection = null;

			if(!error && follow){
				follow = false;

				if(++data -> redirects > data -> max_redirects)
					error = XE_TOO_MANY_REDIRECTS;
				else{
					error = http.redirect(*req, location);
					location.clear();

					if(!error)
						redirected = true;
				}
			}

			location.free();

			if(!redirected)
				req -> finished(error);
		}

		list_remove();
	}

	void closed(int error){
		list_remove();
		finished(error);

		send_headers.free();

		xe_delete(connection);
		xe_delete(this);
	}

	int open(xe_request& request_){
		list_remove();

		request = &request_;
		data = (xe_http_data*)request_.data;
		data -> connection = this;

		pause(false);

		if(!generate_headers())
			return XE_ENOMEM;
		ssize_t sent = connection -> send(send_headers.data(), send_headers.size());

		if(sent <= 0){
			if(sent == 0)
				return XE_SEND_ERROR;
			if(sent != XE_EAGAIN)
				return sent;
		}

		if(sent == XE_EAGAIN || sent < send_headers.size()){
			send_offset = 0;

			if(sent > 0)
				send_offset = sent;
			if((sent = connection -> poll_writable(true)))
				return sent;
		}

		flags = FLAG_REQUEST_ACTIVE | FLAG_TRANSFER_ACTIVE | FLAG_CONNECTION_CLOSE;
		read_state = READ_HEADER_STATUS;
		header_offset = 0;
		header_total = 0;
		chunked_state = 0;
		transfer_mode = 0;
		transfer_encoding = 0;
		content_length = 0;
		follow = false;
		location.free();

		if(data -> method == "HEAD")
			flags |= FLAG_BODYLESS;
		request -> set_state(XE_REQUEST_STATE_ACTIVE);

#ifdef XE_DEBUG
		xe_string path = data -> url.path();

		if(!path.size())
			path = "/";
		xe_log_debug("xe_http", this, "%.*s %.*s HTTP/1.1", data -> method.length(), data -> method.c_str(), path.length(), path.c_str());

		for(auto t = data -> headers.begin(); t != data -> headers.end(); t++)
			xe_log_debug("xe_http", this, "%.*s: %.*s", t -> first.length(), t -> first.c_str(), xe_min((size_t)100, t -> second.length()), t -> second.c_str());
#endif
		return 0;
	}

	void pause(bool pause){
		connection -> pause(pause); // TODO handle errors
	}

	void keepalive(){
		connection -> keepalive();
	}

	void end(xe_request& request){
		if(flags & FLAG_REQUEST_ACTIVE){
			flags |= FLAG_CONNECTION_CLOSE;

			finished(XE_ABORTED);

			connection -> close();
		}
	}

	~xe_http_connection(){
		xe_dealloc(header_buffer);
	}
};

bool xe_http_connection_list::has(){
	return list != null;
}

xe_http_connection& xe_http_connection_list::get(){
	return *list;
}

void xe_http_connection_list::add(xe_http_connection& conn){
	xe_assert(conn.next == null && conn.prev == null);

	if(list)
		list -> prev = &conn;
	conn.next = list;
	list = &conn;
}

void xe_http_connection_list::remove(xe_http_connection& conn){
	xe_assert((conn.prev == null) == (&conn == list));

	if(conn.next)
		conn.next -> prev = conn.prev;
	if(conn.prev)
		conn.prev -> next = conn.next;
	else
		list = conn.next;
	conn.next = null;
	conn.prev = null;
}

xe_http::xe_http(xe_net_ctx& net): xe_protocol(net, XE_PROTOCOL_HTTP){
	connections.init();
}

int xe_http::start(xe_request& request){
	xe_http_data& data = *(xe_http_data*)request.data;

	uint port = data.port;
	int err;

	bool secure = data.url.scheme().length() == 5;

	if(!port)
		port = data.url.port();
	if(!port){
		port = secure ? 443 : 80;

		xe_log_debug("xe_http", this, "using default port %u", port);
	}

	xe_http_host host;

	host.hostname = data.url.host();
	host.port = port;
	host.secure = secure;

	auto conn = connections.find(host);

	xe_http_connection_list* list;

	if(conn != connections.end()){
		list = conn -> second;

		if(list -> has()){
			err = list -> get().open(request);

			if(!err)
				return 0;
		}
	}else{
		list = xe_zalloc<xe_http_connection_list>();

		if(!list)
			return XE_ENOMEM;
		if(!host.hostname.copy(data.url.host())){
			xe_dealloc(list);

			return XE_ENOMEM;
		}

		if(!connections.insert(host, list)){
			xe_dealloc(list);

			host.hostname.free();

			return XE_ENOMEM;
		}
	}

	xe_http_connection* conn_data = xe_znew<xe_http_connection>(&request, *this, *list);

	if(!conn_data)
		return XE_ENOMEM;
	xe_connection* connection = xe_zalloc<xe_connection>();

	if(!connection){
		xe_delete(conn_data);

		return XE_ENOMEM;
	}

	connection -> init(net, *conn_data);
	conn_data -> connection = connection;
	err = 0;

	connection -> set_ssl_verify(data.ssl_verify);
	connection -> set_connect_timeout(data.connect_timeout);
	connection -> set_ip_mode(data.ip_mode);
	connection -> set_recvbuf_size(data.recvbuf_size);

	if(secure)
		err = connection -> init_ssl(net.get_ssl_ctx());
	if(!err)
		err = connection -> connect(data.url.host(), port);
	if(err){
		xe_delete(conn_data);
		xe_delete(connection);
	}

	return err;
}

void xe_http::pause(xe_request& request, bool paused){
	xe_http_data& data = *(xe_http_data*)request.data;

	data.connection -> pause(paused);
}

void xe_http::end(xe_request& request){
	xe_http_data& data = *(xe_http_data*)request.data;

	data.connection -> end(request);
}

int xe_http::open(xe_request& request, xe_url url){
	xe_http_data* data;

	if(request.data && request.data -> id() == XE_PROTOCOL_HTTP)
		data = (xe_http_data*)request.data;
	else{
		data = xe_new<xe_http_data>();

		if(!data)
			return XE_ENOMEM;
	}

	int err = data -> open(url);

	if(err){
		if(data != request.data)
			xe_delete(data);
		return err;
	}

	xe_log_verbose("xe_http", this, "opened http request for: %s", url.string().c_str());

	request.data = data;

	return 0;
}

bool xe_http::matches(const xe_string& scheme) const{
	return scheme == "http" || scheme == "https";
}

int xe_http::redirect(xe_request& request, xe_string url_){
	xe_http_data& data = *(xe_http_data*)request.data;
	xe_url parser(url_);

	int err;

	if((err = parser.parse()))
		return err;
	if(!matches(parser.scheme())){
		xe_log_error("xe_http", this, "unsupported redirect to another protocol");

		return XE_BAD_REDIRECT;
	}

	if((err = data.open(parser, true)))
		return err;
	return start(request);
}

xe_http::~xe_http(){
	for(auto& t : connections){
		t.first.hostname.free();

		xe_dealloc(t.second);
	}
}

int xe_http_data::set_method(xe_string& string, int flags){
	if(flags & ~XE_HTTP_CLONE_STRING)
		return XE_EINVAL;
	if(!method.copy(string, flags & XE_HTTP_CLONE_STRING))
		return XE_ENOMEM;
	return 0;
}

int xe_http_data::set_header(xe_string& rkey, xe_string& rvalue, int flags){
	if(flags & ~XE_HTTP_CLONE_STRING)
		return XE_EINVAL;
	xe_option_string key, value;

	key.copy(rkey, false);

	bool clone = flags & XE_HTTP_CLONE_STRING;
	auto pair = headers.find(key);

	if(pair != headers.end()){
		if(!pair -> second.copy(rvalue, clone))
			return XE_ENOMEM;
	}else{
		if(!key.copy(rkey, clone))
			return XE_ENOMEM;
		if(!value.copy(rvalue, clone)){
			key.free();

			return XE_ENOMEM;
		}

		if(!headers.insert(key, value)){
			key.free();
			value.free();

			return XE_ENOMEM;
		}
	}

	return 0;
}

int xe_http_data::set_callback(xe_ptr function, xe_ptr value, int flags){
	if(flags)
		return XE_EINVAL;
	*(xe_ptr*)function = value;

	return 0;
}

void xe_http_data::free(){
	method.free();
	url.free();

	for(auto t = headers.begin(); t != headers.end(); t++){
		t -> first.free();
		t -> second.free();
	}

	headers.clear();
}

xe_http_data::xe_http_data(): xe_protocol_data(XE_PROTOCOL_HTTP){
	xe_zero(&callbacks);

	max_redirects = 5;
	connect_timeout = 0;
	port = 0;
	ssl_verify = 1;
	follow_location = 0;
	redirects = 0;
	ip_mode = XE_IP_ANY;
	recvbuf_size = 0;
	headers.init();
}

int xe_http_data::open(xe_url url_, bool redir){
	if(redir)
		url.free();
	else
		free();
	url = url_;
	method = "GET";

	xe_string key("Host"), host = url.host();

	if(set_header(key, host, 0))
		return XE_ENOMEM;
	return 0;
}

int xe_http_data::set(int option, xe_ptr value, int flags){
	switch(option){
		case XE_HTTP_METHOD:
			return set_method(*(xe_string*)value, flags);
		case XE_HTTP_CALLBACK_RESPONSE:
			return set_callback(&callbacks.response, value, flags);
		case XE_HTTP_CALLBACK_HEADER:
			return set_callback(&callbacks.header, value, flags);
		case XE_NET_MAX_REDIRECT:
			max_redirects = (ulong)value;

			return 0;
		case XE_NET_CONNECT_TIMEOUT:
			connect_timeout = (ulong)value;

			return 0;
		case XE_NET_PORT:
			port = (ulong)value;

			return 0;
		case XE_NET_SSL_VERIFY:
			ssl_verify = (bool)value;

			return 0;
		case XE_NET_FOLLOW_LOCATION:
			follow_location = (bool)value;

			return 0;
		case XE_NET_IP_MODE:
			ip_mode = (xe_ip_mode)(ulong)value;

			return 0;
		case XE_NET_RECVBUF_SIZE:
			recvbuf_size = (ulong)value;

			return 0;
	}

	return XE_EOPNOTSUPP;
}

int xe_http_data::set(int option, xe_ptr value1, xe_ptr value2, int flags){
	if(option == XE_HTTP_HEADER)
		return set_header(*(xe_string*)value1, *(xe_string*)value2, flags);
	return XE_EOPNOTSUPP;
}

xe_http_data::~xe_http_data(){
	free();
}

xe_protocol* xe_net::xe_http_new(xe_net_ctx& net){
	return xe_new<xe_http>(net);
}