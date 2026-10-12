/*
 * print.c - formatting and printing of the entries.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <limits.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/sysmacros.h>
#endif

#include "ls.h"

#define SIXMONTHS (180L * 24 * 60 * 60)

struct widths {
	int ino, blk, nlink, user, group, size;
};

/* Block size from BLOCKSIZE (e.g. "512", "1024", "4k", "1m"); default 512. */
static unsigned long long
block_unit(void)
{
	const char *s;
	char *end;
	unsigned long long v;

	if (opt.sizefmt == 'k')
		return 1024;
	if ((s = getenv("BLOCKSIZE")) == NULL)
		return 512;
	v = strtoull(s, &end, 10);
	if (end == s || v == 0)
		return 512;
	switch (tolower((unsigned char)*end)) {
	case 'k': v *= 1024ULL; break;
	case 'm': v *= 1024ULL * 1024; break;
	case 'g': v *= 1024ULL * 1024 * 1024; break;
	default: break;
	}
	return v;
}

/* Format a byte count in human readable form, e.g. 512B, 1.5K, 20M. */
static void
humanize(unsigned long long bytes, char *buf, size_t len)
{
	static const char units[] = "BKMGTPE";
	double v = (double)bytes;
	int u = 0;

	while (v >= 1024.0 && u < (int)sizeof(units) - 2) {
		v /= 1024.0;
		u++;
	}
	if (u == 0)
		snprintf(buf, len, "%lluB", bytes);
	else if (v < 10.0)
		snprintf(buf, len, "%.1f%c", (double)((unsigned long long)(v * 10 + 0.999)) / 10, units[u]);
	else
		snprintf(buf, len, "%llu%c", (unsigned long long)(v + 0.999), units[u]);
}

/* Format a count of 512 byte blocks according to -h, -k, BLOCKSIZE. */
static void
fmt_blocks(unsigned long long blocks, char *buf, size_t len)
{
	unsigned long long unit;

	if (opt.sizefmt == 'h') {
		humanize(blocks * 512ULL, buf, len);
		return;
	}
	unit = block_unit();
	snprintf(buf, len, "%llu", (blocks * 512ULL + unit - 1) / unit);
}

static void
fmt_size(const struct entry *e, char *buf, size_t len)
{
	mode_t m = e->sb.st_mode;

	if (S_ISCHR(m) || S_ISBLK(m))
		snprintf(buf, len, "%u, %u", (unsigned)major(e->sb.st_rdev),
		    (unsigned)minor(e->sb.st_rdev));
	else if (opt.sizefmt == 'h')
		humanize((unsigned long long)e->sb.st_size, buf, len);
	else
		snprintf(buf, len, "%llu", (unsigned long long)e->sb.st_size);
}

/* Build a 10 character mode string like strmode(3). */
static void
mode_string(mode_t m, char *s)
{
	strcpy(s, "----------");
	if (S_ISDIR(m)) s[0] = 'd';
	else if (S_ISLNK(m)) s[0] = 'l';
	else if (S_ISCHR(m)) s[0] = 'c';
	else if (S_ISBLK(m)) s[0] = 'b';
	else if (S_ISFIFO(m)) s[0] = 'p';
	else if (S_ISSOCK(m)) s[0] = 's';
#ifdef S_ISWHT
	else if (S_ISWHT(m)) s[0] = 'w';
#endif
	if (m & S_IRUSR) s[1] = 'r';
	if (m & S_IWUSR) s[2] = 'w';
	if (m & S_IXUSR) s[3] = 'x';
	if (m & S_IRGRP) s[4] = 'r';
	if (m & S_IWGRP) s[5] = 'w';
	if (m & S_IXGRP) s[6] = 'x';
	if (m & S_IROTH) s[7] = 'r';
	if (m & S_IWOTH) s[8] = 'w';
	if (m & S_IXOTH) s[9] = 'x';
	if (m & S_ISUID) s[3] = (m & S_IXUSR) ? 's' : 'S';
	if (m & S_ISGID) s[6] = (m & S_IXGRP) ? 's' : 'S';
	if (m & S_ISVTX) s[9] = (m & S_IXOTH) ? 't' : 'T';
}

static void
owner_name(uid_t uid, char *buf, size_t len)
{
	struct passwd *pw = opt.n ? NULL : getpwuid(uid);

	if (pw != NULL)
		snprintf(buf, len, "%s", pw->pw_name);
	else
		snprintf(buf, len, "%u", (unsigned)uid);
}

static void
group_name(gid_t gid, char *buf, size_t len)
{
	struct group *gr = opt.n ? NULL : getgrgid(gid);

	if (gr != NULL)
		snprintf(buf, len, "%s", gr->gr_name);
	else
		snprintf(buf, len, "%u", (unsigned)gid);
}

