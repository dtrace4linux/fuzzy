#include <machine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <hash.h>
#include <dstr.h>
#include <chkalloc.h>

/*
*/

#define TRUE	1
#define	FALSE	0

//######################################################################
//   Command line switches.					      #
//######################################################################
static char *noext[] = {
	"o", "cm", "so", "gif", "png", "bmp",
	NULL
	};
int	files;
char	*f;
char	*arg_s;
char	*dir;
int	v_flag;
hash_t	*h;

# define FL_BOL	0x01
# define FL_IGNORE_CASE	0x02
void	usage(int);

typedef struct file_t {
	char	*f_name;
	char	*f_mask;
	int	f_score;
	} file_t;

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
char	*basename(char *);
char	*re_match(char *b, char *pat, int flags);
char	*re_match2(char *b, char *pat, int flags);
void display(char *f, char *mask, int val);
void usage(int ret);

/**********************************************************************/
/*   Command line switch decoder.				      */
/**********************************************************************/
int
do_switches(int argc, char **argv)
{	int	i;
	char	*cp;

	for (i = 1; i < argc; i++) {
		cp = argv[i];
		if (*cp++ != '-')
			return i;

		if (strcmp(cp, "dir") == 0) {
			if (++i >= argc)
				usage(1);
			dir = argv[i];
			continue;
		}
		if (strcmp(cp, "f") == 0) {
			if (++i >= argc)
				usage(1);
			f = argv[i];
			continue;
		}
		if (strcmp(cp, "files") == 0) {
			files = 1;
			continue;
		}

		if (strcmp(cp, "help") == 0) {
			usage(0);
		}
		if (strcmp(cp, "s") == 0) {
			if (++i >= argc)
				usage(1);
			arg_s = argv[i];
			continue;
		}
		if (strcmp(cp, "v") == 0) {
			v_flag = TRUE;
			continue;
		}
	}

	return i;
}

int
ignore_ext(char *cp)
{
	char *ext = basename(cp);
	char *ext1 = strrchr(ext, '.');
	int	i;

	if (ext1 == NULL)
		return 0;

	for (i = 0; ext1 && noext[i]; i++) {
		if (strcmp(ext1 + 1, noext[i]) == 0) {
			return 1;
		}
	}
	return 0;
}
static int
sort_compare(const void *p1, const void *p2)
{	hash_element_t *h1 = (hash_element_t *) *(void **) p1;
	hash_element_t *h2 = (hash_element_t *) *(void **) p2;

	file_t *f1 = hash_data(h1);
	file_t *f2 = hash_data(h2);


//	printf("sort %s\n", f1->f_name);

	return f2->f_score - f1->f_score;
}

