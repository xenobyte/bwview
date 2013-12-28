//
//	Common include file
//
//        Copyright (c) 2002 Jim Peters <http://uazu.net/>.  Released
//        under the GNU GPL version 2 as published by the Free
//        Software Foundation.  See the file COPYING for details, or
//        visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	One of the target macros should be defined before this point:
//	T_LINUX, T_MINGW, or T_MSVC.
//

#define VERSION "1.0.5"
#define PROGNAME "bwview"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <SDL/SDL.h>
#include <math.h>

#ifndef T_MSVC
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef T_MSVC
#include <float.h>
#define NAN nan_global
#endif

#ifdef T_LINUX
//#include <sfftw.h>		// Single-precision version of fftw
//#include <dfftw.h>
//#include <drfftw.h>
#include <complex.h>
#include <fftw3.h>

#endif

#ifdef T_MINGW
#include <fftw.h>
#include <rfftw.h>
#endif

#ifdef T_MSVC
#define USE_FFTW_DLL
#include <fftw.h>
#include <rfftw.h>
#endif


// I really don't know if this is portable, but it works in GCC both
// with and without optimisation (GCC precompiles it into a constant)
#ifndef NAN
#define NAN (0.0/0.0)
#endif

#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif

#ifndef M_LN10
#define M_LN10         2.30258509299404568402  /* log_e 10 */
#endif

#ifdef T_MINGW
#define isnan(val) _isnan(val)
#endif

#ifdef T_MSVC
#define isnan(val) _isnan(val)
#endif


// With HEADER defined, these C files just give their header info
// (mainly structure-definitions).  Perhaps this is bit unusual --
// people normally seem to use separate header files -- but at least
// it keeps the structures together with their related functions.

#define HEADER 1
#include "file.c"
#include "analysis.c"
#include "config.c"
#undef HEADER

#include "proto.h"

#ifndef DEBUG_ON
#define DEBUG_ON 0
#endif

#define DEBUG if (DEBUG_ON) warn
#define ALLOC(type) ((type*)Alloc(sizeof(type)))
#define ALLOC_ARR(cnt, type) ((type*)Alloc((cnt) * sizeof(type)))

// END //

