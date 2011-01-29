/*
 * Copyright (c) 2006,2007 Jed Davis <jld@panix.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
