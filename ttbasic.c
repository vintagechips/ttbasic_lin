/*
	TOYOSHIKI Tiny BASIC for Linux
	(C)2015 Tetsuya Suzuki
	Build: cc ttbasic.c basic.c -o ttbasic
*/

#include <stdlib.h>
#include <time.h>

void basic(void); // prototype

int main(){
	srand((unsigned int)time(0)); // for RND function
	basic(); // call The BASIC
	return 0;
}
