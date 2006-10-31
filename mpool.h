#ifndef ___UMX__MPOOL_H
#define ___UMX__MPOOL_H

#include <string.h>
/*
  1 << SLSHIFT words
  1 << (SLSHIFT-poolshift) objs
  1 << (SLSHIFT-poolshift-5) bitmap words
*/

#define MPOOL_SLSHIFT 14
#define MPOOL_MINFREE 8

struct mpool {
	unsigned *slab;
	unsigned *bmap;
	unsigned ptr;
	unsigned rsh;
};

#define MPO_OLS(name) name##_shift
#define MPO_OLP(name) name##_slab
#define MPOOL_DECL(name,shift) \
struct mpool MPO_OLP(name); \
static const int MPO_OLS(name) = shift

#define MPOOL_INIT(name) mpool_init(&(MPO_OLP(name)), MPO_OLS(name))
void mpool_init(struct mpool *, int);

#define MPO_NEXT(slabp) (*((unsigned**)&((slabp)[(1<<MPOOL_SLSHIFT)-1])))
#define MPO_BMAP(slabp) (*((unsigned**)&((slabp)[(1<<MPOOL_SLSHIFT)-2])))
#define MPO_FREE(slabp) ((slabp)[(1<<MPOOL_SLSHIFT)-3])
#define MPO_POST(slabp) (*((struct mpool**)&((slabp)[(1<<MPOOL_SLSHIFT)-4])))
#define MPO_BMW(shift) ((1U<<MPOOL_SLSHIFT)>>((shift)+5))
#define MPO_NWH 4
#define MPO_MNF(sh) ((1U<<MPOOL_SLSHIFT)>>((sh)+MPOOL_MINFREE))

#define MPOOL_ALLOC(var,name) \
	((var)=mpool_alloc(&MPO_OLP(name), MPO_OLS(name)))

void* mpool_alloc_eop(struct mpool *, int);
static inline void* mpool_alloc(struct mpool *po, int sh) {
        unsigned i;
	while (__builtin_expect(!po->bmap[po->ptr], 0))
		if (__builtin_expect(po->ptr-- == 0, 0))
			return mpool_alloc_eop(po, sh);
	i = 31^__builtin_clz(po->bmap[po->ptr]);
	po->bmap[po->ptr] &= ~(1<<i);
	--MPO_FREE(po->slab);
	return po->slab + ((i + 32*po->ptr) << sh);
}

#define MPO_PTS(ptr) ((unsigned*)(((unsigned)ptr)&(-4<<MPOOL_SLSHIFT)))
#define MPO_PTI(ptr,sl,sh) ((((unsigned*)ptr)-(sl))>>(sh))

#define MPOOL_FREE(name,ptr) (mpool_free((ptr),&(MPO_OLP(name)),MPO_OLS(name)))

static inline void mpool_free(void *ptr, struct mpool *po, int sh) {
        unsigned *sl = MPO_PTS(ptr);
	unsigned mi = MPO_PTI(ptr,sl,sh);
	if (sl == po->slab && po->ptr >= mi>>5) memset(ptr, 0, 4<<sh);
	MPO_BMAP(sl)[mi>>5] |= 1<<(mi&31);
	if (MPO_FREE(sl)++ == MPO_MNF(sh) && sl != po->slab) {
		MPO_NEXT(sl) = MPO_NEXT(po->slab);
		MPO_NEXT(po->slab) = sl;
	}
}

static inline void mpool_xfree(void *ptr) {
	unsigned *sl = MPO_PTS(ptr);
	struct mpool *po = MPO_POST(sl);
	mpool_free(ptr, po, po->rsh);
}

#endif