int main(int argc, char **argv)
{	int	i;
	char *cp;
	char	buf[BUFSIZ];
	hash_element_t **hep;
	hash_element_t *hep1;

	int arg_index = do_switches(argc, argv);

	hash_t *files = hash_create(20, 20);

	if (dir) {
		for (cp = strtok(dir, ","); cp; cp = strtok(NULL, ",")) {
			FILE	*fp;
			char *type = files ? " -type f" : "";
			snprintf(buf, sizeof buf, "find %s %s", cp, type);
			fp = popen(buf, "r");
			while (fgets(buf, sizeof buf, fp)) {
				char	*cp;

				if (buf[strlen(buf)-1] == '\n')
					buf[strlen(buf)-1] = '\0';
				if (!hash_lookup(files, buf)) {
					file_t *f = chk_zalloc(sizeof *f);
					f->f_name = chk_strdup(buf);
					hash_insert(files, f->f_name, (void *) f);
					}
				}
			pclose(fp);
			}
		}
	else if (f) {
		FILE *fp = fopen(f, "r");
		if (!fp) {
			fprintf(stderr, "Cannot read %s - %s\n", f, strerror(errno));
			exit(1);
			}
		while (fgets(buf, sizeof buf, fp)) {
			if (buf[strlen(buf)-1] == '\n')
				buf[strlen(buf)-1] = '\0';
			if (!hash_lookup(files, buf)) {
				file_t *f = chk_zalloc(sizeof *f);
				f->f_name = chk_strdup(buf);
				hash_insert(files, f->f_name, (void *) f);
				}
			}
		}
	else if (arg_s) {
		file_t *f = chk_zalloc(sizeof *f);
		f->f_name = chk_strdup(arg_s);
		hash_insert(files, f->f_name, (void *) f);
	} else {
		char	*cp1;

		cp = getenv("PATH");
		if (cp == NULL) {
			printf("Env var $PATH is not available\n");
			exit(1);
			}
		cp = chk_strdup(cp);
		for (cp1 = strtok(cp, ":"); cp1; cp1 = strtok(NULL, ":")) {
			DIR *dh = opendir(cp1);
			struct dirent *de;

			if (dh == NULL)
				continue;

			while ((de = readdir(dh)) != NULL) {
				if (strcmp(de->d_name, ".") == 0 ||
				    strcmp(de->d_name, "..") == 0)
				    	continue;
				char *cp2 = chk_makestr(cp1, "/", de->d_name, NULL);

				if (hash_lookup(files, cp2) || ignore_ext(cp2))
					chk_free(cp2);
				else {
					file_t *f = chk_zalloc(sizeof *f);
					f->f_name = cp2;
					if (hash_insert(files, f->f_name, (void *) f) == FALSE)
						printf("hash insert %s failed\n", f->f_name);
					}
				}
			closedir(dh);
		}
		chk_free(cp);
	}


	if (arg_index >= argc) {
		usage(0);
	}

	/***********************************************/
	/*   Convert  the search pattern to a form of  */
	/*   regexp.				       */
	/***********************************************/
	char *pat = argv[arg_index++];

	dstr_t s;
	dstr_init(&s, 16);
	for (i = 0; pat[i]; i++) {
		if (i)
			dstr_add_char(&s, '*');
		dstr_add_char(&s, pat[i]);
	}
	dstr_add_char(&s, '\0');
	char *dpat = DSTR_STRING(&s);

	hash_iter_t *hi = hash_iterate_start(files, 0);
	while ((hep1 = hash_iterate_next(hi)) != NULL) {
		file_t *fitem = (file_t *) hash_data(hep1);
		char *f = fitem->f_name;
		char *b = basename(f);
		char *r;

		if (strcmp(b, pat) == 0) {
			fitem->f_score = 10;
			r = chk_strdup(b);
			memset(r, 'X', strlen(b));
		} else if (strcasecmp(b, pat) == 0) {
			fitem->f_score = 9;
			r = chk_strdup(b);
			memset(r, 'X', strlen(b));
		} else if ((r = re_match(b, pat, FL_BOL)) != NULL) {
			fitem->f_score = 8;
		} else if ((r = re_match(b, pat, FL_BOL | FL_IGNORE_CASE)) != NULL) {
			fitem->f_score = 7;
		} else if ((r = re_match(b, pat, 0)) != NULL) {
			fitem->f_score = 6;
		} else if ((r = re_match(b, pat, FL_IGNORE_CASE)) != NULL) {
			fitem->f_score = 5;
		} else if ((r = re_match(b, dpat, FL_IGNORE_CASE)) != NULL) {
			fitem->f_score = 4;
		}
//printf("done %s mask=%s\n", f, r);

//printf("=== %3d %s, %s pat=%s\n", fitem->f_score, f, r, pat);
		fitem->f_mask = r;
	}
	hash_iterate_finish(hi);

	hep = hash_linear(files, 0);
	qsort(hep, hash_size(files), sizeof *hep, sort_compare);
	for (i = 0; i < hash_size(files); i++) {
		file_t *fitem = (file_t *) hash_data(hep[i]);
//printf("name=%s mask=%p\n", fitem->f_name, fitem->f_mask);
		if (fitem->f_mask) {
			display(fitem->f_name, fitem->f_mask, fitem->f_score);
			}
		chk_free(fitem->f_name);
		chk_free(fitem->f_mask);
		chk_free(fitem);
		}
	chk_free(hep);

	hash_destroy(files, 0);
	chk_free(dpat);

}

