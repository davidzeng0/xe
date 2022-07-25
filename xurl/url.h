#pragma once
#include "xutil/xutil.h"
#include "xutil/string.h"

namespace xurl{

class xe_url{
private:
	xe_string string;

	uint n_scheme;
	uint n_authl;
	uint n_user;
	uint n_host;
	uint n_port;
	uint n_path;
	uint v_port;
public:
	xe_url(){}
	xe_url(xe_string&& url);

	xe_url(xe_url&& other);
	xe_url& operator=(xe_url&& other);

	xe_url(const xe_url& other) = delete;
	xe_url& operator=(const xe_url& other) = delete;

	xe_string_view href() const;

	int parse();

	xe_string_view scheme() const;
	xe_string_view user_info() const;
	xe_string_view host() const;
	xe_string_view hostname() const;
	xe_string_view path() const;

	uint port() const;
	void free();

	~xe_url();
};

}