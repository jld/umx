#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "stuff.h"
#include "crt.h"
#include "salloc.h"
#include "ra.h"
#include "co.h"
#include "emit.h"

void co__mld(int mr, int vr) { e_umld(mr, vr); }
void co__mst(int mr, int vr) { e_umst(mr, vr); }
void co__mldi(int mr, int i) { e_movri(mr, i); }
void co__msti(int i, int vr) { e_umsti(i, vr); }
void co__mmr(int md, int ms) { e_movrr(md, ms); }

static void co__push(int r)
{
	if (ISR(r))
		e_pushr(g.vtom[r]);
	else if (ISC(r))
		e_pushi(g.con[r]);
	else
		e_pushu(r);
}

void co_enter(void)
{
	e_pushr(EBP);
	e_movrr(EBP,ESP);
        e_pushr(EBX); e_pushr(ESI); e_pushr(EDI);
	e_subri(ESP,12+16);
}

void co_cmov(int ra, int rb, int rc)
{
	int ma, mb, mc;

	mc = ra_mgetv(rc);
	/* Ick. */
	ma = ra_mgetv(ra);
	mb = ra_mgetv(rb);
	e_cmpri(mc, 0);
	jcc_over(CCz);
	e_movrr(ma, mb);
	end_over;
	ra_vdirty(ra);
}

static void co__ldst(char opc, int rs, int ri, int rd, int(*dget)(int))
{
	int ms, md, mi;
	
 	if (ISC(ri)) {
		p_t offs = g.con[ri] * 4;
		if (ISC(rs)) {
			offs += g.con[rs];
			md = dget(rd);
			CC(opc); Cmodrm(MODnd, md, RMabs); CW(offs);
			return;
		}
		ms = ra_mgetv(rs);
		md = dget(rd);
		CC(opc);
		if (ISB((int)offs)) {
			Cmodrm(MODdb, md, ms); CC(offs);
		} else {
			Cmodrm(MODdw, md, ms); CW(offs);
		}
	} else {
		mi = ra_mgetv(ri);
		ms = ra_mgetv(rs);
		md = dget(rd);
		CC(opc); Cmodrm(MODnd, md, RMsib); Csib(Sfour, mi, ms);
	}
}

void co_index(int ra, int rb, int rc)
{
	co__ldst(0x8B, rb, rc, ra, ra_mgetvd);
	ra_vdirty(ra);
}

static void co__cclear(void)
{ ra_mclear(EAX); ra_mclear(ECX); ra_mclear(EDX); }

static void co__postwrite(int ri, int znz)
{
	e_addri(ESP, 16);
	e_pushi(znz);
	e_pushi(g.time);
	co__push(ri);
	e_calli_rtnp(um_postwrite);
}

void co_amend_unsafe(int ra, int rb, int rc)
{
	co__ldst(0x89, ra, rb, rc, ra_mgetv);
}

void co_postwrite_0(int rb)
{
	co__cclear();
	co__postwrite(rb, g.znz);
}

void co_postwrite(int ra, int rb) 
{
	int ma;
	
	ma = ra_mgetv(ra);
	co__cclear();
	e_cmpri(ma, 0);
	e_jcc(g.outl.next, CCz); /* does this really need to be OOL? */
	g.c = &g.outl;
	co__postwrite(rb, g.znz | ZMASK(ra));
	g.c = &g.inl;
}

#define BOILERPLATE(stem)                  \
void co_##stem(int ra, int rb, int rc)     \
{                                          \
	int mab, mc;                       \
	                                   \
	mab = ra_mgetv(rb);                \
	mc = ra_mgetv(rc);                 \
	ra_mchange(mab, ra);               \
	e_##stem##rr(mab, mc);             \
}                                          \
void co_##stem##_i(int ra, int rb, p_t ic) \
{                                          \
	int mab;                           \
                                           \
	mab = ra_mgetv(rb);                \
	ra_mchange(mab, ra);               \
	e_##stem##ri(mab, ic);             \
}

BOILERPLATE(add)
BOILERPLATE(mul)
BOILERPLATE(and)

#undef BOILERPLATE

void co_not(int ra, int rbc)
{
	int m;

	m = ra_mgetv(rbc);
	ra_mchange(m, ra);
	e_notr(m);
}

void co_div(int ra, int rb, int rc)
{
	int mc;

	assert(rb != rc);
	ra_ldvm(rb, EAX);
	ra_mhold(EDX);
	mc = ra_mgetv(rc);
	ra_mclear(EDX);
	e_xorrr(EDX, EDX);
	assert(mc != EAX && mc != EDX);
	ra_mchange(EAX, ra);
	CC(0xF7); Cmodrm(MODreg, 6, mc);
	ra_mrelse(EDX);
}


void co_shr_i(int ra, int rb, int z)
{
	int mab;

	mab = ra_mgetv(rb);
	ra_mchange(mab, ra);
	e_shrri(mab, z);
}

void co_halt(void)
{
	e_xorrr(EAX,EAX);
	e_addri(ESP, 12);
	e_popr(EDI); e_popr(ESI); e_popr(EBX);
	e_leave(); e_ret();
}

