#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "stuff.h"
#include "crt.h"
#include "salloc.h"

static int ctz(unsigned l)
{
	int n = 0;
	if (!l) return -1;
	while (!(l&1)) { ++n; l>>=1; }
	return n;
}

#define SIV static inline void
#define NMREG 8
#define NOREG (-1)
static const int8_t regpref[];
static const int nregpref;

struct {
	struct cod inl, outl, *c;
	p_t time;
	struct block *curblk;
	p_t con[8], mlru[8];
	znz_t znz;
	int8_t vtom[8], mtov[NMREG];
	uint8_t cset, rset, mset;
} g;
#define here (g.c->next)
#define ISC(r) (g.cset&(1<<(r)))
#define ISR(r) (g.rset&(1<<(r)))
#define ISM(r) (g.mset&(1<<(r)))
#define ZMASK(r) (1<<(r))
#define NZMASK(r) (256<<(r))
#define ZNZMASK(r) (257<<(r))
#define ISZ(r) (g.znz & ZMASK(r))
#define ISNZ(r) (g.znz & NZMASK(r))
#define ISUK(r) (0 == (g.znz & ZNZMASK(r)))
#define QZMASK(r,v) ((v)?NZMASK(r):ZMASK(r))

SIV setcon(int r, p_t v) {
	g.cset |= 1 << r; g.con[r] = v;
	g.znz &= ~ZNZMASK(r);
	g.znz |= QZMASK(r,v);
}
SIV noncon(int r) { g.cset &= ~(1 << r); g.znz &= ~ZNZMASK(r); }
SIV setz(int r) { setcon(r, 0); }
SIV setnz(int r) { noncon(r); g.znz |= NZMASK(r); }

#define CC(ch) (*here++ = (ch))
#define CW(w) (((int*)(here+=4))[-1] = (w))

/* --- */

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7

static const int8_t regpref[] = { EBX, ESI, EDI, ECX, EAX, EDX };
static const int nregpref = 6;

#define MODnd 0
#define MODdb 1
#define MODdw 2
#define MODreg 3
#define RMsib ESP
#define RMabs EBP

#define Sone 0
#define Stwo 1
#define Sfour 2
#define Seight 3
#define Bmagic EBP

#define CCo 0
#define CCb 2 /* u */
#define CCz 4
#define CCbe 6 /* u */
#define CCs 8
#define CCp 10
#define CCl 12 /* s */
#define CCle 14 /* s */

#define Cmodrm(mod,ro,rm) CC(((mod)<<6)|((ro)<<3)|(rm))
#define Csib Cmodrm
#define ISB(n) (((n)>=-128)&&((n)<128))
#define DofU(u) (4*(u+2))

SIV e_incr (int r) { CC(0x40+r); }
SIV e_decr (int r) { CC(0x48+r); }
SIV e_pushr(int r) { CC(0x50+r); }
SIV e_popr (int r) { CC(0x58+r); }

SIV e_notr(int r) { CC(0xF7); Cmodrm(MODreg,2,r); }

SIV e_addrr(int rd, int rs) { CC(0x01); Cmodrm(MODreg,rs,rd); }
SIV e_andrr(int rd, int rs) { CC(0x21); Cmodrm(MODreg,rs,rd); }
SIV e_subrr(int rd, int rs) { CC(0x29); Cmodrm(MODreg,rs,rd); }
SIV e_xorrr(int rd, int rs) { CC(0x31); Cmodrm(MODreg,rs,rd); }
SIV e_movrr(int rd, int rs) { CC(0x89); Cmodrm(MODreg,rs,rd); }

SIV e_mulrr(int rd, int rs) { CC(0x0F); CC(0xAF); Cmodrm(MODreg,rd,rs); }

SIV e_learrr(int rd, int rb, int ri, int sc) {
	CC(0x8D); Cmodrm(MODnd, rd, RMsib); Csib(sc, ri, rb);
}
#define e_addrrr(d,l,r) e_learrr(d,l,r,Sone)

SIV e_xxxra(int r, void* addr, int aop, int rop) {
	if (r == EAX) {
		CC(aop);
	} else {
		CC(rop); Cmodrm(MODnd, r, RMabs);
	}
	CW((int)addr);
}

SIV e_movra(int rd, void* addr) { e_xxxra(rd, addr, 0xA1, 0x8B); }
SIV e_movar(int rs, void* addr) { e_xxxra(rs, addr, 0xA3, 0x89); }


SIV e_xxxrd(int rd, int rb, int disp) {
	if (!disp) {
		Cmodrm(MODnd, rd, rb);
	} else if (ISB(disp)) {
		Cmodrm(MODdb, rd, rb); CC(disp);
	} else {
		Cmodrm(MODdw, rd, rb); CW(disp);
	}
}
SIV e_movrd(int rd, int rb, int disp) { CC(0x8B); e_xxxrd(rd, rb, disp); }
SIV e_movdr(int rs, int rb, int disp) { CC(0x89); e_xxxrd(rs, rb, disp); }

