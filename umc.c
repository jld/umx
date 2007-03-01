#include <stdlib.h>
#include "co.h"
#include "ra.h"

#define here (g.c->next)

static int ctz(unsigned l)
{
	int n = 0;
	if (!l) return -1;
	while (!(l&1)) { ++n; l>>=1; }
	return n;
}

void co_ortho(int ri, p_t imm)
{
	ra_vinval(ri);
	setcon(ri, imm);
}

void co_mov(int ra, int rb)
{
	int mab;
	
	mab = ra_mgetv(rb);
	ra_mchange(mab, ra);
	noncon(ra);
	if (ISNZ(rb))
		setnz(ra);
}

static void co_loadguard_x(int rb, int rc)
{
	if (ISZ(rb))
		return;
	co_loadguard(rb, rc);
	g.znz |= ZMASK(rb);
}

static struct block*
umc_mkblk(p_t x, znz_t znz)
{
	int done = 0;
	p_t i, o, a, b, c, limit;
	struct block *blk = malloc(sizeof(struct block));

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
			ra_vflushall();
			co_badness("end of program @%d");
			done = 1; break;
		}
		if (btst(prognoex, g.time)) {
			ra_vflushall();
			co_fltnoex();
			done = 1; break;
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

#define CFOLD(p, e)                               \
	if (ISC(b) && ISC(c)) {                   \
		p_t vb = g.con[b], vc = g.con[c]; \
		if (p) {                          \
			co_ortho(a, e);           \
			break;                    \
		}                                 \
	}

#ifdef MD_TWOREG
/* TODO: OP a, b, c  where a!=b!=c, b dirty, c clean */
# define COMMUTE do { if (c == a) { int t = b; b = c; c = t; } } while(0)
#else
# define COMMUTE ((void)0)
#endif

#define OP_IMM_COMM(stem) do {                 \
	COMMUTE;                               \
	if (ISC(c) && MD_IMM(g.con[c]))        \
		co_##stem##_i(a, b, g.con[c]); \
	else if (ISC(b) && MD_IMM(g.con[b]))   \
		co_##stem##_i(a, c, g.con[b]); \
	else                                   \
		co_##stem(a, b, c);            \
	noncon(a);                             \
} while(0)

		case 0:
			if (ISZ(c) || a == b) /* nop */
				break;
			if (ISNZ(c)) { /* move */
				if (ISC(b))
					co_ortho(a, g.con[b]);
				else
					co_mov(a, b);
				break;
			}
			if (g.time + 1 < limit) {
				p_t ii, oo, bb, cc;
				ii = progdata[g.time+1];
				oo = INSN_OP(ii);
				bb = INSN_B(ii);
				cc = INSN_C(ii);
				if (oo == 12 && a == cc && 
				    a != bb && a != b && a != c
				    && ISC(a) && ISC(b) && ISZ(bb)) {
					p_t ct = g.con[b], cf = g.con[a];
					znz_t znz = g.znz & ~ZNZMASK(a);
					
					ra_vflushall();
					co_condbr(c, a, ct, cf,
					    znz | QZMASK(a, ct) | NZMASK(c),
					    znz | QZMASK(a, cf) | ZMASK(c));
					++g.time; done = 1; break;
				}
			}
		{
			int nz = ISNZ(a) && ISNZ(b);
			co_cmov(a, b, c);
			noncon(a);
			if (nz)
				setnz(a);
		}
			break;
		case 1: 
			co_index(a, b, c);
			noncon(a);
			break;
		case 2: 
			co_amend_unsafe(a, b, c);

			if (ISNZ(a))
				break;
			if (ISZ(a)) {
				if (ISC(b) && !btst(prognowr, g.con[b]))
					bset(prognoex, g.con[b]);
				else {
					ra_vflushall();
					co_postwrite_0(b);
				}
			} else {
				ra_vflushall();
				co_postwrite(a, b);
			}
			break;
		case 3: 
			CFOLD(1, vb + vc);
			OP_IMM_COMM(add);
			break;
		case 4: 
			CFOLD(1, vb * vc);
			OP_IMM_COMM(mul);
			break;
		case 5:
			CFOLD(vc != 0, vb / vc);
			if (b == c) {
				co_ortho(a, 1);
				break;
			}
			if (ISC(c) && g.con[c]) {
				int z = ctz(g.con[c]);
				if (g.con[c] == 1U << z) {
					co_shr_i(a, b, z);
					noncon(a);
					break;
				}
			}
			co_div(a, b, c);
			noncon(a);
			break;
		case 6:
			CFOLD(1, ~(vb & vc));
			if (b != c)
				OP_IMM_COMM(and);
			else 
				co_mov(a, b);

			if (g.time + 1 < limit) { /* not-not-and */
				p_t ii = progdata[g.time+1];
				if (INSN_OP(ii) == 6 && INSN_A(ii) == a
				    && INSN_B(ii) == a && INSN_C(ii) == a) {
					++g.time;  break;
				}
			}
			co_not(a, a);
			noncon(a);
			break;
		case 7:
			ra_vflushall();
			co_halt();
			done = 1; break;
		case 8:
			co_alloc(b, c);
			noncon(b);
			setnz(b);
			break;
		case 9:
			co_free(c);
			setnz(c); /* this info could go backwards */
			break;
		case 10:
			co_output(c);
			break;
		case 11:
			co_input(c);
			noncon(c);
			break;
		case 12:
			ra_vflushall();
			co_loadguard_x(b, c);
			if (ISC(c))
				co_load_0c(g.con[c], g.znz);
			else
				co_load_0(c, g.znz);
			
			done = 1; break;
		case 13: 
			co_ortho(INSN_IR(i), INSN_IM(i));
			break;
		default:
			ra_vflushall();
			co_badness("bad insn @%d");
			done = 1; break;
		}
		nbcompiled[0] += here - then;
		nbcompiled[1] += g.outl.next - othen;
	}
	blk->end = g.time;
	g.curblk = 0;
	useblk(blk);
	if (!done) {
		char *then = here;
		struct block *bln;

		ra_vflushall();
		nbcompiled[0] += here - then;
		/* fallthrough to */
		bln = umc_mkblk(g.time, g.znz);
		depblk(blk, bln);
	}
	return blk;
}

void *umc_enter(p_t x, znz_t znz)
{
	return umc_mkblk(x, znz)->jmp;
}

void umc_init(void)
{
	newcod(&g.inl);
	newcod(&g.outl);
	g.c = &g.inl;
}

void umc_fini(void)
{
	nukecod(&g.inl);
	nukecod(&g.outl);
	g.c = (void*)-1;
}
