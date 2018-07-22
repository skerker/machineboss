#ifndef SOFTPLUS_INCLUDED
#define SOFTPLUS_INCLUDED

#include <limits>
#include <cmath>

using namespace std;

// global definitions relating to the cache
#define SOFTPLUS_CACHE_MAX_LOG    10
#define SOFTPLUS_INTLOG_PRECISION .0001
#define SOFTPLUS_CACHE_ENTRIES    (((long) (SOFTPLUS_CACHE_MAX_LOG / SOFTPLUS_INTLOG_PRECISION)) + 1)

// This can be used as a singleton object
// Creating a new one takes ~100,000 logs, which is not all that time-consuming really
class SoftPlus {

public:
  typedef double    Prob;
  typedef double    Log;
  typedef long long IntLog;

private:
  IntLog* cache;

  // rule of 5: our destructor means we need to specify (hide) the following 4
  SoftPlus (const SoftPlus&) = delete;  // copy constructor
  SoftPlus (SoftPlus&&) = delete;  // move constructor
  SoftPlus& operator= (const SoftPlus&) = delete;  // assignment operator
  SoftPlus& operator= (SoftPlus&&) = delete;  // move assignment operator

  static inline Log softplus (Log x) {
    return (Log) log (1 + exp(x));
  }

  inline IntLog int_softplus_neg (IntLog x) const {
    return x >= SOFTPLUS_CACHE_ENTRIES ? 0 : cache[x];
  }

  static inline IntLog log_to_int (Log x) {
    return (IntLog) (.5 + x / SOFTPLUS_INTLOG_PRECISION);
  }

  static inline Log int_to_log (IntLog x) {
    return (Log) (SOFTPLUS_INTLOG_PRECISION * (double) x);
  }

public:
  SoftPlus() {
    cache = new IntLog [SOFTPLUS_CACHE_ENTRIES];
    for (IntLog n = 0; n < SOFTPLUS_CACHE_ENTRIES; ++n)
      cache[n] = log_to_int (softplus (-int_to_log (n)));
  }

  // destructor
  ~SoftPlus() {
    delete cache;
  }

  static inline IntLog int_log (Prob x) {
    return (x > 0
	    ? log_to_int (log (x))
	    : numeric_limits<IntLog>::min());
  }
  static inline Prob int_exp (IntLog x) {
    return (Prob) exp (int_to_log (x));
  }

  IntLog int_logsumexp (IntLog a, IntLog b) const {
    return (a > b
	    ? (a + int_softplus_neg (a - b))
	    : (b + int_softplus_neg (b - a)));
  }
};

#endif /* SOFTPLUS_INCLUDED */