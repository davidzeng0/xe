#include "http.h"
#include "http_internal.h"
#include "net_internal.h"
#include "xstd/unique_ptr.h"
#include "xstd/linked_list.h"
#include "xutil/log.h"

using namespace xurl;

struct xe_host{
	xe_http_string hostname;

	xe_host() = default;

	xe_host(xe_host&& other){
		operator=(std::move(other));
	}

	xe_host& operator=(xe_host&& other){
		hostname = std::move(other.hostname);
		port = other.port;
		secure = other.secure;

		return *this;
	}

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

	bool operator==(const xe_host& other) const{
		return num == other.num && hostname == other.hostname;
	}
};

template<>
struct xe_hash<xe_host>{
	size_t operator()(const xe_host& host) const{
		return host.hash();
	}
};

struct xe_http_callbacks{
	xe_http_statusline_cb statusline;
	xe_http_singleheader_cb singleheader;
	xe_http_response_cb response;
	xe_http_singleheader_cb trailer;
	xe_http_external_redirect_cb external_redirect;
};

class xe_http_specific_internal : public xe_http_specific, public xe_http_internal_data{
protected:
	friend class xe_http;
public:
	xe_http_callbacks callbacks;
};

class xe_http;
class xe_http_protocol_singleconnection : public xe_http_singleconnection{
protected:
	xe_http_response response;

	xe_http_callbacks& callbacks(){
		return ((xe_http_specific_internal*)specific) -> callbacks;
	}

	template<typename F, typename... Args>
	int call(F xe_http_callbacks::*field, Args&& ...args){
		auto callback = callbacks().*field;

		return callback && callback(std::forward<Args>(args)...) ? XE_ECANCELED : 0;
	}

	int init_socket(){
		xe_return_error(xe_http_singleconnection::init_socket());
		xe_return_error(set_nodelay(true));
		xe_return_error(set_keepalive(true, 60));

		return 0;
	}

	int handle_status_line(xe_http_version version, uint status, const xe_string_view& reason){
		if(callbacks().response){
			response.version = version;
			response.status = status;

			if(!response.status_text.copy(reason)) return XE_ENOMEM;
		}

		return call(&xe_http_callbacks::statusline, *request, version, status, reason);
	}

	int handle_header(const xe_string_view& key, const xe_string_view& value){
		xe_return_error(xe_http_singleconnection::handle_header(key, value));

		if(callbacks().response){
			xe_string skey, svalue;
			auto it = response.headers.find(key);

			if(it != response.headers.end()){
				if(!svalue.copy(value) ||
					!it -> second.push_back(std::move(svalue)))
					return XE_ENOMEM;
			}else{
				xe_vector<xe_string> list;

				if(!skey.copy(key) || !svalue.copy(value) || !list.push_back(std::move(svalue)) ||
					!response.headers.insert(std::move(skey), std::move(list)))
					return XE_ENOMEM;
			}
		}

		return call(&xe_http_callbacks::singleheader, *request, key, value);
	}

	int pretransfer(){
		int err;

		xe_return_error(xe_http_singleconnection::pretransfer());

		if(follow)
			return 0;
		err = call(&xe_http_callbacks::response, *request, response);
		response.clear();

		return err;
	}

	int handle_trailer(const xe_string_view& key, const xe_string_view& value){
		return call(&xe_http_callbacks::trailer, *request, key, value);
	}

	void closed();
public:
	xe_http_protocol_singleconnection(xe_http& proto);

	xe_cstr class_name();
};

class xe_http_connection_list;

template<class xe_connection_type = xe_http_protocol_singleconnection>
class xe_http_connection_node : public xe_linked_node{
public:
	xe_http_connection_list& list;
	xe_connection_type connection;

	xe_http_connection_node(xe_http& proto, xe_http_connection_list& list):
		list(list), connection(proto){}

	~xe_http_connection_node() = default;
};

class xe_http_connection_list{
public:
	xe_linked_list<xe_http_connection_node<>> list;

	operator bool(){
		return !list.empty();
	}

	xe_http_connection_node<>& front(){
		return list.front();
	}

	void add(xe_http_connection_node<>& conn){
		if(!conn.linked())
			list.append(conn);
	}

	void remove(xe_http_connection_node<>& conn){
		if(conn.linked())
			list.erase(conn);
	}
};

class xe_http : public xe_http_protocol{
public:
	xe_map<xe_host, xe_unique_ptr<xe_http_connection_list>> connections;

	xe_http(xurl_ctx& net);

	int start(xe_request_internal& request);

	int transferctl(xe_request_internal& request, uint flags);
	void end(xe_request_internal& request);

	int open(xe_request_internal& request, xe_url&& url);

	bool matches(const xe_string_view& scheme) const;

