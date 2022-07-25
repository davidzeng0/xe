#include "url.h"
#include "xutil/string.h"
#include "xutil/mem.h"
#include "xutil/encoding.h"
#include "xe/error.h"

using namespace xurl;

static inline bool letter(char c){
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline bool alpha_numeric(char c){
	return letter(c) || xe_char_is_digit(c);
}

static inline bool unreserved(char c){
	return alpha_numeric(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

static inline bool schemechr(char c){
	return alpha_numeric(c) || c == '-' || c == '.' || c == '+';
}

static inline bool subdelim(char c){
	switch(c){
		case '!':
		case '$':
		case '&':
		case '\'':
		case '(':
		case ')':
		case '*':
		case '+':
		case ',':
		case ';':
		case '=':
			return true;
		default:
			return false;
	}
}

static inline bool userchr(char c){
	return unreserved(c) || subdelim(c) || c == ':';
}

static inline bool regnamechr(char c){
	return unreserved(c) || subdelim(c);
}

xe_url::xe_url(xe_string&& url){
	string = std::move(url);
}

xe_url::xe_url(xe_url&& url){
	operator=(std::move(url));
}

xe_url& xe_url::operator=(xe_url&& url){
	string = std::move(url.string);
	n_scheme = url.n_scheme;
	n_authl = url.n_authl;
	n_user = url.n_user;
	n_host = url.n_host;
	n_port = url.n_port;
	n_path = url.n_path;
	v_port = url.v_port;

	return *this;
}

xe_string_view xe_url::href() const{
	return string;
}

int xe_url::parse(){
	uint i = 0;
	uint scheme = 0, authl = 0, user = 0, host = 0, path = 0;

	if(0 < string.length()){
		switch(string[0]){
			case '[':
				goto host;
			case ']':
				return XE_MALFORMED_URL;
			default:
				if(!letter(string[0]))
					goto auth;
		}
	}

	while(i < string.length()){
		switch(string[i]){
			case ':': {
				uint left = i++;

				if(i < string.length())
					switch(string[i]){
						case '[':
							scheme = left;
							authl = i;
							user = i;

							goto host;
						case ']':
							return XE_MALFORMED_URL;
						case '/':
							scheme = left;

							if(++i < string.length() && string[i] == '/')
								i++;
							authl = i;
							user = i;

							goto auth;
						case '?':
						case '#':
							scheme = left;
							authl = i;
							user = i;
							host = i;

							if(string[i] == '#'){
								path = i;

								goto finish;
							}

							goto path;
						case '@':
							authl = i;
							user = ++i;

							if(i < string.length() && string[i] == '[')
								goto host;

							goto auth;
						default:
							if(!xe_char_is_digit(string[i])){
								scheme = left;
								authl = left + 1;
								user = authl;

								goto auth;
							}

							i++;
					}
				while(i < string.length())
					switch(string[i]){
						case '[':
							return XE_MALFORMED_URL;
						case ']':
							return XE_MALFORMED_URL;
						case '/':
						case '?':
						case '#':
							host = i;

							if(string[i] == '#'){
								path = i;

								goto finish;
							}

							goto path;
						case '@':
							scheme = left;
							authl = left + 1;
							user = ++i;

							if(i < string.length() && string[i] == '[')
								goto host;
							goto auth;
						default:
							if(!xe_char_is_digit(string[i])){
								scheme = left;
								authl = left + 1;
								user = authl;

								goto auth;
							}

							i++;
					}
				host = i;
				path = i;

				goto finish;

				break;
			}

			case '@':
				user = ++i;

				if(i < string.length() && string[i] == '[')
					goto host;
				goto auth;
			case '/':
			case '?':
			case '#':
				host = i;

				if(string[i] == '#'){
					path = i;

					goto finish;
				}

				goto path;
			case '[':
				return XE_MALFORMED_URL;
			case ']':
				return XE_MALFORMED_URL;
			default:
				i++;
		}
	}

	host = i;
	path = i;

	goto finish;
auth:
	if(i < string.length()){
		switch(string[i]){
			case '@':
				user = ++i;

				if(i < string.length() && string[i] == '[')
					goto host;
			case '/':
			case '?':
			case '#':
				host = i;

				if(string[i] == '#'){
					path = i;

					goto finish;
				}

				goto path;
			case '[':
				goto host;
			case ']':
				return XE_MALFORMED_URL;
			default:
				i++;
		}
	}

	while(i < string.length()){
		switch(string[i]){
			case '@':
				user = ++i;

				if(i < string.length() && string[i] == '[')
					goto host;
				break;
			case '/':
			case '?':
			case '#':
				host = i;

				if(string[i] == '#'){
					path = i;

					goto finish;
				}

				goto path;
			case '[':
			case ']':
				return XE_MALFORMED_URL;
			default:
				i++;
		}
	}
host:
	while(i < string.length()){
		switch(string[i]){
			case '/':
			case '?':
			case '#':
				host = i;

				if(string[i] == '#'){
					path = i;

					goto finish;
				}

				goto path;
			default:
				i++;
		}
	}

	host = i;
	path = i;

	goto finish;
path:
	while(i < string.length()){
		switch(string[i]){
			case '#':
				path = i;

				goto finish;
			default:
				i++;
		}
	}

	path = i;
finish:
	for(uint i = 0; i < scheme; i++)
		if(!schemechr(string[i]))
			return XE_MALFORMED_URL;
	if(user > 0)
		for(uint i = authl; i < user - 1; i++)
			if(!userchr(string[i]))
				return XE_MALFORMED_URL;
	i = user;

	if(i < string.length() && string[i] == '['){
		uint k = host;

		while(i < host){
			if(string[i] == ']'){
				if(k < host)
					return XE_MALFORMED_URL;
				k = i;
			}

			i++;
		}

		if(k == host)
			return XE_MALFORMED_URL;
		i = k + 1;
	}else{
		uint k = host;

		while(i < host)
			if(string[i] == ':'){
				if(k < host)
					return XE_MALFORMED_URL;
				k = i++;
			}else if(!regnamechr(string[i]))
				return XE_MALFORMED_URL;
			else
				i++;
		i = k;
	}

	if(i < string.length() && string[i] == ':'){
		ushort port = 0;

		n_host = i;

		if(xe_read_integer(XE_DECIMAL, port, string.data() + i + 1, host - i - 1) != host - i - 1)
			return XE_MALFORMED_URL;
		v_port = port;
	}else{
		n_host = host;
		v_port = 0;
	}

	n_scheme = scheme;
	n_authl = authl;
	n_user = user;
	n_port = host;
	n_path = path;

	return 0;
}

xe_string_view xe_url::scheme() const{
	return string.substring(0, n_scheme);
}

xe_string_view xe_url::user_info() const{
	return string.substring(n_authl, n_user);
}

xe_string_view xe_url::host() const{
	return string.substring(n_user, n_port);
}

xe_string_view xe_url::hostname() const{
	if(n_host > 0 && n_user < string.length() && string[n_user] == '[' && string[n_host - 1] == ']')
		return string.substring(n_user + 1, n_host - 1);
	return string.substring(n_user, n_host);
}

xe_string_view xe_url::path() const{
	return string.substring(n_port, n_path);
}

uint xe_url::port() const{
	return v_port;
}

void xe_url::free(){
	string.free();
}

xe_url::~xe_url(){
	free();
}