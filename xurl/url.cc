#include "url.h"
#include "xe/string.h"
#include "xe/mem.h"
#include "encoding.h"

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

xe_url::xe_url(){}

xe_url::xe_url(xe_string string){
	string_ = string;
}

xe_string xe_url::string(){
	return string_;
}

int xe_url::parse(){
	uint i = 0;
	uint scheme = 0, authl = 0, user = 0, host = 0, path = 0;

	if(0 < string_.length()){
		switch(string_[0]){
			case '[':
				goto host;
			case ']':
				return XE_MALFORMED_URL;
			default:
				if(!letter(string_[0]))
					goto auth;
		}
	}

	while(i < string_.length()){
		switch(string_[i]){
			case ':': {
				uint left = i++;

				if(i < string_.length())
					switch(string_[i]){
						case '[':
							scheme = left;
							authl = i;
							user = i;

							goto host;
						case ']':
							return XE_MALFORMED_URL;
						case '/':
							scheme = left;

							if(++i < string_.length() && string_[i] == '/')
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

							if(string_[i] == '#'){
								path = i;

								goto finish;
							}

							goto path;
						case '@':
							authl = i;
							user = ++i;

							if(i < string_.length() && string_[i] == '[')
								goto host;

							goto auth;
						default:
							if(!xe_char_is_digit(string_[i])){
								scheme = left;
								authl = left + 1;
								user = authl;

								goto auth;
							}

							i++;
					}
				while(i < string_.length())
					switch(string_[i]){
						case '[':
							return XE_MALFORMED_URL;
						case ']':
							return XE_MALFORMED_URL;
						case '/':
						case '?':
						case '#':
							host = i;

							if(string_[i] == '#'){
								path = i;

								goto finish;
							}

							goto path;
						case '@':
							scheme = left;
							authl = left + 1;
							user = ++i;

							if(i < string_.length() && string_[i] == '[')
								goto host;
							goto auth;
						default:
							if(!xe_char_is_digit(string_[i])){
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

				if(i < string_.length() && string_[i] == '[')
					goto host;
				goto auth;
			case '/':
			case '?':
			case '#':
				host = i;

				if(string_[i] == '#'){
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
	if(i < string_.length()){
		switch(string_[i]){
			case '@':
				user = ++i;

				if(i < string_.length() && string_[i] == '[')
					goto host;
			case '/':
			case '?':
			case '#':
				host = i;

				if(string_[i] == '#'){
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

	while(i < string_.length()){
		switch(string_[i]){
			case '@':
				user = ++i;

				if(i < string_.length() && string_[i] == '[')
					goto host;
				break;
			case '/':
			case '?':
			case '#':
				host = i;

				if(string_[i] == '#'){
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
	while(i < string_.length()){
		switch(string_[i]){
			case '/':
			case '?':
			case '#':
				host = i;

				if(string_[i] == '#'){
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
	while(i < string_.length()){
		switch(string_[i]){
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
		if(!schemechr(string_[i]))
			return XE_MALFORMED_URL;
	if(user > 0)
		for(uint i = authl; i < user - 1; i++)
			if(!userchr(string_[i]))
				return XE_MALFORMED_URL;
	i = user;

	if(i < string_.length() && string_[i] == '['){
		uint k = host;

		while(i < host){
			if(string_[i] == ']'){
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
			if(string_[i] == ':'){
				if(k < host)
					return XE_MALFORMED_URL;
				k = i++;
			}else if(!regnamechr(string_[i]))
				return XE_MALFORMED_URL;
			else
				i++;
		i = k;
	}

	if(i < string_.length() && string_[i] == ':'){
		n_host = i;

		uint port = 0;

		while(++i < host)
			if(!xe_char_is_digit(string_[i]))
				return XE_MALFORMED_URL;
			else{
				port = port * 10 + xe_digit_to_int(string_[i]);

				if(port > 0xffff)
					return XE_MALFORMED_URL;
			}
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

xe_string xe_url::scheme(){
	return string_.substring(0, n_scheme);
}

xe_string xe_url::user_info(){
	return string_.substring(n_authl, n_user);
}

xe_string xe_url::host(){
	return string_.substring(n_user, n_port);
}

xe_string xe_url::hostname(){
	if(n_host > 0 && n_user < string_.length() && string_[n_user] == '[' && string_[n_host - 1] == ']')
		return string_.substring(n_user + 1, n_host - 1);
	return string_.substring(n_user, n_host);
}

xe_string xe_url::path(){
	return string_.substring(n_port, n_path);
}

uint xe_url::port(){
	return v_port;
}

void xe_url::free(){
	string_.free();
}