#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <pango/pangocairo.h>

#include "pango/pango-font.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "state.h" 
#include "wayland.h" 

static int
get_font_height(const char *fontname)
{
	PangoFontMap *fontmap = pango_cairo_font_map_get_default();
	PangoContext *context = pango_font_map_create_context(fontmap);
	PangoFontDescription *desc = pango_font_description_from_string(fontname);
	PangoFont *font = pango_font_map_load_font(fontmap, context, desc);
	PangoFontMetrics *metrics = pango_font_get_metrics(font, NULL);
	int height = pango_font_metrics_get_height(metrics) / PANGO_SCALE;
	pango_font_metrics_unref(metrics);
	pango_font_description_free(desc);
	g_object_unref(context);
	g_object_unref(font);
	return height;
}

static int
get_text_width(const char *font, const char *text) {
	int width;
	cairo_t *cairo = cairo_create(NULL);
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, text, -1);
	pango_layout_get_size(layout, &width, NULL);
	g_object_unref(layout);
	return width / PANGO_SCALE;
}

static bool
parse_color(const char *color, uint32_t *result)
{
	if (color[0] == '#') {
		++color;
	}
	size_t len = strlen(color);
	if ((len != 6 && len != 8) || !isxdigit(color[0]) || !isxdigit(color[1])) {
		return false;
	}
	char *ptr;
	uint32_t parsed = (uint32_t)strtoul(color, &ptr, 16);
	if (*ptr != '\0') {
		return false;
	}
	*result = len == 6 ? ((parsed << 8) | 0xff) : parsed;
	return true;
}

void
input(struct state *state)
{
	char buf[BUFSIZ];
	fgets(buf, BUFSIZ, stdin);
	char *split = strtok(buf, "|");

	do {
		state->menu[state->menu_length] = split;
		state->menu_length++;
		split = strtok(NULL, "|");
	} while (split);
}

struct state *
state_init(int argc, char *argv[]) {
  struct state *state = malloc(sizeof(struct state));
	state->font = "monospace 10";
	state->normal_bg = 0x000000ff;
	state->normal_fg = 0xffffffff;
	state->select_bg = 0x7f7f7fff;
	state->select_fg = 0xffffffff;
	state->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
	state->run = true;
	state->menu_length = 0;
	state->menu_select = 0;

	const char *usage = "Usage: ring [-f font] [-N color] [-n color] [-S color] [-s color]\n";
	int opt;
	while ((opt = getopt(argc, argv, "hf:N:n:S:s:")) != -1) {
		switch (opt) {
			case 'f':
				state->font = optarg;
				break;
			case 'N':
				if (!parse_color(optarg, &state->normal_bg)) {
					fprintf(stderr, "Invalid normal background color: %s", optarg);
				}
				break;
			case 'n':
				if (!parse_color(optarg, &state->normal_fg)) {
					fprintf(stderr, "Invalid normal foreground color: %s", optarg);
				}
				break;
			case 'S':
				if (!parse_color(optarg, &state->select_bg)) {
					fprintf(stderr, "Invalid select background color: %s", optarg);
				}
				break;
			case 's':
				if (!parse_color(optarg, &state->select_fg)) {
					fprintf(stderr, "Invalid select foreground color: %s", optarg);
				}
				break;
			default:
				fprintf(stderr, "%s", usage);
				exit(EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "%s", usage);
		exit(EXIT_FAILURE);
	}

	int width;
	char buf[BUFSIZ];
	while (fgets(buf, BUFSIZ, stdin)) {
		char *p = strchr(buf, '\n');
		printf("%s\n", p);
		if (p) {
			*p = '\0';
		}
		state->menu[state->menu_length] = strdup(buf);
		state->menu_length++;
		width = get_text_width(state->font, strdup(buf));
		if (width > state->width)
			state->width = width;
	}

	// state->width = 320;
	state->font_height = get_font_height(state->font);
	state->line_height = state->font_height + 7;
  state->height = state->menu_length * state->line_height;

  wayland_init(state);

  return state;
}
