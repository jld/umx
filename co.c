#include <stdlib.h>
#include <stdio.h>
#include "stuff.h"
#include "crt.h"

#define SIV static inline void

struct {
	struct cod inl, outl, *c;
	p_t time;
	struct block *curblk;
	p_t con[8];
	uint8_t cset;
} g;
#define here (g.c->next)
#define ISC(r) (g.cset&(1<<(r)))

SIV setcon(int r, p_t v) { g.cset |= 1 << r; g.con[r] = v; }
SIV noncon(int r) { g.cset &= ~(1 << r); }

#define CC(ch) (*here++ = (ch))
#define CW(w) (((int*)(here+=4))[-1] = (w))

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7

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
	if (i == 1) {
		CC(0xD1); Cmodrm(MODreg, ro, r);
	} else {
		CC(0xC1); Cmodrm(MODreg, ro, r); CC(i);
	}
}

SIV e_shrri(int r, int i) { e_shxri(r, i, 5); }
SIV e_shlri(int r, int i) { e_shxri(r, i, 4); }


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

SIV e_umldc(int mr, int ur) 
{ if (ISC(ur)) e_movri(mr,g.con[ur]); else e_umld(mr,ur); }

#define jcc_over(cc) e_jcc(here, cc); do { char *there = here
#define end_over there[-1] = here - there; } while(0)

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
	if (ra != rb) {
		e_umldc(EAX, rc);
		e_cmpri(EAX, 0);
		jcc_over(CCz);
		e_umldc(EAX, rb);
		e_umst(EAX, ra);
		end_over;
		noncon(ra);
	}
}

static void co_index(int ra, int rb, int rc)
{
	e_umldc(ECX, rc); /* index */
	e_umldc(EDX, rb); /* segment */
	CC(0x8B); Cmodrm(MODnd, EAX, RMsib);
	Csib(Sfour, ECX, EDX);
	e_umst(EAX, ra); /* data */
	noncon(ra);
}

static void co_amend(int ra, int rb, int rc)
{
	e_umldc(EAX, rc); /* data */
	e_umldc(ECX, rb); /* index */
	e_umldc(EDX, ra); /* segment */
	CC(0x89); Cmodrm(MODnd, EAX, RMsib);
	Csib(Sfour, ECX, EDX);
	if (ISC(rb) && !btst(prognowr, g.con[rb])) {
		bset(prognoex, g.con[rb]);
	} else {
		/* and now, the postwrite */
		e_cmpri(EDX, 0);
		e_jcc(g.outl.next, CCz);
		g.c = &g.outl;
		e_addri(ESP, 8);
		e_pushi(g.time);
		e_pushr(ECX);
		e_calli(um_postwrite);
		e_cmpri(EAX, 0);
		e_jcc(g.inl.next, CCz);
		e_jmpr(EAX);
		g.c = &g.inl;
	}
}

static void co_add(int ra, int rb, int rc)
{
	if (ISC(rb)&&ISC(rc)) {
		co_ortho(ra, g.con[rb] + g.con[rc]);
		return;
	}
	e_umldc(EAX, rb);
	e_umldc(EDX, rc);
	e_addrr(EAX, EDX);
	e_umst(EAX, ra);
	noncon(ra);
}

static void co_mul(int ra, int rb, int rc)
{
	if (ISC(rb)&&ISC(rc)) {
		co_ortho(ra, g.con[rb] * g.con[rc]);
		return;
	}
	e_umldc(EAX, rb);
	e_umldc(EDX, rc);
	e_mulrr(EAX, EDX);
	e_umst(EAX, ra);
	noncon(ra);
}

static void co_div(int ra, int rb, int rc)
{
	if (ISC(rb)&&ISC(rc) && g.con[rc]) {
		co_ortho(ra, g.con[rb] / g.con[rc]);
		return;
	}
	if (ISC(rc) && g.con[rc]) {
		p_t d = g.con[rc];
		int z = __builtin_ctz(d);
		if (d == 1U << z) {
			e_umld(EAX, rb);
			e_shrri(EAX, z);
			e_umst(EAX, ra);
			noncon(ra);
			return;
		}
	}

	e_umldc(ECX, rc);
	e_umldc(EAX, rb);
	e_xorrr(EDX, EDX);
	CC(0xF7); Cmodrm(MODreg, 6, ECX);
	e_umst(EAX, ra);
	noncon(ra);
}

static void co_nand(int ra, int rb, int rc)
{
	if (ISC(rb)&&ISC(rc)) {
		co_ortho(ra, ~(g.con[rb] & g.con[rc]));
		return;
	}
	e_umldc(EAX, rb);
	e_umldc(EDX, rc);
	e_andrr(EAX, EDX);
	e_notr(EAX);
	e_umst(EAX, ra);
	noncon(ra);
}

static void co_halt(void)
{
	e_xorrr(EAX,EAX);
	e_popr(EDI); e_popr(ESI); e_popr(EBX);
	e_leave(); e_ret();
}

