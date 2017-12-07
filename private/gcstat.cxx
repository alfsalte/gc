
#include <sys/time.h>

#include <ctime>

#include "gcstat.hxx"

#if 0
// seems timeradd vanished for some reason...
static
void timeradd(const struct timeval * a,
	      const struct timeval * b,
	      struct timeval * result)
{
  result->tv_sec = a->tv_sec + b->tv_sec;
  result->tv_usec = a->tv_usec + b->tv_usec;
  if (result->tv_usec >= 1000000) {
    ++result->tv_sec;
    result->tv_usec -= 1000000;
  }
}

static
void timersub(const struct timeval * a,
	      const struct timeval * b,
	      struct timeval * result)
{
  result->tv_sec = a->tv_sec - b->tv_sec;
  result->tv_usec = a->tv_usec - b->tv_usec;
  if (result->tv_usec < 0) {
    --result->tv_sec;
    result->tv_usec += 1000000;
  }
}
#endif

void alf::gc::statistics::gc_add_timing(const struct timeval & t)
{
  timeradd(& t, & timing, & timing);
  ++n_gc;
}

// reset num_gc() and time_gc().
void alf::gc::statistics::reset_num_gc()
{
  timing.tv_usec = 0;
  timing.tv_sec = 0;
  n_gc = 0;
}

// return total time in seconds spent on gc.
// pointer will receive time spent including nano seconds. 
time_t alf::gc::statistics::time_gc(struct timeval * ptv /* = 0 */ ) const
{
  if (ptv) *ptv = timing;
  return timing.tv_sec;
}

std::ostream & alf::gc::statistics::report(std::ostream & os) const
{
  char buf[100];
  int n = 0;

  std::time_t tt = timing.tv_sec;
  int sec = tt % 60;
  tt /= 60; // minutes.
  int min = tt % 60;
  tt /= 60; // hours.
  int hrs = tt % 24;
  tt /= 24; // days
  int days = tt % 7;
  tt /= 7; // weeks.
  os << "gc was called " << n_gc << " times (";
  if (tt != 0)
    n = sprintf( buf, "%d weeks ", int(tt));
  if (days != 0)
    n += sprintf( buf + n, "%d days ", days);
  bool longtime = false;
  if (hrs != 0 || min != 0 || days != 0 || tt != 0) {
    n += sprintf( buf + n, "%02d:%02d:%02", hrs, min, sec);
    longtime = true;
  } else
    n = sprintf(buf, "%d", sec);
  long us = timing.tv_usec;
  if (us)
    n += sprintf(buf + n, ".%06ld", us);
  if (! longtime)
    n += sprintf(buf + n, " secs");
  os << buf << ")" << std::endl;

  std::size_t usz_x = usz_a - usz_d;
  std::size_t sz_x = sz_a - sz_d;
  std::size_t usz_y = usz_f - usz_u;
  std::size_t sz_y = sz_f - sz_u;
  int n_c = n_a - n_d;
  int n_g = n_freeze - n_unfreeze;

  os << "gc: " << n_a << " alloc - " << n_d << " dealloc = "
     << n_c << " active" << std::endl;
  os << "user space " << usz_a << " alloc - " << usz_d
     << " dealloc = " << usz_x  << " in use"
     << std::endl;
  os << "system space is user space + overhead" << std::endl;
  os << "sys space " << sz_a << " alloc - " << sz_d
     << " dealloc = " << sz_x  << " in use"
     << std::endl;

  if (n_freeze) {
    os << "frozen: " << n_freeze << " frozen - " << n_unfreeze
       << " unfrozen = "
       << n_g << " cur frozen" << std::endl;
    os << "user space " << usz_f << " frozen - " << usz_u << " unfrozen = "
       << usz_y << " cur frozen" << std::endl;
    os << "sys spce " << sz_f << " frozen - " << sz_u << " unfrozen = "
       << sz_y << " cur frozen" << std::endl;
  }

  return os;
}
