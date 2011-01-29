/*
 * Copyright (c) 2006,2010 Jed Davis <jld@panix.com>
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

#include <string.h>
#ifndef __GNUC__
#define __builtin_expect(a,b) (a)
#endif

#define SALL_BIG 65536

unsigned *sall__mtop, *sall__mbot;
void salloc_reload(unsigned);
void salloc_init(void);
unsigned *salloc_special(unsigned);

static inline unsigned* salloc(unsigned n)
{
	unsigned *ptr, len = 4 * (n + 1);

	if (__builtin_expect(len > SALL_BIG, 0)) {
		ptr = salloc_special(len);
	} else for (;;) {
		ptr = sall__mtop - len/4 - 2;
		if (__builtin_expect(ptr < sall__mbot, 0))
			salloc_reload(len + 8);
		else {
			sall__mtop = ptr += 2;
			break;
		}
	}
	*ptr++ = len;
	if (n == 3) {
		ptr[0] = ptr[1] = ptr[2] = 0;
	} else {
		memset(ptr, 0, len - 4);
	}
	return ptr;
}

#define SAF_END(p) (((unsigned**)p)[0])
#define SAF_NXT(p) (((unsigned**)p)[1])

unsigned *sall__lastfree;

static inline void sfree(unsigned *ptr)
{
	unsigned len = *--ptr;
	unsigned *lfr = sall__lastfree;

	if (__builtin_expect(SAF_END(lfr) == ptr, 1)) {
		SAF_END(lfr) += len/4;
	} else {
		SAF_NXT(ptr) = lfr;
		SAF_END(ptr) = ptr + len/4;
		sall__lastfree = ptr;
	}
}

#define salloc_size(ptr) ((((unsigned*)(ptr))[-1])/4-1)
