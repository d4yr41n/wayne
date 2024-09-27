#include <string.h>
#include <wayland-client.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/timerfd.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "state.h"
#include "render.h"
#include "wayland.h"
#include "shm.h"

static void noop() {}

void
wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release
};

static void
zwlr_layer_surface_v1_configure(void *data,
		struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
		uint32_t serial, uint32_t width, uint32_t height)
{
	struct state *state = data;
	state->width = width;
	state->height = height;
	zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);

	struct wl_buffer *buffer = create_buffer(state);
	wl_surface_attach(state->wl_surface, buffer, 0, 0);
	wl_surface_commit(state->wl_surface);
}

static const struct zwlr_layer_surface_v1_listener zwlr_layer_surface_v1_listener = {
	.configure = zwlr_layer_surface_v1_configure
};

static void
wl_keyboard_keymap(void *data,
  struct wl_keyboard *wl_keyboard,
  uint32_t format, int32_t fd, uint32_t size)
{
    struct state *state = data;
    assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

    char *map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(map_shm != MAP_FAILED);

    state->xkb_keymap = xkb_keymap_new_from_string(
        state->xkb_context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map_shm, size);
    close(fd);

  	state->xkb_state = xkb_state_new(state->xkb_keymap);
}

static void
keypress(struct state *state, enum wl_keyboard_key_state key_state, xkb_keysym_t sym)
{
	if (key_state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		return;
	}

	bool update = false;
	switch (sym) {
		case XKB_KEY_Escape:
			state->run = false;
			break;
		case XKB_KEY_j:
			if (state->menu_select < state->menu_length - 1) {
				state->menu_select++;
				update = true;
			}
			break;
		case XKB_KEY_k:
			if (state->menu_select > 0) {
				state->menu_select--;
				update = true;
			}
			break;
		case XKB_KEY_Return:
			printf("%s\n", state->menu[state->menu_select]);
			state->run = false;
			break;
	}
	if (update) {
			wl_surface_attach(state->wl_surface, create_buffer(state), 0, 0);
			wl_surface_damage_buffer(state->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
			wl_surface_commit(state->wl_surface);
		}
}

static void
wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, uint32_t time, uint32_t key, uint32_t _key_state)
{
	struct state *state = data;

	enum wl_keyboard_key_state key_state = _key_state;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(state->xkb_state, key + 8);
	keypress(state, key_state, sym);

	if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED && state->repeat_period >= 0) {
		state->repeat_key_state = key_state;
		state->repeat_sym = sym;

		struct itimerspec spec = { 0 };
		spec.it_value.tv_sec = state->repeat_delay / 1000;
		spec.it_value.tv_nsec = (state->repeat_delay % 1000) * 1000000l;
		timerfd_settime(state->repeat_timer, 0, &spec, NULL);
	} else if (key_state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		struct itimerspec spec = { 0 };
		timerfd_settime(state->repeat_timer, 0, &spec, NULL);
	}
}

void wl_keyboard_repeat(struct state *state) {
	keypress(state, state->repeat_key_state, state->repeat_sym);
	struct itimerspec spec = { 0 };
	spec.it_value.tv_sec = state->repeat_period / 1000;
	spec.it_value.tv_nsec = (state->repeat_period % 1000) * 1000000l;
	timerfd_settime(state->repeat_timer, 0, &spec, NULL);
}


static void
wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
		int32_t rate, int32_t delay) {
	struct state *state = data;
	state->repeat_delay = delay;
	if (rate > 0) {
		state->repeat_period = 1000 / rate;
	} else {
		state->repeat_period = -1;
	}
}

static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed,
		uint32_t mods_latched, uint32_t mods_locked,
		uint32_t group) {
	struct state *state = data;
	xkb_state_update_mask(state->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
	.enter = noop,
	.leave = noop,
	.keymap = wl_keyboard_keymap,
	.key = wl_keyboard_key,
	.repeat_info = wl_keyboard_repeat_info,
	.modifiers = wl_keyboard_modifiers,
};

static void
seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps) {
	struct state *state = data;
	if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
		state->wl_keyboard = wl_seat_get_keyboard(seat);
 		wl_keyboard_add_listener(state->wl_keyboard, &wl_keyboard_listener, state);
		state->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		state->repeat_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	}
}

static const struct wl_seat_listener wl_seat_listener = {
	.capabilities = seat_capabilities,
	.name = noop
};


static void
registry_global(void *data, struct wl_registry *wl_registry,
		uint32_t name, const char *interface, uint32_t version)
{
	struct state *state = data;
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->wl_shm = wl_registry_bind(
			wl_registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->wl_compositor = wl_registry_bind(
			wl_registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->zwlr_layer_shell_v1 = wl_registry_bind(
			wl_registry, name, &zwlr_layer_shell_v1_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		state->wl_output = wl_registry_bind(
			wl_registry, name, &wl_output_interface, 4);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 4);
		wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
	} else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
		state->wl_data_device_manager = wl_registry_bind(wl_registry, name, &wl_data_device_manager_interface, 3);
	}
}

static const struct wl_registry_listener wl_registry_listener = {
	.global = registry_global,
};

static void wl_data_device_selection(void *data, struct wl_data_device *data_device,
		struct wl_data_offer *data_offer) {
	struct state *state = data;
	state->wl_data_offer = data_offer;
}

static const struct wl_data_device_listener wl_data_device_listener = {
	.data_offer = noop,
	.enter = noop,
	.leave = noop,
	.motion = noop,
	.drop = noop,
	.selection = wl_data_device_selection,
};

void
wayland_init(struct state *state)
{
	state->wl_display = wl_display_connect(NULL);
	state->wl_registry = wl_display_get_registry(state->wl_display);
	wl_registry_add_listener(state->wl_registry, &wl_registry_listener, state);
	wl_display_roundtrip(state->wl_display);
	
	state->wl_data_device = wl_data_device_manager_get_data_device(
			state->wl_data_device_manager, state->wl_seat);
	wl_data_device_add_listener(state->wl_data_device, &wl_data_device_listener, state);
	
	wl_display_roundtrip(state->wl_display);


	state->wl_surface = wl_compositor_create_surface(state->wl_compositor);
	state->zwlr_layer_surface_v1 = zwlr_layer_shell_v1_get_layer_surface(
		state->zwlr_layer_shell_v1,
		state->wl_surface,
		state->wl_output,
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		"ergo"
	);
	zwlr_layer_surface_v1_set_anchor(state->zwlr_layer_surface_v1, 0);
	zwlr_layer_surface_v1_set_size(state->zwlr_layer_surface_v1, state->width, state->height);
	zwlr_layer_surface_v1_set_exclusive_zone(state->zwlr_layer_surface_v1, -1);
	zwlr_layer_surface_v1_add_listener(state->zwlr_layer_surface_v1, &zwlr_layer_surface_v1_listener, state);
	zwlr_layer_surface_v1_set_keyboard_interactivity(state->zwlr_layer_surface_v1, true);

	wl_surface_commit(state->wl_surface);
	wl_display_roundtrip(state->wl_display);
}

struct wl_buffer *
create_buffer(struct state *state)
{
	int stride = state->width * 4;
	int size = stride * state->height;

	int fd = allocate_shm_file(size);
	if (fd == -1) {
		return NULL;
	}

	// uint32_t 
	void *data = mmap(NULL, size,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
			state->width, state->height, stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	render(data, state);

	munmap(data, size);
	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	return buffer;
}
