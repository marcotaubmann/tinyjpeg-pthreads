#include <stdio.h>
#include "timeutil.h"

/*
*   Calculates the time difference between start and finish in msecs.
*/
long timevaldiff(timer *start, timer *finish){
  long msec;
  msec = (finish->tv_sec - start->tv_sec)*1000;
  msec += (finish->tv_usec - start->tv_usec)/1000;
  return msec;
}

/*
*   Prints the timediff in ms with the given action.
*/
void printtime(char *action, timer *start, timer *finish) {
  long elapsed = 0;
  elapsed = timevaldiff(start, finish);
  printf("%s finished in %.2fs (%ld ms)\n", action, (double)elapsed/1000, elapsed);
}
