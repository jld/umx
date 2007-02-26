#include <stdlib.h>
#include "co.h"
#include "ra.h"

#define here (g.c->next)

static struct block*
umc_mkblk(p_t x, znz_t znz)
{
	int done = 0;
	p_t i, o, a, b, c, limit;
	struct block *blk = malloc(sizeof(struct block));
	void *jmpto = 0;

	limit = x + UM_MAXBLK;

	blk->begin = x;
	blk->jmp = here;
	blk->znz = znz;
	
	g.curblk = blk;
	g.znz = znz;
	g.mset = 255;
	g.rset = 0;
	g.cset = znz & 255;
	memset(&g.con, 0, sizeof(g.con));
	memset(&g.mlru, 0, sizeof(g.mlru));
	memset(&g.mtov, -1, sizeof(g.mtov));
	memset(&g.vtom, 170, sizeof(g.vtom));
	g.c = &g.inl;
	for (g.time = x; !done && g.time < limit; ++g.time) {
		char *then, *othen;
		if (g.time >= proglen) {
			co_badness(); done = 1; break;
		}
		if (btst(prognoex, g.time)) {
			co_fltnoex(); done = 1;	break;
		}

		getcod(&g.inl, 1024);
		getcod(&g.outl, 1024);
		then = here;
		othen = g.outl.next;
		i = progdata[g.time];
		o = INSN_OP(i);
		a = INSN_A(i);
		b = INSN_B(i);
		c = INSN_C(i);
		bset(prognowr, g.time);
		
		switch(o) {
		case 0: 
			if (g.time + 1 < limit) {
				p_t ii, oo, bb, cc;
				ii = progdata[g.time+1];
				oo = INSN_OP(ii);
				bb = INSN_B(ii);
				cc = INSN_C(ii);
				if (oo == 12 && a == cc && 
				    a != bb && a != b && a != c
				    && ISC(a) && ISC(b)) {
					p_t ct = g.con[b], cf = g.con[a];
					co_condbr(bb, c, a, ct, cf);
					++g.time; done = 1; break;
				}
			}
			co_cmov(a, b, c);
			break;
		case 1: co_index(a, b, c); break;
		case 2: co_amend(a, b, c); break;
		case 3: co_add(a, b, c); break;
		case 4: co_mul(a, b, c); break;
		case 5: co_div(a, b, c); break;
		case 6: co_nand(a, b, c); break;
		case 7: co_halt(); done = 1; break;
		case 8: co_alloc(b, c); break;
		case 9: co_free(c); break;
		case 10: co_output(c); break;
		case 11: co_input(c); break;
		case 12: co_load(b,c); done = 1; break;
		case 13: co_ortho(INSN_IR(i), INSN_IM(i)); break;
		default: co_badness(); done = 1; break;
		}
		nbcompiled[0] += here - then;
		nbcompiled[1] += g.outl.next - othen;
	}
	blk->end = g.time;
	g.curblk = 0;
	useblk(blk);
	if (!done) {
		char *then = here;
		ra_vflushall();
		nbcompiled[0] += here - then;
		if (jmpto) 
			co__jmpi(jmpto);
		else {
			struct block *bln;
			/* fallthrough to */
			bln = umc_mkblk(g.time, g.znz);
			depblk(blk, bln);
		}
	}
	return blk;
}

void *umc_enter(p_t x, znz_t znz)
{
	return umc_mkblk(x, znz)->jmp;
}

int umc_start(void)
{
	int (*entry)(int,int,int,int,int,int,int,int);

	newcod(&g.inl);
	newcod(&g.outl);
	g.c = &g.inl;
	entry = (void*)here;
	co_enter();
	umc_enter(0, 255);
	return entry(0,0,0,0,0,0,0,0);
}
