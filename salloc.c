#include "salloc.h"
#include "stuff.h"
#include <stdlib.h>
#include <stdio.h>

#define SLAB (1<<16)

static unsigned sentinel = 0xDEADBEEF, *nextready;
#define mtop sall__mtop
#define mbot sall__mbot
#define lastfree sall__lastfree

static void get_more_ram(void);
static void list_cycle(void);

void salloc_init(void)
{
	nextready = lastfree = &sentinel;
	mtop = mbot = 0;
	get_more_ram();
	salloc_reload(0);
}

void salloc_reload(unsigned n)
{
	unsigned *nextusable;
	
	if (mtop - mbot >= 2) {
		SAF_END(mbot) = mtop;
		SAF_NXT(mbot) = lastfree;
		lastfree = mbot;
	} else if (mtop != mbot) {
		whine(("salloc_reload: O, lamentation!"));
	}

	/* 
	 * You know, if the top of lastfree is big enough, we should
	 * use that instead of nextready, for better locality (LIFO).
	 */
	if (nextready == &sentinel) {
		for (nextusable = lastfree;
		     nextusable != &sentinel &&
			 SAF_END(nextusable) - n/4 < nextusable;
		     nextusable = SAF_NXT(nextusable));

		if (nextusable == &sentinel)
			get_more_ram();
		else
			list_cycle(); /* XXX frags on head */
	}
	mbot = nextready;
	mtop = SAF_END(nextready);
	nextready = SAF_NXT(nextready);
}

static void get_more_ram(void)
{
	unsigned *newslab;

	newslab = malloc(SLAB);
	if (!newslab) 
		panic("salloc: out of sand");
	SAF_END(newslab) = newslab + (SLAB/4);
	SAF_NXT(newslab) = nextready;
	nextready = newslab;
}

#if 0
static void insert_and_stuff(unsigned **plst, unsigned* foo)
{
	unsigned *lst;

 top_of_insert:
	lst = *plst;
	if (lst == &sentinel) {
		goto justcons;
	} else if (SAF_END(lst) == foo) {
		SAF_END(lst) = SAF_END(foo);
	} else if (SAF_END(foo) == lst) {
		SAF_END(foo) = SAF_END(lst);
		SAF_NXT(foo) = SAF_NXT(lst);
		*plst = foo;
	} else if (foo > lst) {
 justcons:
		SAF_NXT(foo) = lst;
		*plst = foo;
	} else {
		plst = &SAF_NXT(lst);
		goto top_of_insert;
	}		
}
#endif

static unsigned *
merge_and_stuff(unsigned *la, unsigned *lb)
{
	unsigned *rv, **cdr;

	cdr = &rv;
	for (;;) {
		if (la == &sentinel) {
			*cdr = lb; 
			break;
		} else if (lb == &sentinel) {
			*cdr = la;
			break;
		} else if (SAF_END(la) == lb) {
			SAF_END(la) = SAF_END(lb);
			lb = SAF_NXT(lb);
		} else if (SAF_END(lb) == la) {
			SAF_END(lb) = SAF_END(la);
			la = SAF_NXT(la);
		} else if (la > lb) {
			*cdr = la;
			cdr = &SAF_NXT(la);
			la = SAF_NXT(la);
		} else {
			*cdr = lb;
			cdr = &SAF_NXT(lb);
			lb = SAF_NXT(lb);
		}
	}
	return rv;
}

static unsigned *
sort_and_stuff(unsigned *lst)
{
	unsigned *tort, **ptort, *hare, *la, *lb;
	hare = tort = lst;

	while(hare != &sentinel && SAF_NXT(hare) != &sentinel) {
		hare = SAF_NXT(SAF_NXT(hare));
		ptort = &SAF_NXT(tort);
		tort = SAF_NXT(tort);
	}
	if (tort == lst)
		return lst;
	*ptort = &sentinel;
	la = sort_and_stuff(lst);
	lb = sort_and_stuff(tort);
	return merge_and_stuff(la, lb);
}

static void list_cycle(void)
{
	nextready = sort_and_stuff(lastfree);
	lastfree = &sentinel;
}

unsigned *salloc_special(unsigned len)
{
	unsigned *ptr;

	len += SLAB - 1;
	len /= SLAB;
	len *= SLAB;
	ptr = malloc(len);
	if (!ptr)
		panic("salloc_special: no more sand");
	return ptr;
}

void sall_dump(unsigned *ptr)
{
	while(ptr != &sentinel) {
		fprintf(stderr, "%p - %p (%d)\n",
		    ptr, SAF_END(ptr), SAF_END(ptr) - ptr);
		ptr = SAF_NXT(ptr);
	}
}
