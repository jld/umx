#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <arpa/inet.h>
#include "stuff.h"

#ifdef UM_MPOOL
#include "mpool.h"
MPOOL_DECL(segpool4, 2);
MPOOL_DECL(segpool8, 3);
MPOOL_DECL(segpool16, 4);
MPOOL_DECL(segpool32, 5);
#endif

static size_t um_pdatmlen;
static size_t vm_roundup(size_t);
static int um_progalloc(p_t);
static struct timeval ipltime;

p_t *
um_alloc3(p_t len)
{
	p_t *rv;
	
	MPOOL_ALLOC(rv, segpool4);
	rv[0] = len;
	return rv +1;
}

p_t *
um_alloc7(p_t len)
{
	p_t *rv;
	
	MPOOL_ALLOC(rv, segpool8);
	rv[0] = len;
	return rv +1;
}

p_t *
um_alloc(p_t len)
{
	p_t *rv;

#ifndef UM_MPOOL
	rv = malloc(4 * (len + 1));
#else
	if (len <= 3) 
		return um_alloc3(len);
	else if (len <= 7)
		return um_alloc7(len);
	else if (len <= 15)
		MPOOL_ALLOC(rv, segpool16);
	else if (len <= 31)
		MPOOL_ALLOC(rv, segpool32);
	else
		rv = calloc(4, len + 1); 
#endif
	if (!rv) 
		um_abend("the platter supply is finite (rC=%d)", len);

#ifndef UM_MPOOL
	if (len == 3) 
		rv[1] = rv[2] = rv[3] = 0;
	else
		memset(rv + 1, 0, 4 * len);
#endif

	*rv = len;
	return rv + 1;
}

void
um_free(p_t *seg)
{
#ifdef UM_MPOOL
	if (seg[-1] <= 31)
		mpool_xfree(seg-1);
	else
#endif
		free(seg - 1);
}

static void
prtimes(void)
{
	struct timeval wall;
	struct rusage ru;

	getrusage(RUSAGE_SELF, &ru);
	gettimeofday(&wall, 0);
	wall.tv_sec -= ipltime.tv_sec;
	wall.tv_usec -= ipltime.tv_usec;
	if (wall.tv_usec < 0) {
		--wall.tv_sec;
		wall.tv_usec += 1000000L;
	}
	
#define SU(tv) ((tv).tv_sec), (long)((tv).tv_usec)
	fprintf(stderr, "%ld.%06lds user, %ld.%06lds sys, %ld.%06lds wall;"
	    " %llu+%llu bytes compiled.\n",
	    SU(ru.ru_utime), SU(ru.ru_stime), SU(wall), 
	    nbcompiled[0], nbcompiled[1]);
#undef SU
}

void
um_abend(const char* fmt, ...)
{
	va_list v;

	va_start(v, fmt);
	fprintf(stderr, UM_IDENT"/"UM_CPU_IDENT": ABEND: ");
	vfprintf(stderr, fmt, v);
	fprintf(stderr, "\n\tafter ");
	va_end(v);
	prtimes();
	// cpu_progfini(UM_PF_ABEND);
	exit(1);
}

void
um_halt(void)
{
	fprintf(stderr, UM_IDENT"/"UM_CPU_IDENT": HALT after ");
	prtimes();
	// cpu_progfini(UM_PF_HALT);
	exit(0);
}

void
um_newprog(p_t aid)
{
	p_t np = ((p_t*)aid)[-1];

	whine(("far load %d platters", np));
	// cpu_progfini(UM_PF_LOAD);
	munmap(progdata, um_pdatmlen);
	if (0 > um_progalloc(np))
		um_abend("load map: %s", ESTR);
	memcpy(progdata, (p_t*)aid, np*4);
	// cpu_proginit();
}

struct extent {
	p_t* e_base;
	size_t e_bound;
};

#define DIE(ae, l) do { if ((ae) < 0) { yowl l; exit(2); } } while(0)

void
um_ipl(int argc, char** argv)
{
	struct extent *es = malloc(argc * sizeof(struct extent));
	long long stuff = 0, ss;
	int i, fd;
	struct stat sb;
	mode_t ft;

	gettimeofday(&ipltime, 0);
	memset(&nbcompiled, 0, sizeof(nbcompiled));
	setvbuf(stdout, NULL, _IOLBF, 0);
	
#ifdef UM_MPOOL
	MPOOL_INIT(segpool4);
	MPOOL_INIT(segpool8);
	MPOOL_INIT(segpool16);
	MPOOL_INIT(segpool32);
#endif
	
	for (i = 1; i < argc; ++i) {
		fd = open(argv[i], O_RDONLY);
		DIE(fd, ("IPL: open: %s: %s", argv[i], ESTR));
		DIE(fstat(fd, &sb), ("IPL: stat: %s: %s", argv[i], ESTR));
		ft = sb.st_mode & S_IFMT;
		if (ft != S_IFREG && ft != S_IFCHR)
			DIE(-1, ("IPL: bad file type: %s", argv[i]));
		ss = sb.st_size;
		es[i].e_base = mmap(0, vm_roundup(ss), PROT_READ,
		    MAP_FILE | MAP_SHARED, fd, 0);
		if (es[i].e_base == MAP_FAILED)
			DIE(-1, ("IPL: mmap: %s: %s", argv[i], ESTR));
		stuff += es[i].e_bound = ss/4;
		close(fd);
	}

	if (stuff == 0)
		DIE(-1, ("IPL: empty program"));
	if (stuff > 0x100000000LL)
		DIE(-1, ("IPL: program too long (%lld)", stuff));
	DIE(um_progalloc(stuff), ("IPL: mmap: %s", ESTR));
	stuff = 0;
	for (i = 1; i < argc; ++i) {
		for(ss = 0; ss < es[i].e_bound; ++ss) {
			progdata[stuff+ss] = ntohl(es[i].e_base[ss]);
		}
		stuff += ss;
		munmap(es[i].e_base, vm_roundup(4*es[i].e_bound));
	}
	free(es);
	
	fprintf(stderr, UM_IDENT"/"UM_CPU_IDENT" %s booting...\n", um_vers);
	// cpu_proginit();
}


static size_t
vm_roundup(size_t n)
{
	static size_t pagesize = 0;

	if (!pagesize)
		pagesize = (size_t)sysconf(_SC_PAGESIZE);
	return (n + pagesize - 1) & ~(pagesize - 1);
}


static int
um_progalloc(p_t n)
{
	proglen = n;
	progdata = mmap(0, vm_roundup(4*n), PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
	
	return progdata == MAP_FAILED ? -1 : 0;
}
