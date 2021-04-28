/* vdict.h
 *
 * A generic, ordered dictionary type inspired by Python's dict.
 */

/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef VDICT_NAME
#error "VDICT_NAME undefined. This is used as the struct name and as the function prefix"
#endif
#ifndef VDICT_KEY
#error "VDICT_KEY undefined. This is used as the key type"
#endif
#ifndef VDICT_VAL
#error "VDICT_VAL undefined. This is used as the value type"
#endif
#ifndef VDICT_HASH
#error "VDICT_HASH undefined. This is used as the key hash function (try vdict_hash_int or vdict_hash_string)"
#endif
#ifndef VDICT_EQUAL
#error "VDICT_EQUAL undefined. This is used to compare keys for equality (try vdict_eq_int or vdict_eq_string)"
#endif
#ifndef VDICT_LINK
#define VDICT_LINK
#endif

#ifndef _vdict_COMMON
#define _vdict_COMMON

// Hash functions {{{
static inline uint32_t vdict_hash_int(uint32_t x) {
	// From https://stackoverflow.com/a/12996028
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x;
}

static inline uint32_t vdict_hash_string(const char *s) {
	// djb2a
	uintmax_t hash = 5381;
	while (*s) hash = hash*33 ^ *s++;
	return hash;
}
// }}}

// Equality functions {{{
static inline _Bool vdict_eq_int(uint32_t a, uint32_t b) {
	return a == b;
}

static inline _Bool vdict_eq_string(const char *a, const char *b) {
	return !strcmp(a, b);
}
// }}}

#define _vdict_SPLAT_(a, b, c, d, e, ...) a##b##c##d##e
#define _vdict_SPLAT(...) _vdict_SPLAT_(__VA_ARGS__,,,)

#define _vdict_intern(name) _vdict_SPLAT(_, VDICT_NAME, _, name)
#define _vdict_extern(name) _vdict_SPLAT(VDICT_NAME, _, name)
#define _vdict VDICT_NAME
#define _vdict_entry _vdict_intern(entry)

#endif

struct _vdict;

// Create a new dictionary
VDICT_LINK struct _vdict *_vdict_extern(new)(void);

// Delete a dictionary
VDICT_LINK void _vdict_extern(free)(struct _vdict *d);

// Insert a key/value pair into a dictionary
// Returns 1 if the key was already in the dictionary, 0 if it was not, and -1 if out-of-memory
VDICT_LINK int _vdict_extern(put)(struct _vdict *d, VDICT_KEY k, VDICT_VAL v);

// Get the value of a key
// Returns 1 if the key was found, 0 otherwise
// If v is not NULL and the key was found, *v is set to the value
VDICT_LINK _Bool _vdict_extern(get)(struct _vdict *d, VDICT_KEY k, VDICT_VAL *v);

// Delete a key/value pair
// Returns 1 if the key was found, 0 otherwise
// If v is not NULL and the key was found, *v is set to the value before the entry is deleted
VDICT_LINK _Bool _vdict_extern(del)(struct _vdict *d, VDICT_KEY, VDICT_VAL *v);

#ifdef VDICT_IMPL
#undef VDICT_IMPL

struct _vdict_entry {
	uint32_t hash;
	_Bool removed;

	VDICT_KEY k;
	VDICT_VAL v;
};

struct _vdict {
	// Total number of entries
	uint32_t n_entry;
	// log_2 of number of allocated entries
	uint32_t ecap_e;
	// log_2 of number of allocated indices in `map`
	uint32_t mcap_e;

	// Entries referenced by indices in `map`
	struct _vdict_entry *ent;
	// The actual hash table. Stores indices into entries, 1-indexed, or 0 for empty cell
	uint32_t *map;
};

// Hash a key, returning an in-bounds value for the specified dict
static inline uint32_t _vdict_intern(hash)(struct _vdict *d, VDICT_KEY k) {
	return (VDICT_HASH(k)) >> (32 - d->mcap_e);
}

// Wrap an index to be in-bounds for the specified dict
static inline uint32_t _vdict_intern(wrap)(struct _vdict *d, uint32_t i) {
	return i & ((1 << d->mcap_e) - 1);
}

// Get the entry of a hash table index
static inline struct _vdict_entry *_vdict_intern(entry)(struct _vdict *d, uint32_t i) {
	return d->ent + d->map[i] - 1;
}

