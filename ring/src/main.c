#include <poll.h>
#include <errno.h>
#include <string.h>

#include "state.h"
#include "wayland.h"

int
main(int argc, char *argv[])
{
	struct state *state = state_init(argc, argv);

	// input(state);
	
	struct pollfd fds[] = {
		{ wl_display_get_fd(state->wl_display), POLLIN },
		{ state->repeat_timer, POLLIN },
	};
	const size_t nfds = sizeof(fds) / sizeof(*fds);

	while (state->run) {
		errno = 0;
		do {
			if (wl_display_flush(state->wl_display) == -1 && errno != EAGAIN) {
				fprintf(stderr, "wl_display_flush: %s\n", strerror(errno));
				break;
			}
		} while (errno == EAGAIN);

		if (poll(fds, nfds, -1) < 0) {
			fprintf(stderr, "poll: %s\n", strerror(errno));
			break;
		}

		if (fds[0].revents & POLLIN) {
			if (wl_display_dispatch(state->wl_display) < 0) {
				state->run = false;
			}
		}

		if (fds[1].revents & POLLIN) {
			wl_keyboard_repeat(state);
		}
	}

	return 0;
}