SIV e_movdi(int rb, int disp, int val) {
	CC(0xC7); e_xxxrd(0, rb, disp); CW(val);
}

SIV e_cmprd(int rl, int rb, int disp) { CC(0x3B); e_xxxrd(rl, rb, disp); }
SIV e_cmpdr(int rr, int rb, int disp) { CC(0x39); e_xxxrd(rr, rb, disp); }
SIV e_cmpra(int rl, void* addr) { e_cmprd(rl, RMabs, 0); CW((int)addr); }
SIV e_cmpar(int rr, void* addr) { e_cmpdr(rr, RMabs, 0); CW((int)addr); }

SIV e_pushi(int i) {
	if (ISB(i)) { 
		CC(0x6A); CC(i);
	} else {
		CC(0x68); CW(i);
	}
}

SIV e_movri(int r, int i)
{
	if (i == 0) e_xorrr(r,r);
	else { CC(0xB8+r); CW(i); }
}

SIV e_xxxri(int r, int i, int or, int aop)
{
	if (ISB(i)) {
		CC(0x83); Cmodrm(MODreg,or,r); CC(i);
	} else if (!r) {
		CC(aop); CW(i);
	} else {
		CC(0x81); Cmodrm(MODreg,or,r); CW(i);
	}
}

SIV e_addri(int r, int i) { 
	if (i == 1) e_incr(r);
	else if (i == -1) e_decr(r);
	else e_xxxri(r, i, 0, 0x05);
}
SIV e_andri(int r, int i) {
	if (i == 0) e_xorrr(r,r);
	else if (i == ~0) return;
	else e_xxxri(r, i, 4, 0x25);
}
SIV e_subri(int r, int i) {
	if (i == -1) e_incr(r);
	else if (i == 1) e_decr(r);
	e_xxxri(r, i, 5, 0x2D);
}
SIV e_xorri(int r, int i) {
	if (i == 0) return;
	else if (i == ~0) e_notr(r);
	else e_xxxri(r, i, 6, 0x35);
}
SIV e_cmpri(int r, int i) {
	if (i == 0) { /* test %r,%r */
		CC(0x85); Cmodrm(MODreg, r, r);
	} else
		e_xxxri(r, i, 7, 0x3D);
}

SIV e_shxri(int r, int i, int ro) {
	if (i == 0)
		return;
	if (i == 1) {
		CC(0xD1); Cmodrm(MODreg, ro, r);
	} else {
		CC(0xC1); Cmodrm(MODreg, ro, r); CC(i);
	}
}

SIV e_shrri(int r, int i) { e_shxri(r, i, 5); }
SIV e_shlri(int r, int i) { e_shxri(r, i, 4); }

SIV e_mulri(int r, p_t i) {
	int z = ctz(i);
	p_t m = i >> z;

	if (i == 0) {
		e_xorrr(r, r);
		return;
	}
	switch(m) {
	case 9:
		e_learrr(r, r, r, Seight);
		goto shl;
	case 5:
		e_learrr(r, r, r, Sfour);
		goto shl;
	case 3:
		e_learrr(r, r, r, Stwo);
	case 1: shl:
		e_shlri(r, z);
		break;
	default:
		if (i < 128) {
			CC(0x6B); Cmodrm(MODreg, r, r); CC(i);
		} else {
			CC(0x69); Cmodrm(MODreg, r, r); CW(i);
		}
	}
}


SIV e_jmpr(int r) {
	CC(0xFF); Cmodrm(MODreg,4,r);
}

SIV e_jmpi(void* i) {
	int s = (char*)i-(here+2);
	int l = (char*)i-(here+5);
	if (ISB(s)) { CC(0xEB); CC(s); }
	else { CC(0xE9); CW(l); }
}

SIV e_calli(void *i) {
	int l = (char*)i-(here+5);
	CC(0xE8); CW(l);
}

SIV e_ret(void) { CC(0xC3); }
SIV e_leave(void) { CC(0xC9); }

SIV e_jcc(void* i, int cc) {
	int s = (char*)i-(here+2);
	int l = (char*)i-(here+6);
	if (ISB(s)) { CC(0x70+cc); CC(s); }
	else { CC(0x0F); CC(0x80+cc); CW(l); }
}

SIV e_umld(int mr, int ur) { CC(0x8B); Cmodrm(MODdb,mr,EBP); CC(DofU(ur)); }
SIV e_umst(int mr, int ur) { CC(0x89); Cmodrm(MODdb,mr,EBP); CC(DofU(ur)); }
SIV e_umsti(p_t im, int ur)
{ assert(ur>=0 && ur<8); CC(0xC7); Cmodrm(MODdb,0,EBP); CC(DofU(ur)); CW(im); }

