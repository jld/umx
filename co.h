#include "stuff.h"
#include "crt.h"
#include "machdep.h"

/* Prototypes for MD compile functions. */
void co_enter(void);
void co_cmov(int ra, int rb, int rc);
void co_index(int ra, int rb, int rc);
void co_amend(int ra, int rb, int rc);
void co_add(int ra, int rb, int rc);
void co_mul(int ra, int rb, int rc);
void co_div(int ra, int rb, int rc);
void co_nand(int ra, int rb, int rc);
void co_halt(void);
void co_alloc(int rb, int rc);
void co_free(int rc);
void co_output(int rc);
void co_input(int rc);
void co_load(int rb, int rc);
void co_ortho(int ri, p_t imm);
void co_badness(void);
void co_condbr(int rs, int rc, int ri, p_t ct, p_t cf);
void co_fltnoex(void);
void co__jmpi(void *a);

/* State shared by umc and co, 
   which perhaps the latter should care less about. */

/* FIXME: this should go in machdep.h or something */

struct {
	struct cod inl, outl, *c;
	p_t time;
	struct block *curblk;
	p_t con[8], mlru[8];
	znz_t znz;
	int8_t vtom[8], mtov[MD_NMREG];
	uint8_t cset, rset, mset;
} g;

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

#define SIV static inline void

SIV setcon(int r, p_t v) {
	g.cset |= 1 << r; g.con[r] = v;
	g.znz &= ~ZNZMASK(r);
	g.znz |= QZMASK(r,v);
}
SIV noncon(int r) { g.cset &= ~(1 << r); g.znz &= ~ZNZMASK(r); }
SIV setz(int r) { setcon(r, 0); }
SIV setnz(int r) { noncon(r); g.znz |= NZMASK(r); }

