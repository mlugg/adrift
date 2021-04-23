#include "draw.h"
#include "common.h"
#include "calc.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEXT_PAD 3

static void set_color_cfg(struct state *s, const char *k, float r, float g, float b, float a) {
	config_get_color(s->cfg, k, &r, &g, &b, &a);
	cairo_set_source_rgba(s->cr, r, g, b, a);
}

// To stop the timer position doing weird things, we assume the width of
// digits is constant
int get_approx_time_width(struct state *s, const char *str) {
	size_t len = strlen(str) + 1;
	char *buf = malloc(len);
	for (size_t i = 0; i < len; ++i) {
		char c = str[i];
		if (c >= '0' && c <= '9') {
			buf[i] = '0';
		} else {
			buf[i] = c;
		}
	}
	cairo_text_extents_t ext;
	cairo_text_extents(s->cr, buf, &ext);
	free(buf);
	return ext.width;
}

const char *format_time(uint64_t total, char prefix, int prec) {
	if (total == UINT64_MAX) return "-";

	static char buf[64];

	// The logic for calculating frac is pretty simple but it's still
	// easier to just switch on the precision
	uint64_t frac;
	switch (prec) {
	case 0:
		frac = 0;
		break;
	case 1:
		frac = (total / 100000) % 10;
		break;
	case 2:
		frac = (total / 10000) % 100;
		break;
	case 3:
		frac = (total / 1000) % 1000;
		break;
	case 4:
		frac = (total / 100) % 10000;
		break;
	case 5:
		frac = (total / 10) % 100000;
		break;
	case 6:
	default:
		frac = total % 1000000;
		break;
	}

	total /= 1000000;
	uint64_t secs = total % 60;
	total /= 60;
	uint64_t mins = total % 60;
	total /= 60;
	uint64_t hrs = total;

	char *ptr = buf;
	size_t bufsz = sizeof buf;
	if (prefix) {
		buf[0] = prefix;
		++ptr;
		--bufsz;
	}

	if (hrs) {
		snprintf(ptr, bufsz, "%"PRIu64":%02"PRIu64":%02"PRIu64".%0*"PRIu64, hrs, mins, secs, prec, frac);
	} else if (mins) {
		snprintf(ptr, bufsz, "%"PRIu64":%02"PRIu64".%0*"PRIu64, mins, secs, prec, frac);
	} else {
		snprintf(ptr, bufsz, "%"PRIu64".%0*"PRIu64, secs, prec, frac);
	}

	return buf;
}

enum align {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT,
	ALIGN_RIGHT_TIME,
};

int get_font_height(struct state *s) {
	cairo_font_extents_t ext;
	cairo_font_extents(s->cr, &ext);
	return ext.ascent + ext.descent;
}

void draw_text(struct state *s, const char *str, int w, int h, int *y, bool update_y, enum align align, int off) {
	int width;
	if (align == ALIGN_RIGHT_TIME) {
		width = get_approx_time_width(s, str);
	} else {
		cairo_text_extents_t ext;
		cairo_text_extents(s->cr, str, &ext);
		width = ext.width;
	}
	cairo_font_extents_t fext;
	cairo_font_extents(s->cr, &fext);
	int old = *y;
	*y += TEXT_PAD + fext.ascent;
	switch (align) {
	case ALIGN_LEFT:
		cairo_move_to(s->cr, off + TEXT_PAD, *y);
		break;
	case ALIGN_CENTER:
		cairo_move_to(s->cr, off + (w - off - width) / 2, *y);
		break;
	case ALIGN_RIGHT:
	case ALIGN_RIGHT_TIME:
		cairo_move_to(s->cr, w - off - TEXT_PAD - width, *y);
		break;
	}
	cairo_show_text(s->cr, str);
	*y += TEXT_PAD + fext.descent;
	if (!update_y) *y = old;
}

