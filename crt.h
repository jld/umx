/*
 * Copyright (c) 2006,2007 Jed Davis <jld@panix.com>
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

void* um_postwrite(void **rtnp, p_t target, p_t source, znz_t znz);
void* um_enter(p_t x, znz_t znz);
void* um_loadfar(void **rtnp, p_t seg, p_t idx, znz_t znz);
void* um_enterdep(p_t x, struct block *src, znz_t znz);

bitset_t prognowr, prognoex;

struct cod
{
	char *base, *next, *end;
	struct cod* cdr;
};
void newcod(struct cod *c);
void getcod(struct cod *c, int n);
void nukecod(struct cod *c);

void um_crti(void);
void um_crtf(void);
void um_destroy_world(void);
void *um_destroy_and_go(void **rtnp, p_t x, znz_t znz);

void umc_codlink(struct cod *from, char *to);		/* MD */
void* umc_enter(p_t x, znz_t znz);
int umc_start(void);					/* MD */
void umc_init(void);
void umc_fini(void);

#endif
