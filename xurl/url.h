#pragma once
#include "xstd/types.h"
#include "xstd/string.h"
#include "xutil/util.h"

namespace xurl{

class xe_url{
private:
	xe_string string;

	struct{
		uint scheme;
		uint authl;
		uint user;
		uint host;
		uint port;
		uint path;
	} offsets;

	ushort port_;
public:
	xe_url() = default;
	xe_url(xe_string&& url);

	xe_url(xe_url&& other);
	xe_url& operator=(xe_url&& other);

	xe_disable_copy(xe_url)

	xe_string_view href() const;

	int parse();

	xe_string_view scheme() const;
	xe_string_view user_info() const;
	xe_string_view host() const;
	xe_string_view hostname() const;
	xe_string_view path() const;

	ushort port() const;
	void clear();

	~xe_url();
};

}