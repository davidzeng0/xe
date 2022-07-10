#pragma once
#include "xe/string.h"
#include "http_base.h"
#include "../url.h"
#include "../conn.h"
#include "../request.h"
#include "../encoding.h"

using namespace xurl;

class xe_http_string : public xe_string{
protected:
	bool owner;
public:
	bool copy(const xe_string& src);
	void own(const xe_string& src);
	void free();

	xe_http_string& operator=(const xe_string& src);

	~xe_http_string();
};

class xe_http_connection;
class xe_http_internal_data{
	struct xe_http_case_insensitive{
		bool operator()(const xe_string& a, const xe_string& b) const{
			return a.equal_case(b);
		}
	};

	struct xe_http_lowercase_hash{
		size_t operator()(const xe_string& str) const{
			return str.hash();
		}
	};
public:
	typedef xe_map<xe_http_string, xe_http_string, xe_http_lowercase_hash, xe_http_case_insensitive> xe_http_headers;

	xe_url url;
	uint redirects;
	xe_http_version min_version;
	xe_http_version max_version;
	xe_http_string method;
	xe_http_headers headers;
	xe_http_connection* connection;

	xe_http_internal_data();

	bool internal_set_method(xe_string method, uint flags);
	bool internal_set_header(xe_string key, xe_string value, uint flags);
	void free();

	~xe_http_internal_data();
};

class xe_http_common_specific : public xe_http_common_data, public xe_http_internal_data{};

class xe_http_protocol : public xe_protocol{
public:
	xe_http_protocol(xurl_ctx& ctx, xe_protocol_id id): xe_protocol(ctx, id){}

	virtual void redirect(xe_request& request, xe_string&& url);
	virtual bool available(xe_http_connection& connection, bool available);
	virtual void closed(xe_http_connection& connection);
	virtual int open(xe_http_internal_data& data, xe_url url, bool redirect);

	~xe_http_protocol(){}
};

class xe_http_connection : public xe_connection{
protected:
	xe_http_protocol& proto;
public:
	xe_http_connection(xe_http_protocol& proto): proto(proto){}

	virtual int open(xe_request& request) = 0;
	virtual int transferctl(xe_request& request, uint flags) = 0;
	virtual void end(xe_request& request) = 0;

	virtual void close(int error);

	virtual ~xe_http_connection(){}

	virtual xe_cstr class_name() = 0;
};

class xe_http_singleconnection : public xe_http_connection{
protected:
	xe_request* request;
	xe_http_common_specific* specific;

	uint read_state;
	uint chunked_state;
	uint transfer_mode;

	ulong data_len;
	xe_string location;

	xe_vector<char> client_headers;
	size_t send_offset;

	xe_bptr header_buffer;
	uint header_total;
	uint header_offset;

	bool request_active: 1;
	bool transfer_active: 1;

	bool bodyless: 1;
	bool follow: 1;
	bool connection_close: 1;

	bool client_write(xe_ptr buf, size_t len);

	int start();
	int transferctl(uint flags);
	int send_headers();
	void complete(int error);

	void set_state(xe_connection_state state);
	bool readable();
	int writable();
	int ready();
	void close(int error);

	ssize_t data(xe_ptr data, size_t size);

	int read_line(xe_bptr& buf, size_t& len, xe_string& line);
	bool parse_status_line(xe_string& line, xe_http_version& version, uint& status, xe_string& reason);
	int parse_headers(xe_bptr buf, size_t len);
	int handle_header(xe_string& key, xe_string& value);
	int parse_trailers(xe_bptr buf, size_t len);
	int chunked_body(xe_bptr buf, size_t len);

	virtual int write_body(xe_bptr buf, size_t len);

	virtual int handle_statusline(xe_http_version version, uint status, xe_string& reason);
	virtual int handle_singleheader(xe_string& key, xe_string& value);
	virtual int handle_trailer(xe_string& key, xe_string& value);
public:
	xe_http_singleconnection(xe_http_protocol& proto): xe_http_connection(proto){}

	int open(xe_request& req);
	int transferctl(xe_request& request, uint flags);
	void end(xe_request& request);

	~xe_http_singleconnection();

	xe_cstr class_name();
};