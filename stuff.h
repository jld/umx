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

#ifndef ___UM__STUFF_H
#define ___UM__STUFF_H

#include "machdep.h"

#ifdef __GNUC__
# define um_printflike __attribute__((__format__ (printf, 1, 2)))
# define um_likely(x) (__builtin_expect((x) != 0, 1))
# define um_unlike(x) (__builtin_expect((x) != 0, 0))
#else
# define um_printflike
# define um_likely(x) (x)
# define um_unlike(x) (x)
#endif

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

typedef uint32_t p_t;
p_t *progdata;
p_t proglen;
unsigned long long nbcompiled[2];

void *um_interp(p_t *gpr, p_t ip);

void um_ipl(int argc, char **argv);
void um_newprog(p_t aid);
void um_halt(void);
void um_abend(const char* fmt, ...) um_printflike;
p_t *um_alloc(p_t len);
void um_free(p_t *arr);

#define INSN_OP(i) ((i)>>28)
#define INSN_A(i) (((i)>>6)&7)
#define INSN_B(i) (((i)>>3)&7)
#define INSN_C(i) ((i)&7)
#define INSN_IR(i) (((i)>>25)&7)
#define INSN_IM(i) ((i)&0x1FFFFFF)

void panic(const char* fmt, ...) um_printflike;
void yowl(const char* fmt, ...) um_printflike;
#ifdef UM_VERBOSE
# define whine(aa) yowl aa
#else
# define whine(aa) ((void)0)
#endif
#define ESTR (strerror(errno))

#define UM_PROGN "umx"
#define UM_IDENT "UMX"
extern const char *um_vers;

#endif
