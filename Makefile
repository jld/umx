### AUTOGENERATED; DO NOT EDIT ###
# -*- Makefile -*- 
.POSIX:
CC=gcc
CDEFS=
COPT=-O3 -g -fno-delete-null-pointer-checks -fstrict-aliasing
CWARN=-Wall -W
CINC=-I. -Ii386

CDBG=
CFLAGS=`sh oscflags.sh` $(COPT) $(CWARN) $(CDBG) $(CDEFS) $(CINC) $(XCF)

OBJS=rt.o util.o crt.o main.o salloc.o vers.o ra.o umc.o i386/co.o
umx: $(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJS) `sh oslflags.sh` -o umx

clean:
	-rm -f umx $(OBJS)

force: clean
	$(MAKE) umx

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

CS=$(OBJS:.o=.c)
Makefile: Makefile.tmpl $(CS)
	@echo '### AUTOGENERATED; DO NOT EDIT ###' > $@
	cat Makefile.tmpl >> $@
	$(MAKE) -f Makefile.tmpl _depend
_depend:
	@gcc -MM $(CDEFS) $(CINC) $(CS) >> Makefile
###END###
rt.o: rt.c stuff.h i386/machdep.h salloc.h
util.o: util.c stuff.h i386/machdep.h
crt.o: crt.c stuff.h i386/machdep.h crt.h
main.o: main.c stuff.h i386/machdep.h crt.h
salloc.o: salloc.c salloc.h stuff.h i386/machdep.h
vers.o: vers.c
ra.o: ra.c ra.h stuff.h i386/machdep.h co.h crt.h
umc.o: umc.c co.h stuff.h i386/machdep.h crt.h ra.h
co.o: i386/co.c stuff.h i386/machdep.h crt.h stuff.h salloc.h ra.h co.h \
 crt.h i386/emit.h