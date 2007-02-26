#include "stuff.h"

/* flush deferred store */
void ra_vflush(int v);

/* I need *this* mreg... */
void ra_mclear(int m);
/* ...and I need nothing else to get it for a while... */
void ra_mhold(int m);
/* ...done now. */
void ra_mrelse(int m);

/* get me some mreg */
int ra_mget(void);

/* get me this vreg in *this* mreg */ 
void ra_ldvm(int v, int m);

/* get me this vreg in some mreg */
int ra_mgetv(int v);

/* what it says on the tin: */
void ra_vflushall(void);

/* this vreg is being altered; clear memory residence bit */
void ra_vdirty(int v);
/* as above, and also clear register residence */
void ra_vinval(int v);

/* this mreg will shortly represent the contents of this vreg instead of
   whatever it was doing before; also, forget whatever the vreg used to be */
void ra_mchange(int m, int v);

/* get some mreg to hold this vreg, but I don't care what's in it, because
   I'm about to overwrite it */
int ra_mgetvd(int v);


/* Stuff for the MD codegen module to fill in */
void co__mld(int mr, int vr);
void co__mst(int mr, int vr);
void co__mldi(int mr, int i);
void co__msti(int i, int vr);
void co__mmr(int md, int ms);
extern const int regpref[], nregpref;
