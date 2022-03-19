#pragma once
#include "../common.h"
#include "../string.h"

namespace xe_net{

class xe_url{
private:
	xe_string string_;

	uint n_scheme;
	uint n_authl;
	uint n_user;
	uint n_host;
	uint n_port;
	uint n_path;
	uint v_port;
public:
	xe_url();
	xe_url(xe_string string);

	xe_string string();

	int parse();

	xe_string scheme();
	xe_string user_info();
	xe_string host();
	xe_string path();

	uint port();

	void free();
};

}