/* Character appended by -F, or 0 for none. */
static int
indicator(mode_t m)
{
	if (S_ISDIR(m)) return '/';
	if (S_ISLNK(m)) return '@';
	if (S_ISSOCK(m)) return '=';
	if (S_ISFIFO(m)) return '|';
#ifdef S_ISWHT
	if (S_ISWHT(m)) return '%';
#endif
	if (m & (S_IXUSR | S_IXGRP | S_IXOTH)) return '*';
	return 0;
}

/* Print a file name, replacing non-printable characters if -q is set. */
static void
print_name(const char *name)
{
	const unsigned char *p;

	for (p = (const unsigned char *)name; *p != '\0'; p++)
		putchar((opt.q && !isprint(*p)) ? '?' : *p);
}

static void
print_time(time_t t)
{
	char buf[64];
	struct tm *tm;
	time_t now = time(NULL);

	if ((tm = localtime(&t)) == NULL) {
		printf("?            ");
		return;
	}
	if (t < now - SIXMONTHS || t > now)
		strftime(buf, sizeof(buf), "%b %e  %Y", tm);
	else
		strftime(buf, sizeof(buf), "%b %e %H:%M", tm);
	printf("%s", buf);
}

static int
numlen(unsigned long long v)
{
	char buf[32];

	return snprintf(buf, sizeof(buf), "%llu", v);
}

static void
max_width(int *w, size_t len)
{
	if ((int)len > *w)
		*w = (int)len;
}

static void
compute_widths(struct entry *e, size_t n, struct widths *w)
{
	char buf[128];
	size_t i;

	memset(w, 0, sizeof(*w));
	for (i = 0; i < n; i++) {
		if (opt.i)
			max_width(&w->ino, numlen(e[i].sb.st_ino));
		if (opt.s) {
			fmt_blocks(e[i].sb.st_blocks, buf, sizeof(buf));
			max_width(&w->blk, strlen(buf));
		}
		if (!opt.l)
			continue;
		max_width(&w->nlink, numlen(e[i].sb.st_nlink));
		owner_name(e[i].sb.st_uid, buf, sizeof(buf));
		max_width(&w->user, strlen(buf));
		group_name(e[i].sb.st_gid, buf, sizeof(buf));
		max_width(&w->group, strlen(buf));
		fmt_size(&e[i], buf, sizeof(buf));
		max_width(&w->size, strlen(buf));
	}
}

static void
print_one(const struct entry *e, const struct widths *w)
{
	char buf[PATH_MAX];
	char mode[16];
	int ind;
	ssize_t len;

	if (opt.i)
		printf("%*llu ", w->ino, (unsigned long long)e->sb.st_ino);
	if (opt.s) {
		fmt_blocks(e->sb.st_blocks, buf, sizeof(buf));
		printf("%*s ", w->blk, buf);
	}
	if (opt.l) {
		mode_string(e->sb.st_mode, mode);
		printf("%s %*llu ", mode, w->nlink,
		    (unsigned long long)e->sb.st_nlink);
		owner_name(e->sb.st_uid, buf, sizeof(buf));
		printf("%-*s  ", w->user, buf);
		group_name(e->sb.st_gid, buf, sizeof(buf));
		printf("%-*s  ", w->group, buf);
		fmt_size(e, buf, sizeof(buf));
		printf("%*s ", w->size, buf);
		print_time(entry_time(e));
		putchar(' ');
	}
	print_name(e->name);
	if (opt.F && (ind = indicator(e->sb.st_mode)) != 0)
		putchar(ind);
	if (opt.l && S_ISLNK(e->sb.st_mode)) {
		if ((len = readlink(e->path, buf, sizeof(buf) - 1)) < 0) {
			warn("readlink: %s", e->path);
			exit_status = 1;
		} else {
			buf[len] = '\0';
			printf(" -> ");
			print_name(buf);
		}
	}
	putchar('\n');
}

/*
 * Print n entries, one per line.  If show_total is set, a "total" line
 * with the number of blocks used is printed first.
 */
void
print_entries(struct entry *e, size_t n, int show_total)
{
	struct widths w;
	unsigned long long total = 0;
	char buf[64];
	size_t i;

	compute_widths(e, n, &w);
	if (show_total) {
		for (i = 0; i < n; i++)
			total += (unsigned long long)e[i].sb.st_blocks;
		fmt_blocks(total, buf, sizeof(buf));
		printf("total %s\n", buf);
	}
	for (i = 0; i < n; i++)
		print_one(&e[i], &w);
}
