#include "xutil/assert.h"
#include "poll.h"
#include "../error.h"

void xe_poll::poll_cb(xe_req& req, int events){
	xe_poll& handle = xe_containerof(req, &xe_poll::poll_req);
	int res;

	if(!handle.active) [[unlikely]]
		goto done;
	res = events;

	if(events == XE_ECANCELED)
		events = 0;
	else if(events < 0) [[unlikely]]
		goto err;
	if(handle.updated) [[unlikely]]{
		/* events were updated mid-poll
		 * check if there are any events to report
		 */
		events &= handle.events_;

		if(!events) goto ok;
	}else if(handle.events_ & XE_POLL_ONESHOT){
		handle.active = false;
	}

	if(handle.poll_callback) [[likely]] {
		/* temporarily prevent a modify req from being started */
		handle.updated = true;

		handle.poll_callback(handle, events);
	}

	if(!handle.active) [[unlikely]]
		goto done;
ok:
	handle.updated = false;
	res = handle.loop_ -> run(handle.poll_req, xe_op::poll(handle.fd_, handle.events_));

	if(!res) [[likely]]
		return;
err:
	handle.active = false;
	handle.polling = false;
	handle.updated = false;

	if(handle.poll_callback)
		handle.poll_callback(handle, res);
	return;
done:
	/* events aren't wanted anymore */
	handle.polling = false;
	handle.updated = false;

	handle.check_close();
}

void xe_poll::cancel_cb(xe_req& req, int cancel_res){
	xe_poll& handle = xe_containerof(req, &xe_poll::cancel_req);
	int res;

	handle.modifying = false;

	if(!handle.restart){
		handle.check_close();

		return;
	}

	handle.restart = false;
	res = handle.loop_ -> cancel(handle.cancel_req, handle.poll_req, xe_op::poll_cancel());

	if(res == XE_EINPROGRESS)
		handle.modifying = true;
	else{
		xe_assert(res);

		/* cancel subsequent callbacks and inform the user of the error */
		handle.active = false;

		if(handle.poll_callback && !handle.closing) handle.poll_callback(handle, res);
	}
}

int xe_poll::update_poll(){
	if(updated){
		/* poll already completing */
		return 0;
	}

	if(modifying){
		/* poll completed but the modify request didn't return yet
		 * restart modify request to update the poll
		 */
		restart = true;
	}else{
		/* force the poll request to return and restart using new events */
		int res = loop_ -> cancel(cancel_req, poll_req, xe_op::poll_cancel(), closing ? &cancel_info : null);

		if(res != XE_EINPROGRESS){
			xe_assert(res);

			return res ?: XE_FATAL;
		}

		modifying = true;
	}

	updated = true;

	return 0;
}

void xe_poll::check_close(){
	if(!closing || modifying || polling)
		return;
	closing = false;

	if(close_callback) close_callback(*this);
}

int xe_poll::poll(uint events){
	/*
	 * ACTIVE: if we should keep polling
	 * POLLING: if the poll request is currently active
	 * MODIFYING: if the modify (cancel) request is currently active
	 * UPDATED: if the events were updated or the poll was cancelled (events set to 0),
	 * 	will be reset when the up-to-date state is seen by the poll request
	 * RESTART: if we should restart the modify request
	 *
	 * ACTIVE must imply POLLING
	 * UPDATED must imply POLLING
	 * !POLLING must imply !ACTIVE && !UPDATED
	 * !UPDATED && !ACTIVE must imply !POLLING
	 *
	 * RESTART must imply MODIFYING
	 * RESTART must imply UPDATED (two poll requests cannot return in between two cancel requests)
	 * !MODIFYING must imply !RESTART
	 *
	 * events == 0: stop poll
	 * events != 0: start or modify poll
	 *
	 * the poll request can return both before and after the modify request
	 * a second modification needs to be started if
	 *  - the events are modified
	 *  - the poll request is cancelled
	 *  - the poll request returns (and restarted with the new events)
	 *  - the events are modified
	 *  - the modify request returns
	 *
	 * 2 main states:
	 *
	 * idle: !ACTIVE
	 * 	start: base state
	 * 	 - flags: !POLLING, !RESTART && !UPDATED implied
	 * 	 - action: start polling
	 * 	start: after cancel, modify request completed first
	 * 	 - flags: POLLING, !RESTART && UPDATED implied
	 * 	 - action: set active
	 * 	stop - do nothing
	 *
	 * active: ACTIVE
	 * 	modify: base state
	 * 	 - flags: !UPDATED && !MODIFYING, !RESTART && POLLING implied
	 * 	 - action: start modify request
	 * 	modify: the modify request has not returned yet
	 * 	 - flags: !UPDATED && MODIFYING, POLLING && !RESTART implied
	 * 	 - action: set restart
	 * 	modify: a request to change events is sent, poll request has not returned
	 * 	 - flags: UPDATED, POLLING && !RESTART implied, MODIFYING does not matter
	 * 	 - action: just update events bitfield
	 * 	stop - follow same semantics as modify, just that events are set to 0 and active is unset
	 */
	if(closing)
		return XE_STATE;
	if(events){
		if(events & XE_POLL_EDGE_TRIGGERED)
			return XE_EOPNOTSUPP;
		/* events that are always monitored
		 * mask them so that when an event update is processed
		 * the correct events are returned
		 */
		events |= XE_POLL_ERR | XE_POLL_HUP | XE_POLL_NVAL | XE_POLL_RDHUP;

		if(!polling){
			xe_return_error(loop_ -> run(poll_req, xe_op::poll(fd_, events)));

			polling = true;
		}else{
			xe_return_error(update_poll());
		}

		/* if the poll request has not returned yet,
		 * just set active to true and it will automatically
		 * poll again
		 */
		active = true;
	}else if(active){
		active = false;

		return update_poll();
	}

	events_ = events;

	return 0;
}

int xe_poll::close(){
	if(closing)
		return XE_EALREADY;
	if(!polling && !modifying)
		return 0;
	int res = 0;

	closing = true;
	active = false;

	/* handle the extra case where
	 * active is unset, but the cancel request failed to submit.
	 * try to force submit the cancel request
	 */
	if(polling)
		res = update_poll();
	return res ?: XE_EINPROGRESS;
}