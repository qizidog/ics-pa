#ifndef __COMMON_H__
#define __COMMON_H__

/* Uncomment these macros to enable corresponding functionality. */
#define HAS_CTE
//#define HAS_VME
//#define MULTIPROGRAM
//#define TIME_SHARING

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <debug.h>
#if !defined(__ISA_AM_NATIVE__)
#include <autoconf.h>  // auto generated macro by menuconfig of nemu
#endif

#endif
