#include <xe/loop.h>
#include <xutil/log.h>

int timer_callback(xe_loop& loop, xe_timer& timer){
	xe_print("timer callback executed");

	return 0;
}

int main(){
	using namespace std::chrono_literals;

	xe_loop loop;
	xe_timer timer;

	/* init */
	loop.init(8); /* 8 sqes and cqes */

	timer.callback = timer_callback;
	loop.timer(timer, 1000ms, 0s, 0); /* set a timer for 1000 ms without repeating */

	loop.run();

	loop.close();

	return 0;
}