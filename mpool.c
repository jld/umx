#include "mpool.h"
#include "stuff.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

static unsigned *
mpool_newslab(int sh)
{
	unsigned *sl, *bm, n;

#ifdef MAP_ALIGNED
        sl = mmap(0, 4 << MPOOL_SLSHIFT, PROT_READ | PROT_WRITE,
            MAP_ANON | MAP_ALIGNED(2 + MPOOL_SLSHIFT) | MAP_PRIVATE, -1, 0);
#else
        sl = mmap(0, 8 << MPOOL_SLSHIFT, PROT_READ | PROT_WRITE,
            MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
	if (sl == MAP_FAILED)
		panic("mpool_newslab: %s", ESTR);
	
#ifndef MAP_ALIGNED
	{
                unsigned misaln = (-(unsigned)sl) & ((4 << MPOOL_SLSHIFT) - 1);

                munmap(sl, misaln);
                munmap(sl + (misaln >> 2) + (1 << MPOOL_SLSHIFT),
                    (4 << MPOOL_SLSHIFT) - misaln);
                sl += misaln >> 2;
        }
#endif

	bm = malloc(MPO_BMW(sh)*4);
	memset(bm, 255, MPO_BMW(sh)*4);
	n = ((MPO_NWH + (1 << sh) - 1) >> sh);
	bm[MPO_BMW(sh)-1] >>= n;
	MPO_BMAP(sl) = bm;
	MPO_FREE(sl) = MPO_BMW(sh) * 32 - n;

	return sl;
}

static void
slab_wipe(unsigned *sl, int sh)
{
	unsigned bwi, ncl, *bm;

	bm = MPO_BMAP(sl);
	for (bwi = 0; bwi < MPO_BMW(sh); ++bwi) {
		if (bm[bwi] == ~0U) {
			memset(&sl[bwi << (5+sh)], 0, 4<<(5+sh));
			ncl += 32;
		} else {
			unsigned bmw, bn = 0;

			for (bmw = bm[bwi], bn = 0; bmw; bmw >>= 1, ++bn)
				if (bmw & 1) {
					memset(&sl[(bwi * 32 + bn) << sh],
					    0, 4<<sh);
					++ncl;
				}
		}
	}
}

static void
slab_putout(struct mpool *mp, unsigned *sl, int sh)
{
	mp->slab = sl;
	mp->bmap = MPO_BMAP(sl);
	mp->ptr = MPO_BMW(sh) - 1;
}

void mpool_init(struct mpool *mp, int sh)
{
	unsigned *sl;

	mp->rsh = sh;
	sl = mpool_newslab(sh);
	MPO_POST(sl) = mp;
	slab_putout(mp, sl, sh);
}

void *mpool_alloc_eop(struct mpool *mp, int sh)
{
	unsigned *sl = mp->slab;

	if (MPO_FREE(sl) <= MPO_MNF(sh)) {
		sl = MPO_NEXT(sl);
		MPO_NEXT(mp->slab) = 0;
	}
	if (!sl) {
		sl = mpool_newslab(sh);
		MPO_POST(sl) = mp;
	} else {
		slab_wipe(sl, sh);
	}
	
	slab_putout(mp, sl, sh);
	return mpool_alloc(mp, sh);
}
