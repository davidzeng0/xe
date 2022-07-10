#include "../request.h"
#include "../conn.h"
#include "http_internal.h"
#include "http.h"

using namespace xurl;
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
		return xe_hash_combine(xe_hash_int(num), hostname.hash());
	}

	bool operator==(const xe_http_host& other) const{
		return num == other.num && hostname == other.hostname;
	}
};

class xe_http_specific_internal : public xe_http_specific, public xe_http_internal_data{
protected:
	friend class xe_http;
public:
	struct xe_callbacks{
		xe_http_statusline_cb statusline;
		xe_http_singleheader_cb singleheader;
		xe_http_response_cb response;
		xe_http_singleheader_cb trailer;
		xe_http_external_redirect_cb external_redirect;
	} callbacks;
};

class xe_http;
class xe_http_protocol_singleconnection : public xe_http_singleconnection{
	xe_http_specific_internal* options(){
		return (xe_http_specific_internal*)specific;
	}
protected:
	int handle_statusline(xe_http_version version, uint status, xe_string& reason){
		return options() -> callbacks.statusline && options() -> callbacks.statusline(*request, version, status, reason) ? XE_ABORTED : 0;
	}

	int handle_singleheader(xe_string& key, xe_string& value){
		return options() -> callbacks.singleheader && options() -> callbacks.singleheader(*request, key, value) ? XE_ABORTED : 0;
	}

	int handle_trailer(xe_string& key, xe_string& value){
		return options() -> callbacks.trailer && options() -> callbacks.trailer(*request, key, value) ? XE_ABORTED : 0;
	}
public:
	xe_http_protocol_singleconnection(xe_http& proto);
};

class xe_http_connection_list;

template<class xe_connection_type = xe_http_protocol_singleconnection>
class xe_http_connection_node{
public:
	xe_http_connection_list& list;
	xe_http_connection_node* next;
	xe_http_connection_node* prev;

	xe_connection_type connection;

	xe_http_connection_node(xe_http& proto, xe_http_connection_list& list): connection(proto), list(list){}
};

class xe_http_connection_list{
public:
	xe_http_connection_node<>* head;

	operator bool(){
		return head != null;
	}

	void add(xe_http_connection_node<>& conn){
		if(head == &conn || conn.next != null || conn.prev != null)
			return;
		if(head)
			head -> prev = &conn;
		conn.next = head;
		head = &conn;
	}

	void remove(xe_http_connection_node<>& conn){
		if(head != &conn && conn.next == null && conn.prev == null)
			return;
		xe_assert((conn.prev == null) == (&conn == head));

		if(conn.next)
			conn.next -> prev = conn.prev;
		if(conn.prev)
			conn.prev -> next = conn.next;
		else
			head = conn.next;
		conn.next = null;
		conn.prev = null;
	}
};

class xe_http : public xe_http_protocol{
public:
	xe_map<xe_http_host, xe_http_connection_list*> connections;

	xe_http(xurl_ctx& net);

	int start(xe_request& request);

	int transferctl(xe_request& request, uint flags);
	void end(xe_request& request);

	int open(xe_request& request, xe_url url);

	bool matches(const xe_string& scheme) const;

	void redirect(xe_request& request, xe_string&& url);
	int internal_redirect(xe_request& request, xe_string&& url);
	bool available(xe_http_connection& connection, bool available);
	void closed(xe_http_connection& connection);

	~xe_http();

	static xe_cstr class_name();
};

xe_http_protocol_singleconnection::xe_http_protocol_singleconnection(xe_http& proto): xe_http_singleconnection(proto){}

xe_http::xe_http(xurl_ctx& net): xe_http_protocol(net, XE_PROTOCOL_HTTP){
	connections.init();
}

int xe_http::start(xe_request& request){
	xe_http_specific_internal& data = *(xe_http_specific_internal*)request.data;

	uint port = data.port;
	int err;

	bool secure = data.url.scheme().length() == 5;

	if(!port)
		port = data.url.port();
	if(!port){
		port = secure ? 443 : 80;

		xe_log_debug(this, "using default port %u", port);
	}

	xe_http_host host;

	host.hostname = data.url.hostname();
	host.port = port;
	host.secure = secure;

	auto conn = connections.find(host);

	xe_http_connection_list* list;

	if(conn != connections.end()){
		list = conn -> second;

		if(*list){
			err = list -> head -> connection.open(request);

			if(!err)
				return 0;
		}
	}else{
		list = xe_zalloc<xe_http_connection_list>();

		if(!list)
			return XE_ENOMEM;
		if(!host.hostname.copy(data.url.hostname())){
			xe_dealloc(list);

			return XE_ENOMEM;
		}

		if(!connections.insert(host, list)){
			xe_dealloc(list);

			host.hostname.free();

			return XE_ENOMEM;
		}
	}

	xe_http_connection_node<>* node = xe_znew<xe_http_connection_node<>>(*this, *list);

	if(!node)
		return XE_ENOMEM;
	node -> connection.set_ssl_verify(data.ssl_verify);
	node -> connection.set_connect_timeout(data.connect_timeout);
	node -> connection.set_ip_mode(data.ip_mode);
	node -> connection.set_recvbuf_size(data.recvbuf_size);
	node -> connection.set_tcp_keepalive(true);
	node -> connection.open(request);

	do{
		if((err = node -> connection.init(*ctx)))
			break;
		if(secure && (err = node -> connection.init_ssl(ctx -> ssl())))
			break;
		if((err = node -> connection.connect(data.url.hostname(), port)))
			break;
		return 0;
	}while(false);

	xe_delete(node);

	return err;
}