void draw_splits(struct state *s, int w, int h, int *y, int off, struct split *splits, size_t nsplits) {
	cairo_set_font_size(s->cr, 16.0f);
	for (size_t i = 0; i < nsplits; ++i) {
		struct times times = get_split_times(&splits[i]);
		uint64_t comparison = get_comparison(s, times);
		uint64_t cur = times.cur;

		bool active = !splits[i].is_group && splits[i].split.id == s->active_split;

		if (active) {
			set_color_cfg(s, "col_active_split", 1.0, 0.0, 0.0, 1.0);
			cairo_rectangle(s->cr, 0, *y, w, get_font_height(s) + 2 * TEXT_PAD);
			cairo_fill(s->cr);
		}

		// For the active split, we want to display the delta as soon as it goes over gold
		if (active && (s->split_time > times.best)) {
			cur = s->timer;
		}

		if (cur == UINT64_MAX) {
			if (comparison != UINT64_MAX) {
				// There's a comparison, but no current time - draw comparison
				set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
				draw_text(s, format_time(comparison, 0, 2), w, h, y, false, ALIGN_RIGHT, 0);
			}
			// No time at all - draw nothing
		} else {
			// There's a time for the current run
			const char *delta = "-";
			if (comparison != UINT64_MAX) {
				if (cur < comparison) delta = format_time(comparison - cur, '-', 2);
				else delta = format_time(cur - comparison, '+', 2);
			}

			if (times.golded_this_run && !splits[i].is_group) {
				set_color_cfg(s, "col_split_gold", 1.0, 0.9, 0.3, 1.0);
			} else if (comparison != UINT64_MAX) {
				if (cur < comparison) {
					set_color_cfg(s, "col_split_ahead", 0.2, 1.0, 0.2, 1.0);
				} else {
					set_color_cfg(s, "col_split_behind", 1.0, 0.2, 0.2, 1.0);
				}
			} else {
				set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
			}

			draw_text(s, delta, w, h, y, false, ALIGN_RIGHT_TIME, 65);

			// For splits before active, draw the time obtained
			// For splits after, draw the comparison
			set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
			if (splits[i].split.id >= s->active_split) {
				draw_text(s, format_time(comparison, 0, 2), w, h, y, false, ALIGN_RIGHT, 0);
			} else {
				draw_text(s, format_time(cur, 0, 2), w, h, y, false, ALIGN_RIGHT, 0);
			}
		}

		set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
		draw_text(s, splits[i].name, w, h, y, true, ALIGN_LEFT, off);

		if (splits[i].is_group && splits[i].group.expanded) {
			draw_splits(s, w, h, y, off + 20, splits[i].group.splits, splits[i].group.nsplits);
		}
	}
}

void draw_widget(struct state *s, enum widget_type t, int w, int h, int *y) {
	switch (t) {
	case WIDGET_GAME_NAME:
		set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
		cairo_set_font_size(s->cr, 23.0f);
		draw_text(s, s->game_name, w, h, y, true, ALIGN_CENTER, 0);
		break;
	case WIDGET_CATEGORY_NAME:
		set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
		cairo_set_font_size(s->cr, 16.0f);
		draw_text(s, s->category_name, w, h, y, true, ALIGN_CENTER, 0);
		break;
	case WIDGET_TIMER:
		set_color_cfg(s, "col_timer", 1.0, 1.0, 1.0, 1.0);
		if (s->active_split != -1) {
			struct times times = get_split_times(get_split_by_id(s, s->active_split));
			uint64_t comparison = get_comparison(s, times);
			if (s->timer < comparison) {
				set_color_cfg(s, "col_timer_ahead", 1.0, 1.0, 1.0, 1.0);
			} else {
				set_color_cfg(s, "col_timer_behind", 1.0, 1.0, 1.0, 1.0);
			}
		}
		cairo_set_font_size(s->cr, 26.0f);
		draw_text(s, format_time(s->timer, 0, 3), w, h, y, true, ALIGN_RIGHT_TIME, 0);
		break;
	case WIDGET_SPLIT_TIMER:
		set_color_cfg(s, "col_timer", 1.0, 1.0, 1.0, 1.0);
		if (s->active_split != -1) {
			struct times times = get_split_times(get_split_by_id(s, s->active_split));
			uint64_t comparison = get_comparison(s, times);

			uint64_t prev_comparison = 0;
			if (s->active_split > 0) {
				struct times prev_times = get_split_times(get_split_by_id(s, s->active_split-1));
				prev_comparison = get_comparison(s, prev_times);
			}

			if (s->split_time < comparison - prev_comparison) {
				set_color_cfg(s, "col_timer_ahead", 1.0, 1.0, 1.0, 1.0);
			} else {
				set_color_cfg(s, "col_timer_behind", 1.0, 1.0, 1.0, 1.0);
			}
		}
		cairo_set_font_size(s->cr, 24.0f);
		draw_text(s, format_time(s->split_time, 0, 3), w, h, y, true, ALIGN_RIGHT_TIME, 0);
		break;
	case WIDGET_SPLITS:
		draw_splits(s, w, h, y, 0, s->splits, s->nsplits);
		break;
	case WIDGET_SUM_OF_BEST:
		set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
		cairo_set_font_size(s->cr, 17.0f);
		draw_text(s, "Sum of best:", w, h, y, false, ALIGN_LEFT, 0);
		draw_text(s, format_time(calc_sum_of_best(s), 0, 3), w, h, y, true, ALIGN_RIGHT, 0);
		break;
	case WIDGET_BEST_POSSIBLE_TIME:
		set_color_cfg(s, "col_text", 1.0, 1.0, 1.0, 1.0);
		cairo_set_font_size(s->cr, 17.0f);
		draw_text(s, "Best possible time:", w, h, y, false, ALIGN_LEFT, 0);
		draw_text(s, format_time(calc_best_possible_time(s), 0, 3), w, h, y, true, ALIGN_RIGHT, 0);
		break;
	}
}

void draw_handler(vtk_event ev, void *u) {
	struct state *s = u;

	int w, h;
	vtk_window_get_size(s->win, &w, &h);

	cairo_rectangle(s->cr, 0, 0, w, h);
	set_color_cfg(s, "col_background", 0.0, 0.0, 0.0, 0.0);
	cairo_fill(s->cr);

	int y = 0;

	for (size_t i = 0; i < s->nwidgets; ++i) {
		draw_widget(s, s->widgets[i], w, h, &y);
	}
}

