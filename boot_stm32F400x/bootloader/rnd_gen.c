#include "stdint.h"

//!!! ONLY FOR DEMONSTRATION DONT USE IN REAL PROJECT !!!
#error ONLY FOR DEMONSTRATION DONT USE IN REAL PROJECT

#define rnd_mul   1234567890123 
#define rnd_add   2345678901235

int64_t rnd_seed;

void rnd_init()
{
	 rnd_seed = 1234567890123456;
}

uint8_t rnd_calc()
{
	rnd_seed = (rnd_seed + rnd_add) * rnd_mul;

	union
	{
		int64_t v64;
		uint8_t dat[8];
	} s;
	s.v64 = rnd_seed;

	return  (s.dat[0] ^ s.dat[1] ^ s.dat[2] ^ s.dat[3] ^ s.dat[4] ^ s.dat[5] ^ s.dat[6] ^ s.dat[7]);
}

//!!! ONLY FOR DEMONSTRATION DONT USE IN REAL PROJECT !!!
