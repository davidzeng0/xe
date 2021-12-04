#include "url.h"
#include "../string.h"
#include "../mem.h"

using namespace xe_net;

static inline bool letter(char c){
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline bool alphaxe_numeric(char c){
	return letter(c) || xe_cisi(c);
}

static inline bool unreserved(char c){
	return alphaxe_numeric(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

static inline bool schemechr(char c){
	return alphaxe_numeric(c) || c == '-' || c == '.' || c == '+';
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
	_string = string;
}

xe_string xe_url::string(){
	return _string;
}

int xe_url::parse(){
	uint i = 0;
	uint scheme = 0, authl = 0, user = 0, host = 0, path = 0;

	if(0 < _string.length()){
		switch(_string[0]){
			case '[':
				goto host;
			case ']':
				return XE_MALFORMED_URL;
			default:
				if(!letter(_string[0]))
					goto auth;
		}
	}

	while(i < _string.length()){
		switch(_string[i]){
			case ':': {
				uint left = i++;

				if(i < _string.length())
					switch(_string[i]){
						case '[':
							scheme = left;
							authl = i;
							user = i;

							goto host;
						case ']':
							return XE_MALFORMED_URL;
						case '/':
							scheme = left;

							if(++i < _string.length() && _string[i] == '/')
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

							if(_string[i] == '#'){
								path = i;

								goto finish;
							}

							goto path;
						case '@':
							authl = i;
							user = ++i;

							if(i < _string.length() && _string[i] == '[')
								goto host;

							goto auth;
						default:
							if(!xe_cisi(_string[i])){
								scheme = left;
								authl = left + 1;
								user = authl;

								goto auth;
							}

							i++;
					}
				while(i < _string.length())
					switch(_string[i]){
						case '[':
							return XE_MALFORMED_URL;
						case ']':
							return XE_MALFORMED_URL;
						case '/':
						case '?':
						case '#':
							host = i;

							if(_string[i] == '#'){
								path = i;

								goto finish;
							}

							goto path;
						case '@':
							scheme = left;
							authl = left + 1;
							user = ++i;

							if(i < _string.length() && _string[i] == '[')
								goto host;
							goto auth;
						default:
							if(!xe_cisi(_string[i])){
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

				if(i < _string.length() && _string[i] == '[')
					goto host;
				goto auth;
			case '/':
			case '?':
			case '#':
				host = i;

				if(_string[i] == '#'){
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

	if(i < _string.length()){
		switch(_string[i]){
			case '@':
				user = ++i;

				if(i < _string.length() && _string[i] == '[')
					goto host;
			case '/':
			case '?':
			case '#':
				host = i;

				if(_string[i] == '#'){
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

	while(i < _string.length()){
		switch(_string[i]){
			case '@':
				user = ++i;

				if(i < _string.length() && _string[i] == '[')
					goto host;
				break;
			case '/':
			case '?':
			case '#':
				host = i;

				if(_string[i] == '#'){
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

	while(i < _string.length()){
		switch(_string[i]){
			case '/':
			case '?':
			case '#':
				host = i;

				if(_string[i] == '#'){
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

	while(i < _string.length()){
		switch(_string[i]){
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
		if(!schemechr(_string[i]))
			return XE_MALFORMED_URL;
	if(user > 0)
		for(uint i = authl; i < user - 1; i++)
			if(!userchr(_string[i]))
				return XE_MALFORMED_URL;
	i = user;

	if(i < _string.length() && _string[i] == '['){
		uint k = host;

		while(i < host){
			if(_string[i] == ']'){
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
			if(_string[i] == ':'){
				if(k < host)
					return XE_MALFORMED_URL;
				k = i++;
			}else if(!regnamechr(_string[i]))
				return XE_MALFORMED_URL;
			else
				i++;
		i = k;
	}

	if(i < _string.length() && _string[i] == ':'){
		n_host = i;

		uint port = 0;

		while(++i < host)
			if(!xe_cisi(_string[i]))
				return XE_MALFORMED_URL;
			else{
				port = port * 10 + xe_ctoi(_string[i]);

				if(port > 0xffff)
					return XE_MALFORMED_URL;
			}
		v_port = port;
	}else{
		n_host = host;
		v_port = -1;
	}

	n_scheme = scheme;
	n_authl = authl;
	n_user = user;
	n_port = host;
	n_path = path;

	return 0;
}

xe_string xe_url::scheme(){
	return _string.substring(0, n_scheme);
}

xe_string xe_url::user_info(){
	return _string.substring(n_authl, n_user);
}

xe_string xe_url::host(){
	return _string.substring(n_user, n_host);
}

xe_string xe_url::path(){
	return _string.substring(n_port, n_path);
}

int xe_url::port(){
	return v_port;
}

void xe_url::free(){
	_string.free();
}