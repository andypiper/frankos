/*
 * frankos_math.c - Math function implementations for FRANK OS
 *
 * Provides round, trunc, fmod and ARM EABI 64-bit conversion helpers
 * that aren't available in the FRANK OS sys_table. These are needed
 * by soundgen.c and newsnd.c for the sound generator.
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * ARM EABI helpers for signed long long <-> double conversion.
 * Implemented in terms of int/unsigned int conversions which ARE
 * provided by m-os-api-math.c via sys_table.
 */
long long __aeabi_d2lz(double x) {
    /* Small values: use __aeabi_d2iz (int cast, provided by sys_table) */
    if (x > -2147483648.0 && x < 2147483648.0)
        return (long long)(int)x;
    /* Large values: split into hi/lo using unsigned int casts */
    int neg = (x < 0.0);
    if (neg) x = -x;
    unsigned int hi = (unsigned int)(x / 4294967296.0);
    unsigned int lo = (unsigned int)(x - (double)hi * 4294967296.0);
    long long r = (long long)(((unsigned long long)hi << 32) | lo);
    return neg ? -r : r;
}

double __aeabi_l2d(long long x) {
    if (x >= 0) {
        double hi = (double)(unsigned int)((unsigned long long)x >> 32);
        double lo = (double)(unsigned int)((unsigned long long)x & 0xFFFFFFFFu);
        return hi * 4294967296.0 + lo;
    }
    unsigned long long ux = (unsigned long long)(-x);
    double hi = (double)(unsigned int)(ux >> 32);
    double lo = (double)(unsigned int)(ux & 0xFFFFFFFFu);
    return -(hi * 4294967296.0 + lo);
}

int __aeabi_dcmple(double x, double y) {
    /* x <= y is true iff !(y < x) */
    /* __aeabi_dcmplt is provided by sys_table via m-os-api-math.c */
    extern int __aeabi_dcmplt(double, double);
    return !__aeabi_dcmplt(y, x);
}

/*
 * Standard math functions built on the EABI helpers above.
 */
double trunc(double x) {
    if (x >= 0.0) {
        if (x >= 9007199254740992.0)  /* 2^53: already integer */
            return x;
        return (double)(long long)x;
    } else {
        if (x < -9007199254740992.0)
            return x;
        return (double)(long long)x;
    }
}

double round(double x) {
    if (x >= 0.0)
        return trunc(x + 0.5);
    else
        return trunc(x - 0.5);
}

double fmod(double x, double y) {
    if (y == 0.0)
        return 0.0;
    return x - trunc(x / y) * y;
}

/*
 * ARM EABI helpers for unsigned long long <-> double conversion.
 */
unsigned long long __aeabi_d2ulz(double x) {
    if (x < 0.0)
        return 0;
    if (x < 4294967296.0)
        return (unsigned long long)(unsigned int)x;
    unsigned int hi = (unsigned int)(x / 4294967296.0);
    unsigned int lo = (unsigned int)(x - (double)hi * 4294967296.0);
    return ((unsigned long long)hi << 32) | lo;
}

double __aeabi_ul2d(unsigned long long x) {
    double hi = (double)(unsigned int)(x >> 32);
    double lo = (double)(unsigned int)(x & 0xFFFFFFFFu);
    return hi * 4294967296.0 + lo;
}
