static int ctz(unsigned l)
{
	int n = 0;
	if (!l) return -1;
	while (!(l&1)) { ++n; l>>=1; }
	return n;
}

#define here (g.c->next)
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

const int regpref[] = { EBX, ESI, EDI, ECX, EAX, EDX };
const int nregpref = 6;

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

SIV e_subri(int,int);
SIV e_addri(int r, int i) { 
	if (i == 1) e_incr(r);
	else if (i == -1) e_decr(r);
	else if (i == 128) e_subri(r, -i);
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
	else if (i == 128) e_addri(r, -i);
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

SIV e_calli_rtnp(void *i) {
	e_movrr(EAX, ESP);
	e_subri(EAX, 8);
	e_pushr(EAX);
	e_calli(i);
}

SIV e_umld(int mr, int ur) { CC(0x8B); Cmodrm(MODdb,mr,EBP); CC(DofU(ur)); }
SIV e_umst(int mr, int ur) { CC(0x89); Cmodrm(MODdb,mr,EBP); CC(DofU(ur)); }
SIV e_umsti(p_t im, int ur)
{ assert(ur>=0 && ur<8); CC(0xC7); Cmodrm(MODdb,0,EBP); CC(DofU(ur)); CW(im); }

#if 0
SIV e_umldc(int mr, int ur) 
{ if (ISC(ur)) e_movri(mr,g.con[ur]); else e_umld(mr,ur); }
#endif

#define jcc_over(cc) e_jcc(here, cc); do { char *there = here
#define end_over there[-1] = here - there; } while(0)
