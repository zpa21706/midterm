/*
 * ls.c - a subset of ls(1): option parsing and directory traversal.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ls.h"

struct options opt;
int exit_status = 0;

static int printed;	/* something was printed already (for blank lines) */

struct list {
	struct entry *v;
	size_t n, cap;
};

static void
usage(void)
{
	fprintf(stderr, "usage: ls [-AacdFfhiklnqRrSstuw] [file ...]\n");
	exit(2);
}

static void *
xrealloc(void *p, size_t sz)
{
	if ((p = realloc(p, sz)) == NULL)
		err(1, "realloc");
	return p;
}

static char *
xstrdup(const char *s)
{
	char *r;

	if ((r = strdup(s)) == NULL)
		err(1, "strdup");
	return r;
}

/* Append a new entry to the list; the stat data is filled in by the caller. */
static struct entry *
list_add(struct list *l, const char *name, const char *path)
{
	struct entry *e;

	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 16;
		l->v = xrealloc(l->v, l->cap * sizeof(*l->v));
	}
	e = &l->v[l->n++];
	e->name = xstrdup(name);
	e->path = xstrdup(path);
	return e;
}

static void
list_free(struct list *l)
{
	size_t i;

	for (i = 0; i < l->n; i++) {
		free(l->v[i].name);
		free(l->v[i].path);
	}
	free(l->v);
	l->v = NULL;
	l->n = l->cap = 0;
}

static void
sort_list(struct list *l)
{
	if (!opt.f && l->n > 1)
		qsort(l->v, l->n, sizeof(*l->v), cmp_entries);
}

/* Join dir and name into a newly allocated path. */
static char *
join_path(const char *dir, const char *name)
{
	size_t len = strlen(dir) + strlen(name) + 2;
	char *p = xrealloc(NULL, len);

	if (dir[0] != '\0' && dir[strlen(dir) - 1] == '/')
		snprintf(p, len, "%s%s", dir, name);
	else
		snprintf(p, len, "%s/%s", dir, name);
	return p;
}

static int
is_dot_or_dotdot(const char *name)
{
	return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

/* Should the directory entry called name be listed? */
static int
wanted(const char *name)
{
	if (opt.a)
		return 1;
	if (opt.A)
		return !is_dot_or_dotdot(name);
	return name[0] != '.';
}

/* List the contents of the directory path (recursively if -R). */
static void
list_dir(const char *path, int header)
{
	DIR *dp;
	struct dirent *de;
	struct list l = { NULL, 0, 0 };
	struct entry *e;
	char *full;
	size_t i;

	if (header) {
		if (printed)
			putchar('\n');
		printf("%s:\n", path);
	}
	printed = 1;

	if ((dp = opendir(path)) == NULL) {
		warn("%s", path);
		exit_status = 1;
		return;
	}
	while ((de = readdir(dp)) != NULL) {
		if (!wanted(de->d_name))
			continue;
		full = join_path(path, de->d_name);
		e = list_add(&l, de->d_name, full);
		free(full);
		if (lstat(e->path, &e->sb) == -1) {
			warn("%s", e->path);
			exit_status = 1;
			free(e->name);
			free(e->path);
			l.n--;
		}
	}
	closedir(dp);

	sort_list(&l);
	if (l.n > 0 || opt.l || opt.s)
		print_entries(l.v, l.n, opt.l || (opt.s && opt.tty));

	if (opt.R) {
		for (i = 0; i < l.n; i++) {
			if (!S_ISDIR(l.v[i].sb.st_mode) ||
			    is_dot_or_dotdot(l.v[i].name))
				continue;
			list_dir(l.v[i].path, 1);
		}
	}
	list_free(&l);
}

/* Parse BLOCKSIZE-independent flags; later flags override earlier ones. */
static void
parse_options(int argc, char **argv)
{
	int ch;

	opt.tty = isatty(STDOUT_FILENO);
	opt.q = opt.tty;	/* default for terminals; -w turns it off */
	while ((ch = getopt(argc, argv, "AacdFfhiklnqRrSstuw")) != -1) {
		switch (ch) {
		case 'A': opt.A = 1; break;
		case 'a': opt.a = 1; break;
		case 'c': opt.timesel = 'c'; break;
		case 'd': opt.d = 1; break;
		case 'F': opt.F = 1; break;
		case 'f': opt.f = 1; break;
		case 'h': opt.sizefmt = 'h'; break;
		case 'i': opt.i = 1; break;
		case 'k': opt.sizefmt = 'k'; break;
		case 'l': opt.l = 1; opt.n = 0; break;
		case 'n': opt.l = 1; opt.n = 1; break;
		case 'q': opt.q = 1; break;
		case 'R': opt.R = 1; break;
		case 'r': opt.r = 1; break;
		case 'S': opt.sortby = 'S'; break;
		case 's': opt.s = 1; break;
		case 't': opt.sortby = 't'; break;
		case 'u': opt.timesel = 'u'; break;
		case 'w': opt.q = 0; break;
		default:
			usage();
		}
	}
	if (geteuid() == 0)
		opt.A = 1;
}

int
main(int argc, char **argv)
{
	struct list files = { NULL, 0, 0 }, dirs = { NULL, 0, 0 };
	struct entry *e;
	struct stat sb;
	char *dot[] = { ".", NULL };
	char **ops;
	int nops, i;

	parse_options(argc, argv);
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		ops = dot;
		nops = 1;
	} else {
		ops = argv;
		nops = argc;
	}

	for (i = 0; i < nops; i++) {
		if (lstat(ops[i], &sb) == -1) {
			warn("%s", ops[i]);
			exit_status = 1;
			continue;
		}
		/* Follow a symlink operand unless -d; keep the link if dangling. */
		if (S_ISLNK(sb.st_mode) && !opt.d) {
			struct stat target;

			if (stat(ops[i], &target) == 0 && S_ISDIR(target.st_mode))
				sb = target;
		}
		if (S_ISDIR(sb.st_mode) && !opt.d)
			e = list_add(&dirs, ops[i], ops[i]);
		else
			e = list_add(&files, ops[i], ops[i]);
		e->sb = sb;
	}

	sort_list(&files);
	sort_list(&dirs);

	if (files.n > 0) {
		print_entries(files.v, files.n, 0);
		printed = 1;
	}
	for (i = 0; i < (int)dirs.n; i++)
		list_dir(dirs.v[i].path, nops > 1 || opt.R);

	list_free(&files);
	list_free(&dirs);
	return exit_status;
}