static void co_alloc(int rb, int rc)
{
	p_t *(*allo)(p_t) = um_alloc;

	if (ISC(rc)) {
		if (g.con[rc] <= 3)
			allo = um_alloc3;
		else if (g.con[rc] <= 7)
			allo = um_alloc7;
	}
	e_umldc(EAX, rc);
	e_addri(ESP,4);
	e_pushr(EAX);
	e_calli(allo);
	e_umst(EAX, rb);
	noncon(rb);
}

static void co_free(int rc)
{
	e_umld(EAX, rc);
	e_addri(ESP,4);
	e_pushr(EAX);
	e_calli(um_free);
}

static void co_output(int rc)
{
	e_umldc(EAX, rc);
	e_addri(ESP,4);
	e_pushr(EAX);
	e_calli(putchar);
}

static void co_input(int rc)
{
	e_calli(getchar);
	e_umst(EAX, rc);
	noncon(rc);
}

static void co__loadguard(int rb, int rc)
{
	e_umldc(EDX, rb);
	e_cmpri(EDX, 0);
	e_jcc(g.outl.next, CCz+1);
	g.c = &g.outl;
	e_umld(EAX, rc);
	e_addri(ESP,8);
	e_pushr(EAX);
	e_pushr(EDX);
	e_calli(um_loadfar);
	e_jmpr(EAX);
	g.c = &g.inl;
}

static void co__load0c(p_t cc)
{
	struct block *dst = getblkx(cc);

	if (dst) {
		e_jmpi(dst->jmp);
		depblk(g.curblk, dst);
	} else {
		e_calli(g.outl.next);
		g.c = &g.outl;
		e_subri(ESP, 4);
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

static void co__load0(int rc)
{
	if (ISC(rc)) {
		co__load0c(g.con[rc]);
	} else {
		e_umld(EAX, rc);
		e_addri(ESP,4);
		e_pushr(EAX);
		e_calli(um_enter);
		e_jmpr(EAX);
	}	
}

static void co_load(int rb, int rc)
{
	co__loadguard(rb, rc);
	co__load0(rc);
}

static void co_ortho(int ri, p_t imm)
{
	e_movri(EAX, imm);
	e_umst(EAX, ri);
	setcon(ri, imm);
}

static void co_badness(void)
{
	static const char *str = "bad insn @%d";
	e_addri(ESP,8);
	e_pushi(g.time);
	e_pushi((int)str);
	e_calli(um_abend);
	CC(0xCC);
}

static void co_condbr(int rs, int rc, int ri, p_t ct, p_t cf)
{
	co__loadguard(rs, ri);
	e_umldc(EAX, rc);
	e_cmpri(EAX, 0);
	jcc_over(CCz);
	co__load0c(ct);
	end_over;
	co__load0c(cf);
}

void umc_codlink(struct cod *from, char *to)
{
	*from->next = 0xE9;
	from->next += 5;
	((int*)from->next)[-1] = to - from->next;
}

static struct block*
umc_mkblk(p_t x)
{
	int done = 0, thispg = x/UM_PGSZ;
	p_t i, o, a, b, c, limit;
	struct block *blk = malloc(sizeof(struct block)), *bli;
	void *jmpto = 0;

	limit = (thispg + 1) * UM_PGSZ;
	/* This is no longer strictly necessary; with more
	   optimization it might be worth removing? */
	for (bli = progblks[thispg]; bli; bli = bli->next)
		if (bli->begin > x && bli->begin < limit) {
			limit = bli->begin;
			jmpto = bli->jmp;
		}
	
	blk->begin = x;
	blk->jmp = here;
	
	g.curblk = blk;
	g.cset = 0;
	g.c = &g.inl;
	for (g.time = x; !done && g.time < limit; ++g.time) {
		char *then, *othen;
		if (g.time >= proglen) {
			co_badness(); done = 1; break;
		}
		if (btst(prognoex, g.time)) {
			e_calli(um_destroy_world);
			e_addri(ESP, 4);
			e_pushi(g.time);
			e_calli(um_enter);
			e_jmpr(EAX);
			done = 1;
			break;
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
					co_cmov(a, b, c);
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
		case 7: co_halt(); break;
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
		if (jmpto) 
			e_jmpi(jmpto);
		else {
			struct block *bln;
			/* fallthrough to */
			bln = umc_mkblk(g.time);
			depblk(blk, bln);
		}
	}
	return blk;
}

void *umc_enter(p_t x)
{
	return umc_mkblk(x)->jmp;
}

int umc_start(void)
{
	int (*entry)(int,int,int,int,int,int,int,int);

	newcod(&g.inl);
	newcod(&g.outl);
	g.c = &g.inl;
	entry = (void*)here;
	co_enter();
	umc_enter(0);
	return entry(0,0,0,0,0,0,0,0);
}

