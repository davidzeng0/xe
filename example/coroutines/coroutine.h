#pragma once
#include <xe/loop.h>

/* coroutine task structure */
struct task{
	struct promise_type{
		task get_return_object(){
			return task(std::coroutine_handle<promise_type>::from_promise(*this));
		}

		auto initial_suspend(){
			return std::suspend_never();
		}

		auto final_suspend() noexcept{
			return std::suspend_never();
		}

		void return_void(){}

		void unhandled_exception(){}
	};

	std::coroutine_handle<promise_type> handle;

	task(std::coroutine_handle<promise_type> h){
		handle = h;
	}

	~task(){
		/* don't kill the coroutine, nothing to do */
	}
};