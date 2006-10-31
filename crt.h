#ifndef ___UMX__CRT_H
#define ___UMX__CRT_H
#include "stuff.h"
#include <sys/types.h>

#define UM_MAXBLK 256

typedef unsigned *bitset_t;

static inline unsigned btst(bitset_t b, unsigned idx)
{ return b[idx>>5]&(1U<<(idx&31)); }
static inline void bset(bitset_t b, unsigned idx)
{ b[idx>>5]|=(1U<<(idx&31)); }
static inline void bclr(bitset_t b, unsigned idx)
{ b[idx>>5]&=~(1U<<(idx&31)); }

typedef uint16_t znz_t;

struct block
{
	p_t begin, end;
	void *jmp;
	struct block *next, **prev;
	znz_t znz;
	uint16_t flags;
};

void useblk(struct block *blk);
void delblk(struct block *blk);
struct block *getblk(p_t x);
struct block *getblkx(p_t x, znz_t znz);
void depblk(struct block *src, struct block* dst);

static inline int inblk(const struct block *blk, p_t x)
{ return blk->begin <= x && x < blk->end; }
static inline int znblk(const struct block *blk, znz_t znz)
{ return (blk->znz & znz) == blk->znz; } /* blk->znz \subset znz */

void* um_postwrite(p_t target, p_t source, znz_t znz);
void* um_enter(p_t x, znz_t znz);
void* um_loadfar(p_t seg, p_t idx, znz_t znz);
void* um_enterdep(p_t x, struct block *src, znz_t znz);

bitset_t prognowr, prognoex;

struct cod
{
	char *base, *next, *end;
	struct cod* cdr;
};
void newcod(struct cod *c);
void getcod(struct cod *c, int n);

void um_crti(void);
void um_crtf(void);
void um_destroy_world(void);

void umc_codlink(struct cod *from, char *to);
void* umc_enter(p_t x, znz_t znz);
int umc_start(void);

#endif
