#ifndef STATE_H
#define STATE_H

#include <stdio.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

struct state {
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_shm *wl_shm;
	struct wl_compositor *wl_compositor;
	struct wl_output *wl_output;
	struct wl_surface *wl_surface;
	struct zwlr_layer_shell_v1 *zwlr_layer_shell_v1;
	struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1;
	struct wl_seat *wl_seat;
	struct wl_data_device_manager *wl_data_device_manager;
	struct wl_data_device *wl_data_device;
	struct wl_data_offer *wl_data_offer;
	struct wl_keyboard *wl_keyboard;

	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;

	int repeat_timer;
	int repeat_delay;
	int repeat_period;
	enum wl_keyboard_key_state repeat_key_state;
	xkb_keysym_t repeat_sym;

	int line_height, font_height;
	int width, height;
	uint32_t normal_bg, normal_fg, select_bg, select_fg;
	char *font;
	char *menu[BUFSIZ];
	int menu_length;
	int menu_select;

	int anchor;

	bool run;
};

struct state *state_init(int argc, char *argv[]);
void input(struct state *state);
bool state_update(struct state *state);

#endif
