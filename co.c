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

void co_enter(void)
{
	e_pushr(EBP);
	e_movrr(EBP,ESP);
        e_pushr(EBX); e_pushr(ESI); e_pushr(EDI);
	e_subri(ESP,12);
}

void co_cmov(int ra, int rb, int rc)
{
	int ma, mb, mc;

	int nz = ISNZ(ra) && ISNZ(rb);
	mc = ra_mgetv(rc);
	/* Ick. */
	ma = ra_mgetv(ra);
	mb = ra_mgetv(rb);
	e_cmpri(mc, 0);
	jcc_over(CCz);
	e_movrr(ma, mb);
	end_over;
	ra_vdirty(ra);
	noncon(ra);
	if (nz)
		setnz(ra);
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
	noncon(ra);
}

static void co__cclear(void)
{ ra_mclear(EAX); ra_mclear(ECX); ra_mclear(EDX); }

static void co__postwrite(int mi, int znz)
{
	e_addri(ESP, 12);
	e_pushi(znz);
	e_pushi(g.time);
	e_pushr(mi);
	e_calli(um_postwrite);
	e_cmpri(EAX, 0);
}

void co_amend(int ra, int rb, int rc)
{
	int ma, mb;
	
	co__ldst(0x89, ra, rb, rc, ra_mgetv);

	if (ISNZ(ra))
		return;
	if (ISZ(ra)) {
		if (ISC(rb) && !btst(prognowr, g.con[rb])) {
			bset(prognoex, g.con[rb]);
		} else {
			mb = ra_mgetv(rb); /* index */
			ra_vflushall();
			co__cclear();
			/* and now, the postwrite */
			co__postwrite(mb, g.znz);
			jcc_over(CCz);
			e_jmpr(EAX);
			end_over;
		} 
	} else {
		ma = ra_mgetv(ra); /* segment */		
		mb = ra_mgetv(rb); /* index */
		ra_vflushall();
		co__cclear();
		/* and now, the postwrite */
		e_cmpri(ma, 0);
		e_jcc(g.outl.next, CCz);
		g.c = &g.outl;
		co__postwrite(mb, g.znz | ZMASK(ra));
		e_jcc(g.inl.next, CCz);
		e_jmpr(EAX);
		g.c = &g.inl;
	}
}

void co_add(int ra, int rb, int rc)
{
	int mab, mc;
	
	mab = ra_mgetv(rb);
	mc = ra_mgetv(rc);
	ra_mchange(mab, ra);
	e_addrr(mab, mc);
	noncon(ra);
}

void co_add_i(int ra, int rb, p_t ic)
{
	int mab;

	mab = ra_mgetv(rb);
	ra_mchange(mab, ra);
	e_addri(mab, ic);
	noncon(ra);
}

void co_mul(int ra, int rb, int rc)
{
	int mab, mc;

	mab = ra_mgetv(rb);
	mc = ra_mgetv(rc);
	ra_mchange(mab, ra);
	e_mulrr(mab, mc);
	noncon(ra);
}

void co_mul_i(int ra, int rb, p_t ic)
{
	int mab;
	
	mab = ra_mgetv(rb);
	ra_mchange(mab, ra);
	e_mulri(mab, ic);
	noncon(ra);
}

void co_div(int ra, int rb, int rc)
{
	int mc;

	if (ISC(rc) && g.con[rc]) {
		p_t d = g.con[rc];
		int z = ctz(d), mab;
		if (d == 1U << z) {
			mab = ra_mgetv(rb);
			ra_mchange(mab, ra);
			e_shrri(mab, z);
			noncon(ra);
			return;
		}
	}

	ra_ldvm(rb, EAX);
	ra_mhold(EDX);
	mc = ra_mgetv(rc);
	ra_mclear(EDX);
	e_xorrr(EDX, EDX);
	assert(mc != EAX && mc != EDX);
	ra_mchange(EAX, ra);
	CC(0xF7); Cmodrm(MODreg, 6, mc);
	ra_mrelse(EDX);
	noncon(ra);
}

void co_and(int ra, int rb, int rc)
{
	int mab, mc;

	mab = ra_mgetv(rb);
	mc = ra_mgetv(rc);
	ra_mchange(mab, ra);
	e_andrr(mab, mc);
	noncon(ra);
}

void co_and_i(int ra, int rb, p_t ic)
{
	int mab;

	mab = ra_mgetv(rb);
	ra_mchange(mab, ra);
	e_andri(mab, ic);
	noncon(ra);
}

void co_not(int ra, int rbc)
{
	int m;

	m = ra_mgetv(rbc);
	ra_mchange(m, ra);
	e_notr(m);
	noncon(ra);
}

void co_halt(void)
{
	ra_vflushall();
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
	noncon(rb);
	setnz(rb);
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
	setnz(rc); /* this info could go backwards */
	
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
	noncon(rc);
}

static void co__loadguard(int rb, int rc)
{
	int mb;
	
	if (ISZ(rb))
		return;
	mb = ra_mgetv(rb);
	if (!ISNZ(rb)) {
		e_cmpri(mb, 0);
		e_jcc(g.outl.next, CCz+1);
		g.c = &g.outl;
	}
	e_addri(ESP,12);
	e_pushi(g.znz | NZMASK(rb));
	if (ISR(rc)) {
		e_pushr(g.vtom[rc]);
	} else {
		/* do a memory push; can't mutate ra state while branched */
		CC(0xFF); Cmodrm(MODdb, 6, EBP); CC(DofU(rc));
	}
	e_pushr(mb);
	e_calli(um_loadfar);
	e_jmpr(EAX);
	if (!ISNZ(rb)) {
		g.c = &g.inl;
	}
}

static void co__load0c(p_t cc, znz_t znz)
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

static void co__load0(int rc, znz_t znz)
{
	int mc;

	if (ISC(rc)) {
		co__load0c(g.con[rc], znz);
	} else {
		mc = ra_mgetv(rc);
		e_addri(ESP,8);
		e_pushi(znz);
		e_pushr(mc);
		e_calli(um_enter);
		e_jmpr(EAX);
	}
}

void co_load(int rb, int rc)
{
	ra_vflushall();
	co__loadguard(rb, rc);
	co__load0(rc, g.znz | ZMASK(rb));
}

void co_badness(void)
{
	static const char *str = "bad insn @%d";
	ra_vflushall();
	e_addri(ESP,8);
	e_pushi(g.time);
	e_pushi((int)str);
	e_calli(um_abend);
	CC(0xCC);
}

void co_condbr(int rs, int rc, int ri, p_t ct, p_t cf)
{
	znz_t znz = (g.znz & ~ZNZMASK(ri)) | ZMASK(rs);
	int mc;

	ra_vflushall();
	co__loadguard(rs, ri);
	mc = ra_mgetv(rc);
	e_cmpri(mc, 0);
	jcc_over(CCz);
	co_ortho(ri, ct);
	ra_vflush(ri);
	co__load0c(ct, znz | QZMASK(ri,ct) | NZMASK(rc));
	end_over;
	co__load0c(cf, znz | QZMASK(ri,ct) | ZMASK(rc));
}

void umc_codlink(struct cod *from, char *to)
{
	*from->next = 0xE9;
	from->next += 5;
	((int*)from->next)[-1] = to - from->next;
}

void co_fltnoex(void)
{
	ra_vflushall();
	e_calli(um_destroy_world);
	e_addri(ESP, 8);
	e_pushi(g.znz);
	e_pushi(g.time);
	e_calli(um_enter);
	e_jmpr(EAX);
}
