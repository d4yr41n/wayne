#include <cairo/cairo.h>
#include <assert.h>
#include <string.h>
#include <pango/pangocairo.h>

#include "state.h"
#include "render.h"


static void
cairo_set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(
		cairo,
		(color >> 24 & 0xff) / 255.,
		(color >> 16 & 0xff) / 255.,
		(color >> 8 & 0xff) / 255.,
		(color >> 0 & 0xff) / 255.
	);
}


void
render(void *data, struct state *state)
{
	int stride = state->width * 4;

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
	  data,
		CAIRO_FORMAT_ARGB32,
		state->width,
		state->height,
		stride
	);
	cairo_t *cairo = cairo_create(surface);

	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_source_u32(cairo, state->normal_bg);
	cairo_paint(cairo);

	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoFontDescription *desc = pango_font_description_from_string(state->font);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	for (int i = 0; i < state->menu_length; i++) {
		if (i == state->menu_select) {
			cairo_set_source_u32(cairo, state->select_bg);
			cairo_rectangle(cairo, 0, i * state->line_height, state->width, state->line_height);
			cairo_fill(cairo);
		}

		if (i == state->menu_select)
			cairo_set_source_u32(cairo, state->select_fg);
		else
			cairo_set_source_u32(cairo, state->normal_fg);
		pango_layout_set_text(layout, state->menu[i], -1);
		cairo_move_to(cairo, 0, ((i + 1) * state->line_height - (float)state->font_height) - (state->line_height - (float)state->font_height) / 2);
		pango_cairo_show_layout(cairo, layout);
	}
	g_object_unref(layout);
}

