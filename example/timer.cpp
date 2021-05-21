#include <string.h>
#include "xe/mem.h"
#include "xe/debug.h"
#include "loop.h"

void timer_callback(xe_loop* loop, xe_timer* timer){
	xe_print("timer callback executed");
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_timer timer;

	int ret;

	/* xe lib expects all structs to be zeroed */
	xe_zero(&timer);
	xe_zero(&options);
	xe_zero(&loop);

	options.capacity = 8; /* 8 sqes and cqes */

	/* init */
	ret = xe_loop_init_options(&loop, &options);

	if(ret){
		xe_print("loop_init %s", strerror(-ret));

		return -1;
	}

	timer.callback = timer_callback;

	xe_loop_timer_ms(&loop, &timer, 1000, false); /* set a timer for 1000 ms without repeating */

	ret = xe_loop_run(&loop);

	if(ret){
		xe_print("loop_run %s", strerror(-ret));

		return -1;
	}

	xe_loop_destroy(&loop);

	return 0;
}