#include <sys/types.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdlib.h>
#include "stuff.h"
#include "crt.h"

#define BLKFL_DEP 1
#define UM_NLIST 1024
#define PBHASH(x) ((x) % UM_NLIST)

static struct block **progblks;
#define PBHEAD(x) (progblks[PBHASH(x)])

static /*inline*/ void remblk(struct block *blk)
{ *blk->prev = blk->next;
  if (blk->next) blk->next->prev = blk->prev;
}
static inline void insblk(struct block *blk, struct block **head)
{ if (*head)
	(**head).prev = &blk->next;
  blk->next = *head; 
  blk->prev = head;
  *head = blk;
}


void
useblk(struct block* blk)
{
	struct block** head;
	p_t i;

	blk->flags = 0;
	head = &PBHEAD(blk->begin);
	insblk(blk, head);
	
	for (i = blk->begin; i < blk->end; ++i)
		bset(prognowr, i);
}

void
um_destroy_world(void)
{
	whine(("total destruction"));
	um_crtf();
	um_crti();
}

void
delblk(struct block *blk)
{
	p_t i;

	if (blk->flags & BLKFL_DEP) {
		um_destroy_world();
		return;
	}
	
	remblk(blk);
	for (i = blk->begin; i < blk->end; ++i)
		if (!getblk(i))
			bclr(prognowr, i);

	free(blk); /* XXX */
}

struct block*
getblk(p_t x)
{
	p_t i, l;
	struct block *blk;

	l = x > UM_MAXBLK ? x - UM_MAXBLK : 0;

	for (i = l; i <= x; ++i) {
		blk = getblkx(i, 65535);
		if (inblk(blk, x))
			return blk;
	}
	return NULL;
}

struct block*
getblkx(p_t x, znz_t znz)
{
	struct block **head, *p;

	head = &PBHEAD(x);
	p = *head;
	if (um_unlike(!p) ||
	    (um_likely(p->begin == x) && um_likely(znblk(p, znz))))
		return p;
	for (p = p->next; p; p = p->next)
		if (p->begin == x && znblk(p, znz)) {
			remblk(p);
			insblk(p, head);
			return p;
		}
	return NULL;
}

void
depblk(struct block *src, struct block* dst)
{
	src = src;
	dst->flags |= BLKFL_DEP;
}

void*
um_postwrite(p_t target, p_t source, znz_t znz)
{
	struct block *tblk;
	int yow, ayow = 0;

	if (um_likely(!btst(prognowr, target)))
		return 0;

	while ((tblk = getblk(target))) {
		yow = inblk(tblk, source);
		ayow = ayow || yow;
		whine(("postwrite: shootdown %u -> %u%s",
			  source, target, yow?" (YOW!)":""));
		delblk(tblk);
	}
	assert(!btst(prognowr, target));

	if (!btst(prognowr, source))
		return umc_enter(source + 1, znz);
	else
		return 0;
}

void*
um_enter(p_t x, znz_t znz)
{
	struct block *blk;

	blk = getblkx(x, znz);
	if (blk)
		return blk->jmp;
	else 
		return umc_enter(x, znz);
}

void*
um_loadfar(p_t seg, p_t idx, znz_t znz)
{
	um_crtf();
	um_newprog(seg);
	um_crti();
	return um_enter(idx, znz);
}

void*
um_enterdep(p_t x, struct block *src, znz_t znz)
{
	struct block *blk;

	blk = getblkx(x, znz);
	if (blk) {
		depblk(src, blk);
		return blk->jmp;
	} else 
		return umc_enter(x, znz);
}


#define COD_SEG 1048576

void
newcod(struct cod *c)
{
	void *a;

	a = mmap(0, COD_SEG, PROT_READ | PROT_WRITE | PROT_EXEC, 
	    MAP_ANON | MAP_PRIVATE, -1, 0);
	if (a == MAP_FAILED)
		panic("newcod: mmap: %s", ESTR);

	c->cdr = 0;
	c->base = c->next = a;
	c->end = c->base + COD_SEG;
}

void
getcod(struct cod *c, int n)
{
	struct cod *xc;

	if (c->end - c->next >= n)
		return;
	
	xc = malloc(sizeof(struct cod));
	memcpy(xc, c, sizeof(struct cod));
	newcod(c);
	c->cdr = xc;
	umc_codlink(xc,c->next);
}


void
um_crti(void)
{
	assert(!progblks);
	progblks = calloc(UM_NLIST, sizeof(struct block *));
	
	assert(!prognowr);
	prognowr = calloc((proglen + 31)/32, 4);
	assert(!prognoex);
	prognoex = calloc((proglen + 31)/32, 4);
}

void
um_crtf(void)
{
	p_t i;
	struct block *b, *nb;

	assert(progblks);
	for (i = 0; i < UM_NLIST; ++i)
		for (b = progblks[i]; b; b = nb) {
			nb = b -> next;
			free(b);
		}
	free(progblks);
	progblks = 0;

	assert(prognowr);
	free(prognowr);
	prognowr = 0;
	assert(prognoex);
	free(prognoex);
	prognoex = 0;
}