// Return 1 if the entry of the given hash table index exists and has a value, else 0
static inline _Bool _vdict_intern(exists)(struct _vdict *d, uint32_t i) {
	return d->map[i] && !_vdict_intern(entry)(d, i)->removed;
}

// Find the hash table index of a key
static uint32_t _vdict_intern(index)(struct _vdict *d, VDICT_KEY k, uint32_t h) {
	uint32_t i = h;
	for (;;) {
		if (!d->map[i]) return i;
		struct _vdict_entry *ent = _vdict_intern(entry)(d, i);

		if (!ent->removed && ent->hash == h && VDICT_EQUAL(ent->k, k)) {
			return i;
		}

		i = _vdict_intern(wrap)(d, i + 1);
	}
}

// Create a dict
VDICT_LINK struct _vdict *_vdict_extern(new)(void) {
	struct _vdict *d = malloc(sizeof *d);
	d->n_entry = 0;

	d->ecap_e = 4;
	d->ent = malloc((1 << d->ecap_e) * sizeof *d->ent);
	d->mcap_e = 5;
	d->map = calloc((1 << d->mcap_e), sizeof *d->map);

	return d;
}

// Delete a dict
VDICT_LINK void _vdict_extern(free)(struct _vdict *d) {
	free(d->ent);
	free(d->map);
	free(d);
}

// Put a k/v pair, rehashing if load factor >=50%
VDICT_LINK int _vdict_extern(put)(struct _vdict *d, VDICT_KEY k, VDICT_VAL v) {
	if (2 * d->n_entry >= 1 << d->mcap_e) {
		uint32_t *map = d->map;
		d->map = calloc(1 << ++d->mcap_e, sizeof *d->map);
		if (!d->map) {
			d->map = map;
			d->mcap_e--;
			return -1;
		}

		uint32_t geti = 0, puti = 0;
		while (geti < d->n_entry) {
			struct _vdict_entry ent = d->ent[geti++];
			if (!ent.removed) {
				ent.hash = _vdict_intern(hash)(d, ent.k);
				if (puti != geti) {
					d->ent[puti] = ent;
				}
				puti++;

				uint32_t i = _vdict_intern(index)(d, ent.k, ent.hash);
				d->map[i] = puti; // Increment is before this, because indices are 1-indexed
			}
		}

		free(map);
	}

	uint32_t h = _vdict_intern(hash)(d, k);
	uint32_t i = _vdict_intern(index)(d, k, h);

	int ret;
	if (_vdict_intern(exists)(d, i)) {
		ret = 1; // Already in dict
	} else {
		ret = 0; // Added to dict
		d->map[i] = ++d->n_entry;

		// Grow entry array if needed
		if (d->n_entry >= (1 << d->ecap_e)) {
			struct _vdict_entry *ent = d->ent;
			d->ent = realloc(d->ent, (1 << ++d->ecap_e) * sizeof *d->ent);
			if (!d->ent) {
				d->ent = ent;
				d->ecap_e--;
				return -1;
			}
		}
	}

	*_vdict_intern(entry)(d, i) = (struct _vdict_entry){h, 0, k, v};
	return ret;
}

VDICT_LINK _Bool _vdict_extern(get)(struct _vdict *d, VDICT_KEY k, VDICT_VAL *v) {
	uint32_t h = _vdict_intern(hash)(d, k);
	uint32_t i = _vdict_intern(index)(d, k, h);

	if (!_vdict_intern(exists)(d, i)) return 0;

	if (v) *v = _vdict_intern(entry)(d, i)->v;
	return 1;
}

VDICT_LINK _Bool _vdict_extern(del)(struct _vdict *d, VDICT_KEY k, VDICT_VAL *v) {
	uint32_t h = _vdict_intern(hash)(d, k);
	uint32_t i = _vdict_intern(index)(d, k, h);

	if (!_vdict_intern(exists)(d, i)) return 0;

	struct _vdict_entry *ent = _vdict_intern(entry)(d, i);
	if (v) *v = ent->v;
	ent->removed = 1;

	return 1;
}

#endif

#undef VDICT_LINK
#undef VDICT_EQUAL
#undef VDICT_HASH
#undef VDICT_KEY
#undef VDICT_VAL
#undef VDICT_NAME
