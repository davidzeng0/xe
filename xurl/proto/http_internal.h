#pragma once
#include "xutil/encoding.h"
#include "xstd/string.h"
#include "xe/error.h"
#include "http_base.h"
#include "../url.h"
#include "../conn.h"
#include "../request.h"
#include "../request_internal.h"

using namespace xurl;

class xe_http_string : public xe_string{
protected:
	bool owner;
public:
	using xe_string::operator==;

	xe_http_string();

	xe_http_string(xe_http_string&& other);
	xe_http_string& operator=(xe_http_string&& other);

	bool copy(const xe_string_view& src);
	void own(const xe_string_view& src);
	void clear();

	xe_http_string& operator=(const xe_string_view& src);
	bool operator==(const xe_http_string& other) const;

	~xe_http_string();
};

class xe_http_connection;
class xe_http_internal_data{
public:
	typedef xe_map<xe_http_string, xe_http_string, xe_http_lowercase_hash, xe_http_case_insensitive> xe_http_headers;

	xe_url url;
	xe_http_string method;
	xe_http_headers headers;
	xe_http_connection* connection;
	xe_http_version min_version;
	xe_http_version max_version;
	uint redirects;

	xe_http_internal_data();

	bool internal_set_method(const xe_string_view& method, uint flags);
	bool internal_set_header(const xe_string_view& key, const xe_string_view& value, uint flags);
	bool internal_has_header(const xe_string_view& key);
	xe_string_view internal_get_header(const xe_string_view& key);
	bool internal_erase_header(const xe_string_view& key);
	void clear();

	~xe_http_internal_data();
};

class xe_http_common_specific : public xe_http_common_data, public xe_http_internal_data{};

class xe_http_protocol : public xe_protocol{
public:
	xe_http_protocol(xurl_ctx& ctx, xe_protocol_id id): xe_protocol(ctx, id){}

	virtual void redirect(xe_request_internal& request, xe_string&& url);
	virtual bool available(xe_http_connection& connection, bool available);
	virtual int open(xe_request_internal& req, xe_url&& url) = 0;
	virtual int open(xe_http_internal_data& data, xe_url&& url, bool redirect);

	~xe_http_protocol() = default;
};

class xe_http_connection : public xe_connection{
protected:
	xe_http_protocol& proto;
public:
	xe_http_connection(xe_http_protocol& proto): proto(proto){}

	virtual int open(xe_request_internal& request) = 0;
	virtual int transferctl(xe_request_internal& request, uint flags) = 0;
	virtual void end(xe_request_internal& request) = 0;

	virtual xe_cstr class_name() = 0;
};

class xe_http_singleconnection : public xe_http_connection{
protected:
	xe_request_internal* request;
	xe_http_common_specific* specific;

	uint read_state;
	uint chunked_state;
	uint transfer_mode;

	ulong data_len;
	xe_string location;

	xe_vector<byte> client_headers;
	size_t send_offset;

	byte* header_buffer;
	uint header_total;
	uint header_offset;

	bool request_active: 1;
	bool transfer_active: 1;

	bool bodyless: 1;
	bool follow: 1;
	bool connection_close: 1;
	bool statusline_prefix_checked: 1;

	bool client_write(xe_ptr buf, size_t len);

	int start();
	int transferctl(uint flags);
	int send_headers();
	void complete(int error);

	int init_socket();
	void set_request_state(xe_connection_state state);
	void set_state(xe_connection_state state);
	bool readable();
	int writable();
	int ready();

	ssize_t data(xe_ptr data, size_t size);

	int read_line(byte*& buf, size_t& len, xe_string_view& line);
	virtual int handle_status_line(xe_http_version version, uint status, const xe_string_view& reason);
	int parse_headers(byte* buf, size_t len);
	virtual int handle_header(const xe_string_view& key, const xe_string_view& value);
	int parse_trailers(byte* buf, size_t len);
	bool chunked_save(byte* buf, size_t len);
	int chunked_body(byte* buf, size_t len);
	virtual int handle_trailer(const xe_string_view& key, const xe_string_view& value);

	virtual int pretransfer();
	virtual int posttransfer();
	virtual int write_body(byte* buf, size_t len);
public:
	xe_http_singleconnection(xe_http_protocol& proto): xe_http_connection(proto){}

	int open(xe_request_internal& req);
	int transferctl(xe_request_internal& request, uint flags);
	void end(xe_request_internal& request);

	void close(int error);

	~xe_http_singleconnection() = default;

	virtual xe_cstr class_name();
};