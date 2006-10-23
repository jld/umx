#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "stuff.h"

static void
vyowl(const char* fmt, const char* pfx, va_list v)
{
	fprintf(stderr, UM_PROGN": %s", pfx);
	vfprintf(stderr, fmt, v);
	fprintf(stderr, "\n");
}

void
yowl(const char* fmt, ...)
{
	va_list v;

	va_start(v, fmt);
	vyowl(fmt, "", v);
	va_end(v);
}

void
panic(const char* fmt, ...)
{
	va_list v;

	va_start(v, fmt);
	vyowl(fmt, "PANIC: ", v);
	va_end(v);
	// cpu_progfini(UM_PF_PANIC);
	abort();
}