	void redirect(xe_request_internal& request, xe_string&& url);
	int internal_redirect(xe_request_internal& request, xe_string&& url);
	bool available(xe_http_connection& connection, bool available);

	~xe_http() = default;

	static xe_cstr class_name();
};

xe_http_protocol_singleconnection::xe_http_protocol_singleconnection(xe_http& proto): xe_http_singleconnection(proto){}

void xe_http_protocol_singleconnection::closed(){
	xe_http_singleconnection::closed();
	xe_http_connection_node<>& node = xe_containerof(*this, &xe_http_connection_node<>::connection);

	xe_delete(&node);
}

xe_cstr xe_http_protocol_singleconnection::class_name(){
	return "xe_http_connection";
}

xe_http::xe_http(xurl_ctx& net): xe_http_protocol(net, XE_PROTOCOL_HTTP){}

int xe_http::start(xe_request_internal& request){
	xe_http_specific_internal& data = *(xe_http_specific_internal*)request.data;

	uint port = data.port;
	int err;

	bool secure = data.url.scheme().length() == 5;

	if(!port)
		port = data.url.port();
	if(!port){
		port = secure ? 443 : 80;

		xe_log_verbose(this, "using default port %u", port);
	}

	xe_host host;

	host.hostname = data.url.hostname();
	host.port = port;
	host.secure = secure;

	auto conn = connections.find(host);

	xe_http_connection_list* list;

	if(conn != connections.end()){
		list = conn -> second;

		while(*list){
			xe_http_protocol_singleconnection& conn = list -> front().connection;

			err = 0;

			if(!conn.peer_closed()){
				err = conn.open(request);

				if(!err) return 0;
			}

			conn.close(err);

			if(err && err != XE_SEND_ERROR) return err;
		}
	}else{
		xe_unique_ptr<xe_http_connection_list> new_list;

		list = xe_new<xe_http_connection_list>();
		new_list = list;

		if(!list || !host.hostname.copy(data.url.hostname()) ||
			!connections.insert(std::move(host), std::move(new_list)))
			return XE_ENOMEM;
	}

	xe_http_connection_node<>* node = xe_znew<xe_http_connection_node<>>(*this, *list);

	if(!node)
		return XE_ENOMEM;
	err = xe_start_connection(*ctx, node -> connection, data, secure, data.url.hostname(), port);

	if(err)
		node -> connection.close(err);
	else
		node -> connection.open(request);
	return err;
}

int xe_http::transferctl(xe_request_internal& request, uint flags){
	auto& data = *(xe_http_common_specific*)request.data;

	return data.connection -> transferctl(request, flags);
}

void xe_http::end(xe_request_internal& request){
	auto& data = *(xe_http_common_specific*)request.data;

	data.connection -> end(request);
}

int xe_http::open(xe_request_internal& request, xe_url&& url){
	xe_http_specific_internal* data;
	int err;

	if(request.data && request.data -> id() == XE_PROTOCOL_HTTP)
		data = (xe_http_specific_internal*)request.data;
	else{
		data = xe_znew<xe_http_specific_internal>();

		if(!data)
			return XE_ENOMEM;
		data -> ssl_ctx = &ctx -> ssl_ctx();
	}

	if((err = xe_http_protocol::open(*data, std::move(url), false))){
		if(data != (xe_http_specific_internal*)request.data)
			xe_delete(data);
		return err;
	}

	request.data = data;

	xe_log_verbose(this, "opened http request for: %s", data -> url.href().data());

	return 0;
}

bool xe_http::matches(const xe_string_view& scheme) const{
	return scheme == "http" || scheme == "https";
}

void xe_http::redirect(xe_request_internal& request, xe_string&& url){
	xe_log_verbose(this, "redirect to %.*s", url.length(), url.data());

	int err = internal_redirect(request, std::move(url));

	if(err) request.complete(err);
}

int xe_http::internal_redirect(xe_request_internal& request, xe_string&& url){
	xe_http_specific_internal& data = *(xe_http_specific_internal*)request.data;
	xe_url parser(std::move(url));

	if(data.redirects++ >= data.max_redirects)
		return XE_TOO_MANY_REDIRECTS;
	xe_return_error(parser.parse());

	if(!matches(parser.scheme())){
		data.url = std::move(parser);

		return XE_EXTERNAL_REDIRECT;
	}

	xe_return_error(xe_http_protocol::open(data, std::move(parser), true));

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

xe_cstr xe_http::class_name(){
	return "xe_http";
}

xe_protocol* xurl::xe_http_new(xurl_ctx& ctx){
	return xe_new<xe_http>(ctx);
}

xe_http_specific::xe_http_specific(): xe_http_common_data(XE_PROTOCOL_HTTP){}

bool xe_http_specific::set_method(const xe_string_view& method, uint flags){
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