void co_alloc(int rb, int rc)
{
	int mc, mb, len, i, mz;
	char* alloop;

	if (ISC(rc) && g.con[rc] <= 33) {
		len = (g.con[rc] + 1) * 4;
		if (len < 8)
			len = 8;
		mb = ra_mgetvd(rb);
		alloop = here;
		e_movra(mb, &sall__mtop);
		e_subri(mb, len+8);
		e_cmpra(mb, &sall__mbot);
		e_jcc(g.outl.next, CCb);
		e_addri(mb, 8);
		e_movar(mb, &sall__mtop);
		e_movdi(mb, 0, len);
		e_addri(mb, 4);
		mz = ra_mget();
		e_movri(mz, 0);
		for (i = 0; i < len-4; i += 4)
			e_movdr(mz, mb, i);

		g.c = &g.outl;
		e_pushr(ECX); e_pushr(EDX); e_pushr(EAX); e_pushi(len+8);
		e_calli(salloc_reload);
		e_popr(EAX);  e_popr(EAX);  e_popr(EDX);  e_popr(ECX);
		e_jmpi(alloop);
		g.c = &g.inl;
	} else {
		mc = ra_mgetv(rc);
		e_addri(ESP,4);
		e_pushr(mc);
		co__cclear();
		ra_mchange(EAX, rb);
		e_calli(um_alloc);
	}
}

void co_free(int rc)
{
	int mc, mlen, mlfr;
	
	mc = ra_mgetv(rc);
#if 0
	e_addri(ESP,4);
	e_pushr(mc);
	co__cclear();
	e_calli(um_free);
#else
	ra_mhold(mlen = ra_mget());
	ra_mhold(mlfr = ra_mget());
	e_subri(mc, 4);
	e_movrd(mlen, mc, 0);
	e_movra(mlfr, &sall__lastfree);
	e_cmprd(mc, mlfr, (int)&SAF_END(0));
	e_jcc(g.outl.next, CCz+1);
	e_addrr(mc, mlen);
	e_movdr(mc, mlfr, (int)&SAF_END(0));
	
	g.c = &g.outl;
	e_movdr(mlfr, mc, (int)&SAF_NXT(0));
	e_addrrr(mlfr, mc, mlen);
	e_movdr(mlfr, mc, (int)&SAF_END(0));
	e_movar(mc, &sall__lastfree);
	e_jmpi(g.inl.next);
	g.c = &g.inl;
	ra_mrelse(mlfr);
	ra_mrelse(mlen);
#endif
}

void co_output(int rc)
{
	int mc;

	mc = ra_mgetv(rc);
	e_addri(ESP,4);
	e_pushr(mc);
	co__cclear();
	e_calli(putchar);
}

void co_input(int rc)
{
	co__cclear();
	ra_mchange(EAX, rc);
	e_calli(getchar);
}

void co_loadguard(int rb, int rc)
{
	int mb;
	
	mb = ra_mgetv(rb);
	if (!ISNZ(rb)) {
		e_cmpri(mb, 0);
		e_jcc(g.outl.next, CCz+1);
		g.c = &g.outl;
	}
	e_addri(ESP,16);
	e_pushi(g.znz | NZMASK(rb));
	co__push(rc);
	e_pushr(mb);
	e_calli_rtnp(um_loadfar);
	if (!ISNZ(rb)) {
		g.c = &g.inl;
	}
}

void co_load_0c(p_t cc, znz_t znz)
{
	struct block *dst = getblkx(cc, znz);

	if (dst) {
		e_jmpi(dst->jmp);
		depblk(g.curblk, dst);
	} else {
		e_calli(g.outl.next);
		g.c = &g.outl;
		/* e_subri(ESP, 0); */
		e_pushi(znz);
		e_pushi((int)g.curblk);
		e_pushi(cc);
		e_calli(um_enterdep);
		e_addri(ESP,12);
		e_popr(EDX);
		e_subrr(EAX,EDX); /* rel */
		e_subri(EDX, 5); /* insn */
		/* movb [EDX], $E9 */
		CC(0xC6); Cmodrm(MODnd, 0, EDX); CC(0xE9);
		/* movl [EDX+1], EAX */
		CC(0x89); Cmodrm(MODdb, EAX, EDX); CC(1);
		e_jmpr(EDX);
		g.c = &g.inl;
	}
}

void co_load_0(int rc, znz_t znz)
{
	int mc;

	mc = ra_mgetv(rc);
	e_addri(ESP,8);
	e_pushi(znz);
	e_pushr(mc);
	e_calli(um_enter);
	e_jmpr(EAX);
}

void co_badness(const char *fmt_pc)
{
	e_addri(ESP,8);
	e_pushi(g.time);
	e_pushi((int)fmt_pc);
	e_calli(um_abend);
	CC(0xCC);
}

void co_condbr(int rc, int ri, p_t ct, p_t cf, znz_t zt, znz_t zf)
{
	int mc;
	
	mc = ra_mgetv(rc);
	e_cmpri(mc, 0);
	jcc_over(CCz);
	co_ortho(ri, ct);
	ra_vflush(ri);
	co_load_0c(ct, zt);
	end_over;
	co_load_0c(cf, zf);
}

void umc_codlink(struct cod *from, char *to)
{
	*from->next = 0xE9;
	from->next += 5;
	((int*)from->next)[-1] = to - from->next;
}

void co_fltnoex(void)
{
	e_addri(ESP, 12);
	e_pushi(g.znz);
	e_pushi(g.time);
	e_calli_rtnp(um_destroy_and_go);
}

int umc_start(void)
{
	int (*entry)(int,int,int,int,int,int,int,int);

	entry = (void*)here;
	co_enter();
	umc_enter(0, 255);
	return entry(0,0,0,0,0,0,0,0);
}
