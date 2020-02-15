#include <git2/revwalk.h>
#include <git2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

static char *argv0;
#include "arg.h"

#define LEN(X) (sizeof(X) / sizeof((X)[0]))

static int ***stats;
static char *email;
static struct tm minday, maxday;
static time_t mintime, maxtime;
static int force = 0;

static void
die(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void gdie(const char *s) {
	die("%s: %s\n", s, git_error_last()->message);
}

static void
usage()
{
	die(
	"usage: %s [-f] [-w width] [-e email] [-W first day of week]\n"
	"[-s symbols] [-p placeholder] repo [repo ...]\n",
	argv0);
}

static const git_oid *
get_default_head(git_repository *repo)
{
	int found = 0;
	git_reference *branch = NULL;
	git_branch_iterator *branches;
	git_branch_iterator_new(&branches, repo, GIT_BRANCH_REMOTE);
	for (;;) {
		git_branch_t dumb;
		int rc = git_branch_next(&branch, &dumb, branches);
		if (rc == GIT_ITEROVER)
			break;

		rc = git_branch_is_head(branch);
		if (rc < 0)
			gdie("branch is head");
		if (rc) {
			found = 1;
			break;
		}
	}
	git_branch_iterator_free(branches);

	if (!found) {
		if (0 != git_branch_lookup(&branch, repo, "master", GIT_BRANCH_LOCAL)) {
			if (force) return NULL;
			gdie("branch lookup");
		}
	}

	git_reference *commit;
	git_reference_resolve(&commit, branch);

	return git_reference_target(commit);
}

static void
count_commit(git_commit *commit)
{
	if (email && 0 != strcmp(email, git_commit_author(commit)->email))
		return;

	time_t t = git_commit_author(commit)->when.time;
	if (t < mintime || t > maxtime)
		return;

	struct tm *d = gmtime(&t);
	stats[maxday.tm_year - d->tm_year][d->tm_mon][d->tm_mday]++;
}

static void
count_repo(const char *path)
{
	git_repository *repo = NULL;
	if (0 != git_repository_open(&repo, path)) {
		if (force) return;
		gdie("repository open");
	}

	const git_oid *branch = get_default_head(repo);
	if (branch == NULL) {
		if (force) return;
		die("failed to find default branch");
	}

	git_revwalk *walk = NULL;
	if (0 != git_revwalk_new(&walk, repo))
		gdie("revwalk new");

	git_revwalk_push(walk, branch);

	for (;;) {
		git_oid id;
		int rc = git_revwalk_next(&id, walk);
		if (rc == GIT_ITEROVER)
			break;
		if (rc != 0)
			gdie("revwalk next");

		git_commit *commit;
		if (0 != git_commit_lookup(&commit, repo, &id))
			gdie("commit lookup");

		count_commit(commit);

		git_commit_free(commit);
	}

	git_revwalk_free(walk);
	git_repository_free(repo);
}

static int
utf8len(const unsigned char *s)
{
	unsigned int c = *s;
	if (c == 0)    return 0;
	if (c >= 0xf0) return 4;
	if (c >= 0xe0) return 3;
	if (c >= 0xc0) return 2;
	return 1;
}

int
main(int argc, char *argv[])
{
	int width = 80;
	int first_day_of_week = 1;
	char *symbols = "░▒▓█";
	char *placeholder = " ";

	ARGBEGIN {
	case 'f':
		force = 1;
		break;
	case 'e':
		email = EARGF(usage());
		break;
	case 'w':
		width = atoi(EARGF(usage()));
		break;
	case 'W':
		first_day_of_week = atoi(EARGF(usage()));
		break;
	case 's':
		symbols = EARGF(usage());
		break;
	case 'p':
		placeholder = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND;

	if (argc == 0)
		usage();

	time_t t = time(NULL);
	gmtime_r(&t, &maxday);
	maxday.tm_mday += 6 - (maxday.tm_wday + 7 - first_day_of_week) % 7;
	maxday.tm_hour = 23;
	maxday.tm_min = 59;
	maxday.tm_sec = 59;
	maxtime = mktime(&maxday);

	minday = maxday;
	minday.tm_mday -= 7 * width + 1;
	mintime = mktime(&minday);

	int years = (maxday.tm_year - minday.tm_year + 1);
	stats = malloc(sizeof(int*) * years);
	for (int y = 0; y < years; y++) {
		stats[y] = malloc(sizeof(int*)*12);
		for (int m = 0; m < 12; m++) {
			stats[y][m] = calloc(1, sizeof(int)*32);
		}
	}

	git_libgit2_init();
	for (int i = 0; i < argc; i++)
		count_repo(argv[i]);

	int l, formats_len = 0;
	unsigned char *s = (unsigned char *)symbols;
	while ((l = utf8len(s))) {
		s += l;
		formats_len++;
	}

	char formats[formats_len][5];
	s = (unsigned char *)symbols;
	for (int i = 0; i < formats_len; i++) {
		int l = utf8len(s);
		memset(formats[i], 0, sizeof(formats[0]));
		memcpy(formats[i], s, l);
		s += l;
	}

	int max = 0;
	for (int w = 0; w < 7; w++) {
		for (int i = 0; i < width; i++) {
			struct tm d = minday;
			d.tm_mday += w + i * 7;
			mktime(&d);
			int v = stats[maxday.tm_year - d.tm_year][d.tm_mon][d.tm_mday];
			if (v > max)
				max = v;
		}
	}

	for (int w = 0; w < 7; w++) {
		for (int i = 0; i < width; i++) {
			struct tm d = minday;
			d.tm_mday += w + i * 7;
			mktime(&d);
			int v = stats[maxday.tm_year - d.tm_year][d.tm_mon][d.tm_mday];

			if (v == 0) {
				printf("%s", placeholder);
			} else {
				float ratio = (float)v / (float)max;
				printf("%s", formats[(int)(ratio * (formats_len - 1))]);
			}
		}
		puts("");
	}
}