#ifndef LS_H
#define LS_H

#include <sys/types.h>
#include <sys/stat.h>

#include <stddef.h>

/* Command line options (see ls(1) manual page). */
struct options {
	int A;		/* all except . and .. */
	int a;		/* all entries */
	int d;		/* directories as plain files */
	int F;		/* append file type indicator */
	int f;		/* unsorted */
	int i;		/* print inode number */
	int l;		/* long format */
	int n;		/* numeric uid/gid (implies long format) */
	int q;		/* non-printables shown as '?' */
	int R;		/* recursive */
	int r;		/* reverse sort */
	int s;		/* print block counts */
	int sizefmt;	/* 'h', 'k' or 0 (rightmost of -h/-k wins) */
	int sortby;	/* 'S', 't' or 0 (name) */
	int timesel;	/* 'c', 'u' or 0 (mtime) */
	int tty;	/* standard output is a terminal */
};

/* One file system entry to be displayed. */
struct entry {
	char *name;		/* name to display */
	char *path;		/* path used for stat(2)/readlink(2) */
	struct stat sb;
};

extern struct options opt;
extern int exit_status;

/* cmp.c */
int cmp_entries(const void *, const void *);
time_t entry_time(const struct entry *);

/* print.c */
void print_entries(struct entry *, size_t, int);

#endif /* LS_H */