SIV e_umldc(int mr, int ur) 
{ if (ISC(ur)) e_movri(mr,g.con[ur]); else e_umld(mr,ur); }

#define jcc_over(cc) e_jcc(here, cc); do { char *there = here
#define end_over there[-1] = here - there; } while(0)

/* --- */

SIV ra_mtouch(int m) { g.mlru[m] = g.time; }

static void ra_vflush(int v)
{ /* flush deferred store */
	if (!ISM(v)) {
		if (ISR(v)) 
			e_umst(g.vtom[v], v);
		else 
			e_umsti(g.con[v], v);
		g.mset |= (1<<v);
	}
}

static void ra_mclear(int m)
{ /* I need *this* mreg */
	int v = g.mtov[m];

	if (v < 0)
		return;
	
	ra_vflush(v);
	g.rset &= ~(1<<v);
	g.mtov[m] = -1;
}

/* ...and I need nothing else to get it for a while */
SIV ra_mhold(int m) { ra_mclear(m); g.mtov[m] = -2; }
SIV ra_mrelse(int m) { assert(g.mtov[m] == -2); g.mtov[m] = -1; }

static int ra__dislike(int m)
{ /* how much do I want to eject this mreg? */
	int v, d;
	
	v = g.mtov[m];
	if (v < -1) /* it's held; don't eject */
		return -1;
	if (v < 0) /* it's free; eject! */
		return 1<<30;
	d = 4*(g.time - g.mlru[m]);
	d = d + (ISM(v) ? 1 : -1) * (ISC(v) ? (d/2) : (d/4));
	return d;
}

static int ra_mget(void)
{ /* get me some mreg */
	int i, d, m, dd, mm;

	m = -1; d = -2;
	for (i = 0; i < nregpref; ++i) {
		mm = regpref[i];
		dd = ra__dislike(mm);
		if (dd > d) {
			d = dd;
			m = mm;
		}
	}
	assert(m >= 0 && m < NMREG);
	assert(d > 0); /* mregs used for this insn (or held) are off-limits */
	ra_mclear(m);
	return m;
}

static void ra_ldvm(int v, int m)
{ /* get me this vreg in *this* mreg */ 
	ra_mclear(m);
	if (ISR(v)) {
		e_movrr(m, g.vtom[v]);
		g.mtov[g.vtom[v]] = -1;
	} else
		e_umldc(m, v);
	g.rset |= (1<<v);
	g.mtov[m] = v;
	g.vtom[v] = m;
	ra_mtouch(m);
}

static int ra_mgetv(int v)
{ /* get me this vreg in some mreg */
	int m;
	
	if (ISR(v)) {
		m = g.vtom[v];
		assert(m >= 0 && m < NMREG);
		ra_mtouch(m);
	} else {
		m = ra_mget();
		ra_ldvm(v, m);
	}
	return m;
}

static void ra_vflushall()
{ /* what it says on the tin */
	int i;
	
	for (i = 0; i < 8; ++i)
		ra_vflush(i);
}

SIV ra_vdirty(int v) { g.mset &= ~(1<<v); }

static void ra_vinval(int v)
{
	if (ISR(v)) {
		int m = g.vtom[v];
		g.mtov[m] = -1;
		g.rset &= ~(1<<v);
	}
	ra_vdirty(v);
}

static void ra_mchange(int m, int v)
{ /* this mreg will shortly represent the contents of this vreg instead of
     whatever it was doing before; also, forget whatever the vreg used to be */
	if (ISR(v)) {
		int om = g.vtom[v];
		if (om == m) goto done;
		g.mtov[om] = -1;
	}
	ra_mclear(m);
	g.rset |= (1<<v);
	g.mtov[m] = v;
	g.vtom[v] = m;
 done:
	ra_mtouch(m);
	ra_vdirty(v);
}

static int ra_mgetvd(int v)
{ /* get some mreg to hold this vreg, but I don't care what's in it, because
     I'm about to overwrite it */
	int m;

	if (ISR(v))
		m = g.vtom[v];
	else {
		m = ra_mget();
		g.vtom[v] = m;
		g.mtov[m] = v;
		g.rset |= (1<<v);
	}
	ra_mtouch(m);
	ra_vdirty(v);
	return m;	
}

/* --- */

static void co_ortho(int, p_t);

static void co_enter(void)
{
	e_pushr(EBP);
	e_movrr(EBP,ESP);
        e_pushr(EBX); e_pushr(ESI); e_pushr(EDI);
	e_subri(ESP,12);
}

