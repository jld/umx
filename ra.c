#include <assert.h>
#include "ra.h"
#include "co.h"

static inline void ra_mtouch(int m) { g.mlru[m] = g.time; }

void ra_vflush(int v)
{ 
	if (!ISM(v)) {
		if (ISR(v)) 
			co__mst(g.vtom[v], v);
		else 
			co__msti(g.con[v], v);
		g.mset |= (1<<v);
	}
}

void ra_mclear(int m)
{ 
	int v = g.mtov[m];

	if (v < 0)
		return;
	
	ra_vflush(v);
	g.rset &= ~(1<<v);
	g.mtov[m] = -1;
}


void ra_mhold(int m) { ra_mclear(m); g.mtov[m] = -2; }
void ra_mrelse(int m) { assert(g.mtov[m] == -2); g.mtov[m] = -1; }

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

int ra_mget(void)
{ 
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
	assert(m >= 0 && m < MD_NMREG);
	assert(d > 0); /* mregs used for this insn (or held) are off-limits */
	ra_mclear(m);
	return m;
}

void ra_ldvm(int v, int m)
{ 
	ra_mclear(m);
	if (ISR(v)) {
		co__mmr(m, g.vtom[v]);
		g.mtov[g.vtom[v]] = -1;
	} else if (ISC(v))
		co__mldi(m, g.con[v]);
	else 
		co__mld(m, v);
	g.rset |= (1<<v);
	g.mtov[m] = v;
	g.vtom[v] = m;
	ra_mtouch(m);
}

int ra_mgetv(int v)
{ 
	int m;
	
	if (ISR(v)) {
		m = g.vtom[v];
		assert(m >= 0 && m < MD_NMREG);
		ra_mtouch(m);
	} else {
		m = ra_mget();
		ra_ldvm(v, m);
	}
	return m;
}

void ra_vflushall()
{ 
	int i;
	
	for (i = 0; i < 8; ++i)
		ra_vflush(i);
}

void ra_vdirty(int v)
{
	g.mset &= ~(1<<v); 
}

void ra_vinval(int v)
{
	if (ISR(v)) {
		int m = g.vtom[v];
		g.mtov[m] = -1;
		g.rset &= ~(1<<v);
	}
	ra_vdirty(v);
}

void ra_mchange(int m, int v)
{ 
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

int ra_mgetvd(int v)
{ 
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
