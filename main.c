#include "stuff.h"
#include "crt.h"

int main(int argc, char** argv)
{
	int x;

	um_ipl(argc, argv);
	um_crti();
	x = umc_start();
	um_halt();
	return x;
}