static void co_cmov(int ra, int rb, int rc)
{
	int ma, mb, mc;

	if (ISZ(rc) || ra == rb)
		return;
	if (ISNZ(rc)) {
		if (ISC(rb)) {
			co_ortho(ra, g.con[rb]);
			return;
		}
		mb = ra_mgetv(rb);
		ra_mchange(mb, ra);
		noncon(ra);
		if (ISNZ(rb))
			setnz(ra);
	} else {
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

static void co_index(int ra, int rb, int rc)
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

static void co_amend(int ra, int rb, int rc)
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

static void co_add(int ra, int rb, int rc)
{
	int mab, mc, rt;
	
	if (ISC(rb)&&ISC(rc)) {
		co_ortho(ra, g.con[rb] + g.con[rc]);
		return;
	}
	if (rc == ra || ISC(rb)) { rt = rb; rb = rc; rc = rt; }
	mab = ra_mgetv(rb);
	if (ISC(rc)) {
		ra_mchange(mab, ra);
		e_addri(mab, g.con[rc]);
	} else {
		mc = ra_mgetv(rc);
		ra_mchange(mab, ra);
		e_addrr(mab, mc);
	}
	noncon(ra);
}

static void co_mul(int ra, int rb, int rc)
{
	int mab, mc, rt;

	if (ISC(rb)&&ISC(rc)) {
		co_ortho(ra, g.con[rb] * g.con[rc]);
		return;
	}
	if (rc == ra || ISC(rb)) { rt = rb; rb = rc; rc = rt; }
	mab = ra_mgetv(rb);
	if (ISC(rc)) {
		ra_mchange(mab, ra);
		e_mulri(mab, g.con[rc]);
	} else {
		mc = ra_mgetv(rc);
		ra_mchange(mab, ra);
		e_mulrr(mab, mc);
	}
	noncon(ra);
}

static void co_div(int ra, int rb, int rc)
{
	int mc;

	if (ISC(rb)&&ISC(rc) && g.con[rc]) {
		co_ortho(ra, g.con[rb] / g.con[rc]);
		return;
	}
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

static void co_nand(int ra, int rb, int rc)
{
	int mab, mc, rt;

	if (ISC(rb)&&ISC(rc)) {
		co_ortho(ra, ~(g.con[rb] & g.con[rc]));
		return;
	}
	if (rc == ra || ISC(rb)) { rt = rb; rb = rc; rc = rt; }
	mab = ra_mgetv(rb);
	if (rb != rc) {
		if (ISC(rc)) {
			ra_mchange(mab, ra);
			e_andri(mab, g.con[rc]);
		} else {
			mc = ra_mgetv(rc);
			ra_mchange(mab, ra);
			e_andrr(mab, mc);
		}
	} else {
		ra_mchange(mab, ra);
	}
	e_notr(mab);
	noncon(ra);
}

static void co_halt(void)
{
	ra_vflushall();
	e_xorrr(EAX,EAX);
	e_addri(ESP, 12);
	e_popr(EDI); e_popr(ESI); e_popr(EBX);
	e_leave(); e_ret();
}

static void co_alloc(int rb, int rc)
{
	int mc, mb, len, i;
	char* alloop;

	if (ISC(rc) && g.con[rc] < 16) {
		len = (g.con[rc] + 1) * 4;
		if (len < 8)
			len = 8;
		mb = ra_mgetvd(rb);
		alloop = here;
		e_movra(mb, &sall__mtop);
		e_addri(mb, -len-8);
		e_cmpra(mb, &sall__mbot);
		e_jcc(g.outl.next, CCb);
		e_addri(mb, 8);
		e_movar(mb, &sall__mtop);
		e_movdi(mb, 0, len);
		e_addri(mb, 4);
		for (i = 0; i < len-4; i += 4)
			e_movdi(mb, i, 0);

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

static void co_free(int rc)
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

static void co_output(int rc)
{
	int mc;

	mc = ra_mgetv(rc);
	e_addri(ESP,4);
	e_pushr(mc);
	co__cclear();
	e_calli(putchar);
}

static void co_input(int rc)
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

static void co_load(int rb, int rc)
{
	ra_vflushall();
	co__loadguard(rb, rc);
	co__load0(rc, g.znz | ZMASK(rb));
}

static void co_ortho(int ri, p_t imm)
{
	ra_vinval(ri);
	setcon(ri, imm);
}

static void co_badness(void)
{
	static const char *str = "bad insn @%d";
	ra_vflushall();
	e_addri(ESP,8);
	e_pushi(g.time);
	e_pushi((int)str);
	e_calli(um_abend);
	CC(0xCC);
}

static void co_condbr(int rs, int rc, int ri, p_t ct, p_t cf)
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

static void co_fltnoex(void)
{
	ra_vflushall();
	e_calli(um_destroy_world);
	e_addri(ESP, 8);
	e_pushi(g.znz);
	e_pushi(g.time);
	e_calli(um_enter);
	e_jmpr(EAX);
}

/* --- */

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
			e_jmpi(jmpto);
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

