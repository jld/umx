/*
 * Copyright (c) 2006,2010 Jed Davis <jld@panix.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include "salloc.h"

static size_t um_pdatmlen;
static size_t vm_roundup(size_t);
static int um_progalloc(p_t);
static struct timeval ipltime;

p_t *
um_alloc(p_t len)
{
	if (len == 0)
		len = 1;
	return salloc(len);
}

void
um_free(p_t *seg)
{
	sfree(seg);
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
	exit(1);
}

void
um_halt(void)
{
	fprintf(stderr, UM_IDENT"/"UM_CPU_IDENT": HALT after ");
	prtimes();
	exit(0);
}

void
um_newprog(p_t aid)
{
	p_t np;
	
	np = salloc_size(aid);

	whine(("far load %d platters", np));
	munmap(progdata, um_pdatmlen);
	if (0 > um_progalloc(np))
		um_abend("load map: %s", ESTR);
	memcpy(progdata, (p_t*)aid, np*4);
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

	salloc_init();
	
	for (i = 1; i < argc; ++i) {
		fd = open(argv[i], O_RDONLY);
		DIE(fd, ("IPL: open: %s: %s", argv[i], ESTR));
		DIE(fstat(fd, &sb), ("IPL: stat: %s: %s", argv[i], ESTR));
		ft = sb.st_mode & S_IFMT;
		if (ft != S_IFREG && ft != S_IFCHR)
			DIE(-1, ("IPL: bad file type: %s", argv[i]));
		ss = sb.st_size;
		es[i].e_base = mmap(0, vm_roundup(ss), PROT_READ,
		    MAP_FILE | MAP_PRIVATE, fd, 0);
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

	if (progdata == MAP_FAILED)
		progdata = mmap(0, vm_roundup(4*n), PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE, -1, 0);		
	
	return progdata == MAP_FAILED ? -1 : 0;
}
