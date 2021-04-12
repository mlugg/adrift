#include "draw.h"
#include "common.h"
#include "calc.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#define TEXT_PAD 3
#define TIMER_PRECISION 3

const char *format_time(uint64_t total, char prefix, bool long_format) {
	if (total == UINT64_MAX) return "-";

	static char buf[64];

	total /= 1000;
	uint64_t ms = total % 1000;
	total /= 1000;
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

	if (hrs && long_format) {
		snprintf(ptr, bufsz, "%"PRIu64":%02"PRIu64":%02"PRIu64".%0*"PRIu64, hrs, mins, secs, TIMER_PRECISION, ms);
	} else if (hrs) {
		snprintf(ptr, bufsz, "%"PRIu64":%02"PRIu64":%02"PRIu64, hrs, mins, secs);
	} else if (mins && long_format) {
		snprintf(ptr, bufsz, "%"PRIu64":%02"PRIu64".%0*"PRIu64, mins, secs, TIMER_PRECISION, ms);
	} else if (mins) {
		snprintf(ptr, bufsz, "%"PRIu64":%02"PRIu64, mins, secs);
	} else {
		snprintf(ptr, bufsz, "%"PRIu64".%0*"PRIu64, secs, TIMER_PRECISION, ms);
	}

	return buf;
}

enum align {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT,
};

int get_font_height(struct state *s) {
	cairo_font_extents_t ext;
	cairo_font_extents(s->cr, &ext);
	return ext.ascent + ext.descent;
}

void draw_text(struct state *s, const char *str, int w, int h, int *y, bool update_y, enum align align, int off) {
	cairo_text_extents_t ext;
	cairo_text_extents(s->cr, str, &ext);
	cairo_font_extents_t fext;
	cairo_font_extents(s->cr, &fext);
	int old = *y;
	*y += TEXT_PAD + fext.ascent;
	switch (align) {
	case ALIGN_LEFT:
		cairo_move_to(s->cr, off + TEXT_PAD, *y);
		break;
	case ALIGN_CENTER:
		cairo_move_to(s->cr, off + (w - off - ext.width) / 2, *y);
		break;
	case ALIGN_RIGHT:
		cairo_move_to(s->cr, w - off - TEXT_PAD - ext.width, *y);
		break;
	}
	cairo_show_text(s->cr, str);
	*y += TEXT_PAD + fext.descent;
	if (!update_y) *y = old;
}

void draw_splits(struct state *s, int w, int h, int *y, int off, struct split *splits, size_t nsplits) {
	cairo_set_font_size(s->cr, 13.0f);
	for (size_t i = 0; i < nsplits; ++i) {
		struct times times = get_split_times(&splits[i]);
		uint64_t comparison = get_comparison(s, times);
		uint64_t cur = times.cur;

		bool active = !splits[i].is_group && splits[i].split.id == s->active_split;

		if (active) {
			cairo_set_source_rgba(s->cr, 1, 0, 0, 1);
			cairo_rectangle(s->cr, 0, *y, w, get_font_height(s) + 2 * TEXT_PAD);
			cairo_fill(s->cr);
		}

		if (cur == UINT64_MAX) {
			if (comparison != UINT64_MAX) {
				// There's a comparison, but no current time - draw comparison
				cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
				draw_text(s, format_time(comparison, 0, false), w, h, y, false, ALIGN_RIGHT, 0);
			}
			// No time at all - draw nothing
		} else {
			// There's a time for the current run
			const char *delta = "-";
			if (comparison != UINT64_MAX) {
				if (cur < comparison) delta = format_time(comparison - cur, '-', false);
				else delta = format_time(cur - comparison, '+', false);
			}

			float r = 1.0f, g = 1.0f, b = 1.0f;
			if (times.golded_this_run && !splits[i].is_group) {
				r = 1.0f;
				g = 0.9f;
				b = 0.3f;
			} else if (comparison != UINT64_MAX) {
				if (cur < comparison) {
					r = 0.2f;
					g = 1.0f;
					b = 0.2f;
				} else {
					r = 1.0f;
					g = 0.2f;
					b = 0.2f;
				}
			}

			cairo_set_source_rgba(s->cr, r, g, b, 1);
			draw_text(s, delta, w, h, y, false, ALIGN_RIGHT, 65);

			cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
			draw_text(s, format_time(cur, 0, false), w, h, y, false, ALIGN_RIGHT, 0);
		}

		cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
		draw_text(s, splits[i].name, w, h, y, true, ALIGN_LEFT, off);

		if (splits[i].is_group && splits[i].group.expanded) {
			draw_splits(s, w, h, y, off + 20, splits[i].group.splits, splits[i].group.nsplits);
		}
	}
}

void draw_widget(struct state *s, enum widget_type t, int w, int h, int *y) {
	switch (t) {
	case WIDGET_GAME_NAME:
		cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
		cairo_set_font_size(s->cr, 20.0f);
		draw_text(s, s->game_name, w, h, y, true, ALIGN_CENTER, 0);
		break;
	case WIDGET_CATEGORY_NAME:
		cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
		cairo_set_font_size(s->cr, 15.0f);
		draw_text(s, s->category_name, w, h, y, true, ALIGN_CENTER, 0);
		break;
	case WIDGET_TIMER:
		cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
		cairo_set_font_size(s->cr, 28.0f);
		draw_text(s, format_time(s->timer, 0, true), w, h, y, true, ALIGN_RIGHT, 0);
		break;
	case WIDGET_SPLIT_TIMER:
		cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
		cairo_set_font_size(s->cr, 21.0f);
		draw_text(s, format_time(s->split_time, 0, true), w, h, y, true, ALIGN_RIGHT, 0);
		break;
	case WIDGET_SPLITS:
		draw_splits(s, w, h, y, 0, s->splits, s->nsplits);
		break;
	case WIDGET_SUM_OF_BEST:
		cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
		cairo_set_font_size(s->cr, 15.0f);
		draw_text(s, "Sum of best:", w, h, y, false, ALIGN_LEFT, 0);
		draw_text(s, format_time(calc_sum_of_best(s), 0, true), w, h, y, true, ALIGN_RIGHT, 0);
		break;
	case WIDGET_BEST_POSSIBLE_TIME:
		cairo_set_source_rgba(s->cr, 1, 1, 1, 1);
		cairo_set_font_size(s->cr, 15.0f);
		draw_text(s, "Best possible time:", w, h, y, false, ALIGN_LEFT, 0);
		draw_text(s, format_time(calc_best_possible_time(s), 0, true), w, h, y, true, ALIGN_RIGHT, 0);
		break;
	}
}

void draw_handler(vtk_event ev, void *u) {
	struct state *s = u;

	int w, h;
	vtk_window_get_size(s->win, &w, &h);

	cairo_rectangle(s->cr, 0, 0, w, h);
	cairo_set_source_rgba(s->cr, 0, 0, 0, 0.5);
	cairo_fill(s->cr);

	int y = 0;

	for (size_t i = 0; i < s->nwidgets; ++i) {
		draw_widget(s, s->widgets[i], w, h, &y);
	}
}

