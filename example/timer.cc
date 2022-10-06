#include <string.h>
#include "xutil/mem.h"
#include "xutil/log.h"
#include "xe/loop.h"
#include "xe/error.h"

int timer_callback(xe_loop& loop, xe_timer& timer){
	xe_print("timer callback executed");

	return 0;
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_timer timer;

	options.entries = 8; /* sqes and cqes */

	/* init */
	loop.init_options(options);

	timer.callback = timer_callback;
	loop.timer_ms(timer, 1000, 0, 0); /* set a timer for 1000 ms without repeating */

	loop.run();

	loop.close();

	return 0;
}