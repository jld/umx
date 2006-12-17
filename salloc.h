#include <string.h>

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
