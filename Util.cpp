#include "Util.h"

//This is threadsafe--as long as there isn't a race to the first call. There probably won't be, as long as setStart is the only RTI call.
double elapsed()
{
   static timespec start;
   static bool bOnce = true;
   if(bOnce)
   {
      bOnce = false;
      clock_gettime(CLOCK_REALTIME,&start);
   }
   timespec ts;
   clock_gettime(CLOCK_REALTIME,&ts);
   double secs = (ts.tv_sec - start.tv_sec)*1000;
   double nsecs = ts.tv_nsec - start.tv_nsec;
   secs += nsecs/(1000*1000);
   return secs;
}
static double setStart = elapsed();

int linux_kbhit()
{
   struct timeval tv;
   fd_set read_fd;

   tv.tv_sec=0;
   tv.tv_usec=0;
   FD_ZERO(&read_fd);
   FD_SET(0,&read_fd);

   if(select(1, &read_fd, NULL, NULL, &tv) == -1)
   return 0;

   if(FD_ISSET(0,&read_fd))
   return 1;

   return 0;
}
