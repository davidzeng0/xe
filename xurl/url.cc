#include "xutil/encoding.h"
#include "xe/error.h"
#include "url.h"

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
	offsets.scheme = url.offsets.scheme;
	offsets.authl = url.offsets.authl;
	offsets.user = url.offsets.user;
	offsets.host = url.offsets.host;
	offsets.port = url.offsets.port;
	offsets.path = url.offsets.path;
	port_ = url.port_;

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

		offsets.host = i;

		if(xe_read_integer(XE_DECIMAL, port, string.data() + i + 1, host - i - 1) != host - i - 1)
			return XE_MALFORMED_URL;
		port_ = port;
	}else{
		offsets.host = host;
		port_ = 0;
	}

	offsets.scheme = scheme;
	offsets.authl = authl;
	offsets.user = user;
	offsets.port = host;
	offsets.path = path;

	return 0;
}

xe_string_view xe_url::scheme() const{
	return string.substring(0, offsets.scheme);
}

xe_string_view xe_url::user_info() const{
	return string.substring(offsets.authl, offsets.user);
}

xe_string_view xe_url::host() const{
	return string.substring(offsets.user, offsets.port);
}

xe_string_view xe_url::hostname() const{
	if(offsets.host > 0 && offsets.user < string.length() && string[offsets.user] == '[' && string[offsets.host - 1] == ']')
		return string.substring(offsets.user + 1, offsets.host - 1);
	return string.substring(offsets.user, offsets.host);
}

xe_string_view xe_url::path() const{
	return string.substring(offsets.port, offsets.path);
}

ushort xe_url::port() const{
	return port_;
}

void xe_url::clear(){
	string.clear();
}

xe_url::~xe_url(){
	clear();
}