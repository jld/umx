#ifndef ___UM__STUFF_H
#define ___UM__STUFF_H

#ifdef __GNUC__
# define um_printflike __attribute__((__format__ (printf, 1, 2)))
#else
# define um_printflike
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
p_t *um_alloc3(p_t len);
p_t *um_alloc7(p_t len);
void um_free(p_t *arr);

#define INSN_OP(i) ((i)>>28)
#define INSN_A(i) (((i)>>6)&7)
#define INSN_B(i) (((i)>>3)&7)
#define INSN_C(i) ((i)&7)
#define INSN_IR(i) (((i)>>25)&7)
#define INSN_IM(i) ((i)&0x1FFFFFF)

/*
enum um_pf_how {
	UM_PF_LOAD,
	UM_PF_HALT,
	UM_PF_ABEND,
	UM_PF_PANIC
};
*/

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
#define UM_VERS "0.6.1"
#define UM_CPU_IDENT "i386"

#endif