void display(char *f, char *mask, int val)
{
	printf("\033[37m%d - ", val);
	if (v_flag) {
		printf(" mask=%s -", mask);
	}

	char *b = basename(f);
	if (strchr(f, '/')) {
		char *d = dirname(f);
		printf("%s/", d);
		chk_free(d);
	}

	for (int i = 0; i < (int) strlen(b); i++) {
		int ch = b[i];
		if (mask[i] == 'X') {
			printf("\033[36m%c\033[37m", ch);
		} else {
			printf("%c", ch);
		}
	}
	printf("\n");

	if (strlen(mask) != strlen(b)) {
		printf("error mask mismatch:\n");
		printf("  b='%s'\n", b);
		printf("  m='%s'\n", mask);
		return;
	}
}

char *
re_match(char *b, char *pat, int flags)
{
	int	i;
	int	j = 0;
	char	bbuf[BUFSIZ];
	char	pbuf[BUFSIZ];
	char	*blst[10];
	char	*plst[10];
	int	blen, plen;
	char	*cp;

	strncpy(bbuf, b, sizeof bbuf);
	strncpy(pbuf, pat, sizeof pbuf);
	for (blen = 0, cp = strtok(bbuf, "."); cp && blen < 10; cp = strtok(NULL, ".")) {
		blst[blen++] = cp;
		}
	for (plen = 0, cp = strtok(pbuf, "."); cp && plen < 10; cp = strtok(NULL, ".")) {
		plst[plen++] = cp;
		}

//printf("re_match: b=%s pat=%s\n", b, pat);

	dstr_t m;
	dstr_init(&m, 16);

	for (i = 0; i < blen; i++) {
		if (i >= plen) {
			for ( ; i < blen; i++) {
				dstr_add_char(&m, '.');
				for (j = 0; j < (int) strlen(blst[i]); j++)
					dstr_add_char(&m, '.');
			}
			break;
		}

		char *mat = re_match2(blst[i], plst[i], flags);
//#print "  cmp $b1 $p1 = $mat\n" if $b =~ /^acc/;
		if (mat == NULL) {
			dstr_free(&m);
//printf("re_match(%s, %s, %d) -> fail at %d\n", b, pat, flags, i);
			return NULL;
		}
		if (DSTR_SIZE(&m))
			dstr_add_char(&m, '.');
		dstr_add_mem(&m, mat, strlen(mat));
		chk_free(mat);
	}
	dstr_add_char(&m, '\0');

	if (i < plen) {
		dstr_free(&m);
		return NULL;
	}
//printf("re_match(%s, %s) -> \"%s\" len=%d vs %d\n", b, pat, DSTR_STRING(&m), (int) strlen(b), DSTR_SIZE(&m)-1);
	if ((int) strlen(b) != DSTR_SIZE(&m) -1) {
		printf("Size mismatch - mask does not match!\n");
		exit(1);
	}
//print "-> good\n";
	return DSTR_STRING(&m);
}
char *
re_match2(char *b, char *pat, int flags)
{	int	ch;
	dstr_t dstr;
	int	i = 0;
	int	j = 0;

	dstr_init(&dstr, 64);

	while (i < (int) strlen(b) && j < (int) strlen(pat)) {
//#print "match2: $b $pat\n";
		int b1 = b[i];
		int p1 = pat[j];

		if (flags & FL_IGNORE_CASE) {
			b1 = tolower(b1);
			p1 = tolower(p1);
		}

		if (b1 == p1) {
			dstr_add_char(&dstr, 'X');
			i++, j++;
			continue;
		}

		if (p1 == '*') {
			int found = 0;
			for (i++; i < (int) strlen(b); i++) {
				b1 = flags & FL_IGNORE_CASE ? tolower(b[i]) : b[i];
				if (b1 == p1) {
					i++;
					found = 1;
					break;
				}
				dstr_add_char(&dstr, ' ');
			}
			if (!found)
				break;
			j++;
			continue;
		}

		i++;
		dstr_add_char(&dstr, '_');
	}

	if (j < (int) strlen(pat)) {
		dstr_free(&dstr);
		return NULL;
	}

	while (i++ < (int) strlen(b))
		dstr_add_char(&dstr, '_');
	dstr_add_char(&dstr, '\0');

	char *str = DSTR_STRING(&dstr);

	if (flags & FL_BOL && str[0] != 'X') {
		dstr_free(&dstr);
		return NULL;
		}

	return str;
}
//######################################################################
//   Print out command line usage.				      #
//######################################################################
void usage(int ret)
{

	printf(
"fuzzy.pl -- fuzzy filename matching\n"
"Usage: fuzzy.pl [switches] <search-term>\n"
"\n"
"  This is a simple tool to do fuzzy filename matching. It was designed\n"
"  as a learning experience/POC, to do fuzzy matching, similar to what\n"
"  many IDEs provide. \"Cost based\" ordering gets fiddly and tricky to debug.\n"
"\n"
"  The initial approach for this was to try various algorithms for\n"
"  each item, and then order the results by \"best\". This code is designed\n"
"  to search for filenames - a different set of tests would be \n"
"  appropriate if we were doing spelling corrections. Whilst\n"
"  this is useful for filenames, it can be used for code variable\n"
"  names, since they have the same layout.\n"
"\n"
"  We need to provide list of candidate names to search from. This\n"
"  can be provided in a variety of forms:\n"
"\n"
"  * If no switches are provided, then everything in $PATH is chosen.\n"
"  * We can provide \"-f <filename>\" to give a generated or curated list\n"
"    of filenames\n"
"  * We can use \"-s <name>\" to add a single entry, for debug purposes.\n"
"\n"
"  The results are listed in best-matching order, and color highlighting\n"
"  to show the match.\n"
"\n"
"  We avoid native regular expressions - in reality, you do not\n"
"  use these to specify files, and it overly complicates things.\n"
"\n"
"  Here is an example:\n"
"\n"
"  $ fuzzy xt\n"
"\n"
"  Without any switches, this would match, for instance:\n"
"\n"
"  - /usr/bin/xterm\n"
"  - /usr/bin/xedit\n"
"  ...\n"
"\n"
"  The preference is exact case matching, at the start of the filename.\n"
"  (Directory paths are ignored/stripped). \"xterm\" matches because\n"
"  it starts with \"xt\". \"xedit\" matches because of the \"x\" and \"t\".\n"
"\n"
"  The search pattern is split on \".\" - so you could say:\n"
"\n"
"  $ fuzzy a.j\n"
"\n"
"  which might be used to match \"argumentValidator.java\". You can\n"
"  omit the extension, or use a prefix/substring to select from \n"
"  similar files but different extensions.\n"
"\n"
"Switches:\n"
"\n"
"  -dir <path)\n"
"      Scan (find $dir -print) to generate a list of filenames to match\n"
"      against.\n"
"\n"
"  -f <filename>\n"
"      Specify a list of filenames to be matched against. E.g.\n"
"\n"
"      $ find . -type f > files.lst\n"
"      $ fuzzy -f files.lst abc\n"
"\n"
"  -s <name>\n"
"      Add just <name> to the scan list - useful for debugging one-shot\n"
"      scenarios.\n"
"\n"
"  -v  Verbose - some extra debug.\n"
);
	exit(ret);
}
