/*
 * cmp.c - comparison functions used to sort the entries to be displayed.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>

#include "ls.h"

/*
 * Return the time of the entry that is relevant for -t and -l:
 * status change time (-c), access time (-u), or modification time.
 */
time_t
entry_time(const struct entry *e)
{
	switch (opt.timesel) {
	case 'c':
		return e->sb.st_ctime;
	case 'u':
		return e->sb.st_atime;
	default:
		return e->sb.st_mtime;
	}
}

static int
cmp_name(const struct entry *a, const struct entry *b)
{
	return strcmp(a->name, b->name);
}

static int
cmp_size(const struct entry *a, const struct entry *b)
{
	if (a->sb.st_size > b->sb.st_size)
		return -1;
	if (a->sb.st_size < b->sb.st_size)
		return 1;
	return cmp_name(a, b);
}

static int
cmp_time(const struct entry *a, const struct entry *b)
{
	time_t ta = entry_time(a), tb = entry_time(b);

	if (ta > tb)
		return -1;
	if (ta < tb)
		return 1;
	return cmp_name(a, b);
}

/* qsort(3) callback honoring -S, -t and -r. */
int
cmp_entries(const void *va, const void *vb)
{
	const struct entry *a = va, *b = vb;
	int r;

	switch (opt.sortby) {
	case 'S':
		r = cmp_size(a, b);
		break;
	case 't':
		r = cmp_time(a, b);
		break;
	default:
		r = cmp_name(a, b);
		break;
	}
	return opt.r ? -r : r;
}
