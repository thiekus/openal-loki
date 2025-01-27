#ifndef AL_AL_SITECONFIG_H_
#define AL_AL_SITECONFIG_H_

/*
 * Wrap site specific config stuff
 */

#ifdef DARWIN_PBBUILDER
#include "config-osx.h"
#else /* not DARWIN_PBBUILDER */
#include "config.h"
#endif /* not DARWIN_PBBUILDER */

#if defined(_WIN32) || defined(__WIN32__)
#define snprintf _snprintf
#endif

#ifdef HAVE___ATTRIBUTE__
#define UNUSED(x) x __attribute__((unused))
#define AL_ATTRIBUTE_FORMAT_PRINTF_(x,y) __attribute__((format(printf, x, y)))
#define AL_ATTRIBUTE_NORETURN_ __attribute__((noreturn))
#else /* not HAVE___ATTRIBUTE__ */
#define UNUSED(x) x
#define AL_ATTRIBUTE_FORMAT_PRINTF_(x,y)
#define AL_ATTRIBUTE_NORETURN_
#endif /* not HAVE___ATTRIBUTE__ */

#define USE_LRINT 0 /* icculus look here JIV FIXME */

#if USE_LRINT

#define __USE_ISOC99 1
#define _ISOC99_SOURCE 1
#define __USE_EXTERN_INLINES 1
#define __FAST_MATH__ 1
#include <math.h>

#endif /* USE_LRINT */

#endif /* not AL_SITE_CONFIG_H_ */
