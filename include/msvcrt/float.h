/*
 * Floating point arithmetic.
 *
 * Derived from the mingw header written by Colin Peters.
 * Modified for Wine use by Hans Leidekker.
 * This file is in the public domain.
 */

#ifndef __WINE_FLOAT_H
#define __WINE_FLOAT_H
#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif

#ifndef MSVCRT
# ifdef USE_MSVCRT_PREFIX
#  define MSVCRT(x)    MSVCRT_##x
# else
#  define MSVCRT(x)    x
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DBL_DIG        15
#define DBL_EPSILON    2.2204460492503131e-016
#define DBL_MANT_DIG   53
#define DBL_MAX        1.7976931348623158e+308
#define DBL_MAX_10_EXP 308
#define DBL_MAX_EXP    1024
#define DBL_MIN        2.2250738585072014e-308
#define DBL_MIN_10_EXP (-307)
#define DBL_MIN_EXP    (-1021)

#define _DBL_RADIX  2
#define _DBL_ROUNDS 1

#define DBL_RADIX  _DBL_RADIX
#define DBL_ROUNDS _DBL_ROUNDS

#define FLT_DIG        6
#define FLT_EPSILON    1.192092896e-07F
#define FLT_MANT_DIG   24
#define FLT_MAX        3.402823466e+38F
#define FLT_MAX_10_EXP 38
#define FLT_MAX_EXP    128
#define FLT_MIN        1.175494351e-38F
#define FLT_MIN_10_EXP (-37)
#define FLT_MIN_EXP    (-125)

#define FLT_RADIX  2
#define FLT_ROUNDS 1

#define LDBL_DIG        DBL_DIG
#define LDBL_EPSILON    DBL_EPSILON
#define LDBL_MANT_DIG   DBL_MANT_DIG
#define LDBL_MAX        DBL_MAX
#define LDBL_MAX_10_EXP DBL_MAX_10_EXP
#define LDBL_MAX_EXP    DBL_MAX_EXP
#define LDBL_MIN        DBL_MIN
#define LDBL_MIN_10_EXP DBL_MIN_10_EXP
#define LDBL_MIN_EXP    DBL_MIN_EXP

#define _LDBL_RADIX  _DBL_RADIX
#define _LDBL_ROUNDS _DBL_ROUNDS

#define LDBL_RADIX  _LDBL_RADIX
#define LDBL_ROUNDS _LDBL_ROUNDS

/* _controlfp masks and bitflags - x86 only so far */
#ifdef __i386__

/* Control word masks for unMask */
#define _MCW_EM 0x0008001F  /* Error masks */
#define _MCW_IC 0x00040000  /* Infinity */
#define _MCW_RC 0x00000300  /* Rounding */
#define _MCW_PC 0x00030000  /* Precision */

/* Control word values for unNew (use with related unMask above) */
#define _EM_INVALID    0x00000010
#define _EM_DENORMAL   0x00080000
#define _EM_ZERODIVIDE 0x00000008
#define _EM_OVERFLOW   0x00000004
#define _EM_UNDERFLOW  0x00000002
#define _EM_INEXACT    0x00000001
#define _IC_AFFINE     0x00040000
#define _IC_PROJECTIVE 0x00000000
#define _RC_CHOP       0x00000300
#define _RC_UP         0x00000200
#define _RC_DOWN       0x00000100
#define _RC_NEAR       0x00000000
#define _PC_24         0x00020000
#define _PC_53         0x00010000
#define _PC_64         0x00000000
#endif

/* _statusfp bit flags */
#define _SW_INEXACT    0x00000001 /* inexact (precision) */
#define _SW_UNDERFLOW  0x00000002 /* underflow */
#define _SW_OVERFLOW   0x00000004 /* overflow */
#define _SW_ZERODIVIDE 0x00000008 /* zero divide */
#define _SW_INVALID    0x00000010 /* invalid */

#define _SW_UNEMULATED     0x00000040  /* unemulated instruction */
#define _SW_SQRTNEG        0x00000080  /* square root of a neg number */
#define _SW_STACKOVERFLOW  0x00000200  /* FP stack overflow */
#define _SW_STACKUNDERFLOW 0x00000400  /* FP stack underflow */

#define _SW_DENORMAL 0x00080000 /* denormal status bit */

/* fpclass constants */
#define _FPCLASS_SNAN 0x0001  /* Signaling "Not a Number" */
#define _FPCLASS_QNAN 0x0002  /* Quiet "Not a Number" */
#define _FPCLASS_NINF 0x0004  /* Negative Infinity */
#define _FPCLASS_NN   0x0008  /* Negative Normal */
#define _FPCLASS_ND   0x0010  /* Negative Denormal */
#define _FPCLASS_NZ   0x0020  /* Negative Zero */
#define _FPCLASS_PZ   0x0040  /* Positive Zero */
#define _FPCLASS_PD   0x0080  /* Positive Denormal */
#define _FPCLASS_PN   0x0100  /* Positive Normal */
#define _FPCLASS_PINF 0x0200  /* Positive Infinity */

double _copysign (double, double);
double _chgsign (double);
double _scalb(double, long);
double _logb(double);
double _nextafter(double, double);
int    _finite(double);
int    _isnan(double);
int    _fpclass(double);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_FLOAT_H */
