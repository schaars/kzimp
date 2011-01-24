/* timer header file, from Timer.h in PBFT code */

#ifndef _TIMER_
#define _TIMER_

#include <sys/time.h>
#include <unistd.h>

struct timer {
  struct timeval t0;
  struct timeval t1;
  float accumulated;
  int running;
};


inline static float diff_time(struct timeval t0, struct timeval t1) {
    return (t1.tv_sec-t0.tv_sec)+(t1.tv_usec-t0.tv_usec)/1e6;
    // preserved significant digits by subtracting separately
}


/* reset timer T */
inline static void timer_reset(struct timer *T) {
  T->running = 0;
  T->accumulated = 0.0;
}


/* initialize the timer T */
inline static void timer_init(struct timer *T) {
  timer_reset(T);
}


/* start timer T */
inline static void timer_on(struct timer *T) {
  if (!(T->running)) {
    T->running = 1;
    gettimeofday(&(T->t0), 0);
  }
}


/* stop timer T */
inline static void timer_off(struct timer *T) {
  if (T->running) {
    T->running = 0;
    gettimeofday(&(T->t1), 0);
    T->accumulated += diff_time(T->t0, T->t1);
  }
}


/* return timer T elapsed time */
inline static float timer_elapsed(struct timer T) {
  float runtime;

  if (T.running) {
    gettimeofday(&(T.t1), 0);
    runtime = diff_time(T.t0, T.t1);
    return (T.accumulated + runtime);
  } else {
    return T.accumulated;
  }
}

#endif
