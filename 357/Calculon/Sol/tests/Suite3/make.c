#include <stdio.h>

int main()
{
#ifdef MADE
	printf("This program was built using Make!\n");
#endif
	return 0;
}
