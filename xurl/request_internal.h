#pragma once
#include "request.h"

namespace xurl{

class xe_request_internal : public xe_request{
public:
	void set_state(xe_request_state state);
	int write(xe_ptr data, size_t len);
	void complete(int error);
};

}