int xe_http::transferctl(xe_request& request, uint flags){
	auto& data = *(xe_http_common_specific*)request.data;

	return data.connection -> transferctl(request, flags);
}

void xe_http::end(xe_request& request){
	auto& data = *(xe_http_common_specific*)request.data;

	data.connection -> end(request);
}

int xe_http::open(xe_request& request, xe_url url){
	xe_http_specific_internal* data;

	int err;

	if(request.data && request.data -> id() == XE_PROTOCOL_HTTP)
		data = (xe_http_specific_internal*)request.data;
	else{
		data = xe_znew<xe_http_specific_internal>();

		if(!data) return XE_ENOMEM;
	}

	if((err = xe_http_protocol::open(*(xe_http_internal_data*)data, url, false))){
		if(data != (xe_http_specific_internal*)request.data)
			xe_delete(data);
		return err;
	}

	xe_log_verbose(this, "opened http request for: %s", url.string().c_str());

	request.data = data;

	return 0;
}

bool xe_http::matches(const xe_string& scheme) const{
	return scheme == "http" || scheme == "https";
}

void xe_http::redirect(xe_request& request, xe_string&& url){
	int err = internal_redirect(request, std::forward<xe_string>(url));

	if(err) request.complete(err);
}

int xe_http::internal_redirect(xe_request& request, xe_string&& url){
	xe_http_specific_internal& data = *(xe_http_specific_internal*)request.data;
	xe_url parser(url);

	if(data.redirects++ >= data.max_redirects)
		return XE_TOO_MANY_REDIRECTS;
	xe_return_error(parser.parse());

	if(!matches(parser.scheme()))
		return XE_EXTERNAL_REDIRECT;
	xe_return_error(xe_http_protocol::open((xe_http_internal_data&)data, parser, true));

	return start(request);
}

bool xe_http::available(xe_http_connection& connection, bool available){
	xe_http_connection_node<>& node = xe_containerof((xe_http_protocol_singleconnection&)connection, &xe_http_connection_node<>::connection);

	if(available)
		node.list.add(node);
	else
		node.list.remove(node);
	return false;
}

void xe_http::closed(xe_http_connection& connection){
	xe_http_connection_node<>& node = xe_containerof((xe_http_protocol_singleconnection&)connection, &xe_http_connection_node<>::connection);

	xe_delete(&node);
}

xe_http::~xe_http(){
	for(auto& t : connections){
		t.first.hostname.free();

		xe_dealloc(t.second);
	}
}

xe_cstr xe_http::class_name(){
	return "xe_http";
}

xe_protocol* xurl::xe_http_new(xurl_ctx& ctx){
	return xe_new<xe_http>(ctx);
}

static int combine_version(uint major, uint minor){
	if(major > 9 || minor > 9)
		return -1;
	return major * 10 + minor;
}

xe_http_specific::xe_http_specific(): xe_http_common_data(XE_PROTOCOL_HTTP){}

xe_http_specific::~xe_http_specific(){}

bool xe_http_specific::set_method(xe_string method, uint flags){
	xe_http_specific_internal& internal = *(xe_http_specific_internal*)this;

	return internal.internal_set_method(method, flags);
}

void xe_http_specific::set_min_version(xe_http_version version){
	xe_http_specific_internal& internal = *(xe_http_specific_internal*)this;

	internal.min_version = version;
}

void xe_http_specific::set_max_version(xe_http_version version){
	xe_http_specific_internal& internal = *(xe_http_specific_internal*)this;

	internal.max_version = version;
}

void xe_http_specific::set_body(xe_ptr data, size_t len){

}

void xe_http_specific::set_input(xe_http_input& input){

}

void xe_http_specific::set_statusline_cb(xe_http_statusline_cb cb){
	xe_http_specific_internal& internal = *(xe_http_specific_internal*)this;

	internal.callbacks.statusline = cb;
}

void xe_http_specific::set_singleheader_cb(xe_http_singleheader_cb cb){
	xe_http_specific_internal& internal = *(xe_http_specific_internal*)this;

	internal.callbacks.singleheader = cb;
}

void xe_http_specific::set_response_cb(xe_http_response_cb cb){
	xe_http_specific_internal& internal = *(xe_http_specific_internal*)this;

	internal.callbacks.response = cb;
}

void xe_http_specific::set_trailer_cb(xe_http_singleheader_cb cb){
	xe_http_specific_internal& internal = *(xe_http_specific_internal*)this;

	internal.callbacks.trailer = cb;
}