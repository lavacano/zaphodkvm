#ifndef UTIL_H
#define UTIL_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <stdint.h>

//This gets rid of the "deprecated conversion from string constant to 'char *'" warning.
#pragma GCC diagnostic ignored "-Wwrite-strings"

inline void Sleep(uint32_t millisecs)
{
   if(millisecs % 1000)
   {
      usleep((millisecs % 1000)*1000);
   }
   sleep(millisecs/1000);
};

double elapsed();
int linux_kbhit();

#endif
