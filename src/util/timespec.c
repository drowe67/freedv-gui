/* Functions for working with timespec structures
 * Written by Daniel Collins (2017-2021)
 * timespec_mod by Alex Forencich (2019)
 * Various contributions by Ingo Albrecht (2021)
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
*/

/** \file timespec.c
 *  \brief Functions for working with timespec structures.
 * 
 * This library aims to provide a comprehensive set of functions with
 * well-defined behaviour that handle all edge cases (e.g. negative values) in
 * a sensible manner.
 *
 * Negative values are allowed in the tv_sec and/or tv_usec field of timespec
 * structures, tv_usec is always relative to tv_sec, so mixing positive and
 * negative values will produce consistent results:
 *
 * <PRE>
 * { tv_sec = 1,  tv_nsec = 500000000  } ==  1.5 seconds
 * { tv_sec = 1,  tv_nsec = 0          } ==  1.0 seconds
 * { tv_sec = 1,  tv_nsec = -500000000 } ==  0.5 seconds
 * { tv_sec = 0,  tv_nsec = 500000000  } ==  0.5 seconds
 * { tv_sec = 0,  tv_nsec = 0          } ==  0.0 seconds
 * { tv_sec = 0,  tv_nsec = -500000000 } == -0.5 seconds
 * { tv_sec = -1, tv_nsec = 500000000  } == -0.5 seconds
 * { tv_sec = -1, tv_nsec = 0          } == -1.0 seconds
 * { tv_sec = -1, tv_nsec = -500000000 } == -1.5 seconds
 * </PRE>
 *
 * Furthermore, any timespec structure processed or returned by library functions
 * is normalised according to the rules in timespec_normalise().
*/

#include <limits.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#include "timespec.h"

#define NSEC_PER_SEC 1000000000

/** \fn struct timespec timespec_add(struct timespec ts1, struct timespec ts2)
 *  \brief Returns the result of adding two timespec structures.
*/
struct timespec timespec_add(struct timespec ts1, struct timespec ts2)
{
	/* Normalise inputs to prevent tv_nsec rollover if whole-second values
	 * are packed in it.
	*/
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	ts1.tv_sec  += ts2.tv_sec;
	ts1.tv_nsec += ts2.tv_nsec;
	
	return timespec_normalise(ts1);
}

/** \fn struct timespec timespec_sub(struct timespec ts1, struct timespec ts2)
 *  \brief Returns the result of subtracting ts2 from ts1.
*/
struct timespec timespec_sub(struct timespec ts1, struct timespec ts2)
{
	/* Normalise inputs to prevent tv_nsec rollover if whole-second values
	 * are packed in it.
	*/
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	ts1.tv_sec  -= ts2.tv_sec;
	ts1.tv_nsec -= ts2.tv_nsec;
	
	return timespec_normalise(ts1);
}

/** \fn struct timespec timespec_mod(struct timespec ts1, struct timespec ts2)
 *  \brief Returns the remainder left over after dividing ts1 by ts2 (ts1%ts2).
*/
struct timespec timespec_mod(struct timespec ts1, struct timespec ts2)
{
	int i = 0;
	bool neg1 = false;
	bool neg2 = false;

	/* Normalise inputs to prevent tv_nsec rollover if whole-second values
	 * are packed in it.
	*/
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);

	/* If ts2 is zero, just return ts1
	*/
	if (ts2.tv_sec == 0 && ts2.tv_nsec == 0)
	{
		return ts1;
	}

	/* If inputs are negative, flip and record sign
	*/
	if (ts1.tv_sec < 0 || ts1.tv_nsec < 0)
	{
		neg1 = true;
		ts1.tv_sec = -ts1.tv_sec;
		ts1.tv_nsec = -ts1.tv_nsec;
	}

	if (ts2.tv_sec < 0 || ts2.tv_nsec < 0)
	{
		neg2 = true;
		ts2.tv_sec = -ts2.tv_sec;
		ts2.tv_nsec = -ts2.tv_nsec;
	}

	/* Shift ts2 until it is larger than ts1 or is about to overflow
	*/
	while ((ts2.tv_sec < (LONG_MAX >> 1)) && timespec_ge(ts1, ts2))
	{
		i++;
		ts2.tv_nsec <<= 1;
		ts2.tv_sec <<= 1;
        if (ts2.tv_nsec > NSEC_PER_SEC)
        {
            ts2.tv_nsec -= NSEC_PER_SEC;
            ts2.tv_sec++;
        }
	}

	/* Division by repeated subtraction
	*/
	while (i >= 0)
	{
		if (timespec_ge(ts1, ts2))
		{
			ts1 = timespec_sub(ts1, ts2);
		}

		if (i == 0)
		{
			break;
		}

		i--;
		if (ts2.tv_sec & 1)
		{
			ts2.tv_nsec += NSEC_PER_SEC;
		}
		ts2.tv_nsec >>= 1;
		ts2.tv_sec >>= 1;
	}

	/* If signs differ and result is nonzero, subtract once more to cross zero
	*/
	if (neg1 ^ neg2 && (ts1.tv_sec != 0 || ts1.tv_nsec != 0))
	{
		ts1 = timespec_sub(ts1, ts2);
	}

	/* Restore sign
	*/
	if (neg1)
	{
		ts1.tv_sec = -ts1.tv_sec;
		ts1.tv_nsec = -ts1.tv_nsec;
	}

	return ts1;
}

/** \fn struct timespec timespec_min(struct timespec ts1, struct timespec ts2)
 *  \brief Return the lesser one of the two given timespec values.
*/
struct timespec timespec_min(struct timespec ts1, struct timespec ts2) {
	if(timespec_le(ts1, ts2)) {
		return ts1;
	} else {
		return ts2;
	}
}

/** \fn struct timespec timespec_max(struct timespec ts1, struct timespec ts2)
 *  \brief Return the greater one of the two given timespec values.
*/
struct timespec timespec_max(struct timespec ts1, struct timespec ts2) {
	if(timespec_ge(ts1, ts2)) {
		return ts1;
	} else {
		return ts2;
	}
}

/** \fn struct timespec timespec_clamp(struct timespec ts, struct timespec min, struct timespec max)
 *  \brief Clamp the value of TS between MIN and MAX.
*/
struct timespec timespec_clamp(struct timespec ts, struct timespec min, struct timespec max) {
	if(timespec_gt(ts, max)) {
		return max;
	}
	if(timespec_lt(ts, min)) {
		return min;
	}
	return ts;
}

/** \fn int timespec_cmp(struct timespec ts1, struct timespec ts2)
 *  \brief Returns (1, 0, -1) if ts1 is (greater than, equal to, less than) to ts2.
*/
int timespec_cmp(struct timespec ts1, struct timespec ts2)
{
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	if(ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec == ts2.tv_nsec)
	{
		return 0;
	}
	else if((ts1.tv_sec > ts2.tv_sec)
		  || (ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec > ts2.tv_nsec))
	{
		return 1;
	}
	else {
		return -1;
	}
}

/** \fn bool timespec_eq(struct timespec ts1, struct timespec ts2)
 *  \brief Returns true if the two timespec structures are equal.
*/
bool timespec_eq(struct timespec ts1, struct timespec ts2)
{
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	return (ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec == ts2.tv_nsec);
}

/** \fn bool timespec_gt(struct timespec ts1, struct timespec ts2)
 *  \brief Returns true if ts1 is greater than ts2.
*/
bool timespec_gt(struct timespec ts1, struct timespec ts2)
{
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	return (ts1.tv_sec > ts2.tv_sec || (ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec > ts2.tv_nsec));
}

/** \fn bool timespec_ge(struct timespec ts1, struct timespec ts2)
 *  \brief Returns true if ts1 is greater than or equal to ts2.
*/
bool timespec_ge(struct timespec ts1, struct timespec ts2)
{
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	return (ts1.tv_sec > ts2.tv_sec || (ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec >= ts2.tv_nsec));
}

/** \fn bool timespec_lt(struct timespec ts1, struct timespec ts2)
 *  \brief Returns true if ts1 is less than ts2.
*/
bool timespec_lt(struct timespec ts1, struct timespec ts2)
{
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	return (ts1.tv_sec < ts2.tv_sec || (ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec < ts2.tv_nsec));
}

/** \fn bool timespec_le(struct timespec ts1, struct timespec ts2)
 *  \brief Returns true if ts1 is less than or equal to ts2.
*/
bool timespec_le(struct timespec ts1, struct timespec ts2)
{
	ts1 = timespec_normalise(ts1);
	ts2 = timespec_normalise(ts2);
	
	return (ts1.tv_sec < ts2.tv_sec || (ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec <= ts2.tv_nsec));
}

/** \fn struct timespec timespec_from_double(double s)
 *  \brief Converts a fractional number of seconds to a timespec.
*/
struct timespec timespec_from_double(double s)
{
	struct timespec ts = {
		.tv_sec  = s,
		.tv_nsec = (s - (long)(s)) * NSEC_PER_SEC,
	};
	
	return timespec_normalise(ts);
}

/** \fn double timespec_to_double(struct timespec ts)
 *  \brief Converts a timespec to a fractional number of seconds.
*/
double timespec_to_double(struct timespec ts)
{
	return ((double)(ts.tv_sec) + ((double)(ts.tv_nsec) / NSEC_PER_SEC));
}

/** \fn struct timespec timespec_from_timeval(struct timeval tv)
 *  \brief Converts a timeval to a timespec.
*/
struct timespec timespec_from_timeval(struct timeval tv)
{
	struct timespec ts = {
		.tv_sec  = tv.tv_sec,
		.tv_nsec = tv.tv_usec * 1000
	};
	
	return timespec_normalise(ts);
}

/** \fn struct timeval timespec_to_timeval(struct timespec ts)
 *  \brief Converts a timespec to a timeval.
*/
struct timeval timespec_to_timeval(struct timespec ts)
{
	ts = timespec_normalise(ts);
	
	struct timeval tv = {
		.tv_sec  = ts.tv_sec,
		.tv_usec = ts.tv_nsec / 1000,
	};
	
	return tv;
}

/** \fn struct timespec timespec_from_ms(long milliseconds)
 *  \brief Converts an integer number of milliseconds to a timespec.
*/
struct timespec timespec_from_ms(long milliseconds)
{
	struct timespec ts = {
		.tv_sec  = (milliseconds / 1000),
		.tv_nsec = (milliseconds % 1000) * 1000000,
	};
	
	return timespec_normalise(ts);
}

/** \fn long timespec_to_ms(struct timespec ts)
 *  \brief Converts a timespec to an integer number of milliseconds.
*/
long timespec_to_ms(struct timespec ts)
{
	return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

/** \fn struct timespec timespec_normalise(struct timespec ts)
 *  \brief Normalises a timespec structure.
 *
 * Returns a normalised version of a timespec structure, according to the
 * following rules:
 *
 * 1) If tv_nsec is >=1,000,000,00 or <=-1,000,000,000, flatten the surplus
 *    nanoseconds into the tv_sec field.
 *
 * 2) If tv_nsec is negative, decrement tv_sec and roll tv_nsec up to represent
 *    the same value attainable by ADDING nanoseconds to tv_sec.
*/
struct timespec timespec_normalise(struct timespec ts)
{
	while(ts.tv_nsec >= NSEC_PER_SEC)
	{
		++(ts.tv_sec);
		ts.tv_nsec -= NSEC_PER_SEC;
	}
	
	while(ts.tv_nsec <= -NSEC_PER_SEC)
	{
		--(ts.tv_sec);
		ts.tv_nsec += NSEC_PER_SEC;
	}
	
	if(ts.tv_nsec < 0)
	{
		/* Negative nanoseconds isn't valid according to POSIX.
		 * Decrement tv_sec and roll tv_nsec over.
		*/
		
		--(ts.tv_sec);
		ts.tv_nsec = (NSEC_PER_SEC + ts.tv_nsec);
	}
	
	return ts;
}

#ifdef TEST
#include <stdio.h>

#define TEST_NORMALISE(ts_sec, ts_nsec, expect_sec, expect_nsec) { \
	struct timespec in  = { .tv_sec = ts_sec, .tv_nsec = ts_nsec }; \
	struct timespec got = timespec_normalise(in); \
	if(got.tv_sec != expect_sec || got.tv_nsec != expect_nsec) \
	{ \
		printf("%s:%d: timespec_normalise({%ld, %ld}) returned wrong values\n", __FILE__, __LINE__, \
			(long)(ts_sec), (long)(ts_nsec)); \
		printf("    Expected: {%ld, %ld}\n", (long)(expect_sec), (long)(expect_nsec)); \
		printf("    Got:      {%ld, %ld}\n", (long)(got.tv_sec), (long)(got.tv_nsec)); \
		++result; \
	} \
}

#define TEST_BINOP(func, ts1_sec, ts1_nsec, ts2_sec, ts2_nsec, expect_sec, expect_nsec) { \
	struct timespec ts1 = { .tv_sec = ts1_sec, .tv_nsec = ts1_nsec }; \
	struct timespec ts2 = { .tv_sec = ts2_sec, .tv_nsec = ts2_nsec }; \
	struct timespec got = func(ts1, ts2); \
	if(got.tv_sec != expect_sec || got.tv_nsec != expect_nsec) \
	{ \
		printf(#func "({%ld, %ld}, {%ld, %ld}) returned wrong values\n", \
			(long)(ts1_sec), (long)(ts1_nsec), (long)(ts2_sec), (long)(ts2_nsec)); \
		printf("    Expected: {%ld, %ld}\n", (long)(expect_sec), (long)(expect_nsec)); \
		printf("    Got:      {%ld, %ld}\n", (long)(got.tv_sec), (long)(got.tv_nsec)); \
		++result; \
	} \
}

#define TEST_TRINOP(func, ts1_sec, ts1_nsec, ts2_sec, ts2_nsec, ts3_sec, ts3_nsec, expect_sec, expect_nsec) { \
	struct timespec ts1 = { .tv_sec = ts1_sec, .tv_nsec = ts1_nsec }; \
	struct timespec ts2 = { .tv_sec = ts2_sec, .tv_nsec = ts2_nsec }; \
	struct timespec ts3 = { .tv_sec = ts3_sec, .tv_nsec = ts3_nsec }; \
	struct timespec got = func(ts1, ts2, ts3); \
	if(got.tv_sec != expect_sec || got.tv_nsec != expect_nsec) \
	{ \
		printf(#func "({%ld, %ld}, {%ld, %ld}, {%ld, %ld}) returned wrong values\n", \
		       (long)(ts1_sec), (long)(ts1_nsec), \
		       (long)(ts2_sec), (long)(ts2_nsec), \
		       (long)(ts3_sec), (long)(ts3_nsec)); \
		printf("    Expected: {%ld, %ld}\n", (long)(expect_sec), (long)(expect_nsec)); \
		printf("    Got:      {%ld, %ld}\n", (long)(got.tv_sec), (long)(got.tv_nsec)); \
		++result; \
	} \
}

#define TEST_TEST_FUNC(func, ts1_sec, ts1_nsec, ts2_sec, ts2_nsec, expect) { \
	struct timespec ts1 = { .tv_sec = ts1_sec, .tv_nsec = ts1_nsec }; \
	struct timespec ts2 = { .tv_sec = ts2_sec, .tv_nsec = ts2_nsec }; \
	int got = func(ts1, ts2); \
	if(got != expect) \
	{ \
		printf("%s:%d: " #func "({%ld, %ld}, {%ld, %ld}) returned %d, expected %s\n", __FILE__, __LINE__, \
			(long)(ts1_sec), (long)(ts1_nsec), (long)(ts2_sec), (long)(ts2_nsec), \
			got, #expect); \
		++result; \
	} \
}

#define TEST_FROM_DOUBLE(d_secs, expect_sec, expect_nsec) { \
	struct timespec got = timespec_from_double(d_secs);  \
	if(got.tv_sec != expect_sec || got.tv_nsec != expect_nsec) \
	{ \
		printf("%s:%d: timespec_from_double(%f) returned wrong values\n", __FILE__, __LINE__, (double)(d_secs)); \
		printf("    Expected: {%ld, %ld}\n", (long)(expect_sec), (long)(expect_nsec)); \
		printf("    Got:      {%ld, %ld}\n", (long)(got.tv_sec), (long)(got.tv_nsec)); \
		++result; \
	} \
}

#define TEST_TO_DOUBLE(ts_sec, ts_nsec, expect) { \
	struct timespec ts = { .tv_sec = ts_sec, .tv_nsec = ts_nsec }; \
	double got = timespec_to_double(ts); \
	if(got != expect) { \
		printf("%s:%d: timespec_to_double({%ld, %ld}) returned wrong value\n", __FILE__, __LINE__, \
			(long)(ts_sec), (long)(ts_nsec)); \
		printf("    Expected: %f\n", (double)(expect)); \
		printf("    Got:      %f\n", got); \
		++result; \
	} \
}

#define TEST_FROM_TIMEVAL(in_sec, in_usec, expect_sec, expect_nsec) { \
	struct timeval tv = { .tv_sec = in_sec, .tv_usec = in_usec }; \
	struct timespec got = timespec_from_timeval(tv); \
	if(got.tv_sec != expect_sec || got.tv_nsec != expect_nsec) \
	{ \
		printf("%s:%d: timespec_from_timeval({%ld, %ld}) returned wrong values\n", __FILE__, __LINE__, \
			(long)(in_sec), (long)(in_usec)); \
		printf("    Expected: {%ld, %ld}\n", (long)(expect_sec), (long)(expect_nsec)); \
		printf("    Got:      {%ld, %ld}\n", (long)(got.tv_sec), (long)(got.tv_nsec)); \
		++result; \
	} \
}

#define TEST_TO_TIMEVAL(ts_sec, ts_nsec, expect_sec, expect_usec) { \
	struct timespec ts = { .tv_sec = ts_sec, .tv_nsec = ts_nsec }; \
	struct timeval got = timespec_to_timeval(ts); \
	if(got.tv_sec != expect_sec || got.tv_usec != expect_usec) \
	{ \
		printf("%s:%d: timespec_to_timeval({%ld, %ld}) returned wrong values\n", __FILE__, __LINE__, \
			(long)(ts_sec), (long)(ts_nsec)); \
		printf("    Expected: {%ld, %ld}\n", (long)(expect_sec), (long)(expect_usec)); \
		printf("    Got:      {%ld, %ld}\n", (long)(got.tv_sec), (long)(got.tv_usec)); \
		++result; \
	} \
}

#define TEST_FROM_MS(msecs, expect_sec, expect_nsec) { \
	struct timespec got = timespec_from_ms(msecs);  \
	if(got.tv_sec != expect_sec || got.tv_nsec != expect_nsec) \
	{ \
		printf("%s:%d: timespec_from_ms(%ld) returned wrong values\n", __FILE__, __LINE__, (long)(msecs)); \
		printf("    Expected: {%ld, %ld}\n", (long)(expect_sec), (long)(expect_nsec)); \
		printf("    Got:      {%ld, %ld}\n", (long)(got.tv_sec), (long)(got.tv_nsec)); \
		++result; \
	} \
}

#define TEST_TO_MS(ts_sec, ts_nsec, expect) { \
	struct timespec ts = { .tv_sec = ts_sec, .tv_nsec = ts_nsec }; \
	long got = timespec_to_ms(ts); \
	if(got != expect) { \
		printf("%s:%d: timespec_to_ms({%ld, %ld}) returned wrong value\n", __FILE__, __LINE__, \
			(long)(ts_sec), (long)(ts_nsec)); \
		printf("    Expected: %ld\n", (long)(expect)); \
		printf("    Got:      %ld\n", got); \
		++result; \
	} \
}

int main()
{
	int result = 0;
	
	// timespec_add
	
	TEST_BINOP(timespec_add, 0,0,         0,0,         0,0);
	TEST_BINOP(timespec_add, 0,0,         1,0,         1,0);
	TEST_BINOP(timespec_add, 1,0,         0,0,         1,0);
	TEST_BINOP(timespec_add, 1,0,         1,0,         2,0);
	TEST_BINOP(timespec_add, 1,500000000, 1,0,         2,500000000);
	TEST_BINOP(timespec_add, 1,0,         1,500000000, 2,500000000);
	TEST_BINOP(timespec_add, 1,500000000, 1,500000000, 3,0);
	TEST_BINOP(timespec_add, 1,500000000, 1,499999999, 2,999999999);
	TEST_BINOP(timespec_add, 1,500000000, 1,500000000, 3,0);
	TEST_BINOP(timespec_add, 1,999999999, 1,999999999, 3,999999998);
	TEST_BINOP(timespec_add, 0,500000000, 1,500000000, 2,0);
	TEST_BINOP(timespec_add, 1,500000000, 0,500000000, 2,0);
	
	// timespec_sub
	
	TEST_BINOP(timespec_sub, 0,0,         0,0,         0,0);
	TEST_BINOP(timespec_sub, 1,0,         0,0,         1,0);
	TEST_BINOP(timespec_sub, 1,0,         1,0,         0,0);
	TEST_BINOP(timespec_sub, 1,500000000, 0,500000000, 1,0);
	TEST_BINOP(timespec_sub, 5,500000000, 2,999999999, 2,500000001);
	TEST_BINOP(timespec_sub, 0,0,         1,0,         -1,0);
	TEST_BINOP(timespec_sub, 0,500000000, 1,500000000, -1,0);
	TEST_BINOP(timespec_sub, 0,0,         1,500000000, -2,500000000);
	TEST_BINOP(timespec_sub, 1,0,         1,500000000, -1,500000000);
	TEST_BINOP(timespec_sub, 1,0,         1,499999999, -1,500000001);
	
	// timespec_mod
	
	TEST_BINOP(timespec_mod, 0,0,         0,0,         0,0);
	TEST_BINOP(timespec_mod, 0,0,         1,0,         0,0);
	TEST_BINOP(timespec_mod, 1,0,         0,0,         1,0);
	TEST_BINOP(timespec_mod, 1,0,         1,0,         0,0);
	TEST_BINOP(timespec_mod, 10,0,        1,0,         0,0);
	TEST_BINOP(timespec_mod, 10,0,        3,0,         1,0);
	TEST_BINOP(timespec_mod, 10,0,        -3,0,        -2,0);
	TEST_BINOP(timespec_mod, -10,0,       3,0,         2,0);
	TEST_BINOP(timespec_mod, -10,0,       -3,0,        -1,0);
	TEST_BINOP(timespec_mod, 10,0,        5,0,         0,0);
	TEST_BINOP(timespec_mod, 10,0,        -5,0,        0,0);
	TEST_BINOP(timespec_mod, -10,0,       5,0,         0,0);
	TEST_BINOP(timespec_mod, -10,0,       -5,0,        0,0);
	TEST_BINOP(timespec_mod, 1,500000000, 0,500000000, 0,0);
	TEST_BINOP(timespec_mod, 5,500000000, 2,999999999, 2,500000001);
	TEST_BINOP(timespec_mod, 0,500000000, 1,500000000, 0,500000000);
	TEST_BINOP(timespec_mod, 0,0,         1,500000000, 0,0);
	TEST_BINOP(timespec_mod, 1,0,         1,500000000, 1,0);
	TEST_BINOP(timespec_mod, 1,0,         0,1,         0,0);
	TEST_BINOP(timespec_mod, 1,123456789, 0,1000,      0,789);
	TEST_BINOP(timespec_mod, 1,0,         0,9999999,   0,100);
	TEST_BINOP(timespec_mod, 12345,54321, 0,100001,    0,5555);
	TEST_BINOP(timespec_mod, LONG_MAX,0,  0,1,         0,0);
	TEST_BINOP(timespec_mod, LONG_MAX,0,  LONG_MAX,1,  LONG_MAX,0);
	
	// timespec_clamp
	
	TEST_TRINOP(timespec_clamp, 0,0,    0,0,   0,0,   0,0);
	
	TEST_TRINOP(timespec_clamp, 1000,0,    2000,0,   3000,0,   2000,0);
	TEST_TRINOP(timespec_clamp, 1500,0,    2000,0,   3000,0,   2000,0);
	TEST_TRINOP(timespec_clamp, 1999,0,    2000,0,   3000,0,   2000,0);
	TEST_TRINOP(timespec_clamp, 2000,0,    2000,0,   3000,0,   2000,0);
	TEST_TRINOP(timespec_clamp, 2001,0,    2000,0,   3000,0,   2001,0);
	TEST_TRINOP(timespec_clamp, 2250,0,    2000,0,   3000,0,   2250,0);
	TEST_TRINOP(timespec_clamp, 2500,0,    2000,0,   3000,0,   2500,0);
	TEST_TRINOP(timespec_clamp, 2750,0,    2000,0,   3000,0,   2750,0);
	TEST_TRINOP(timespec_clamp, 2999,0,    2000,0,   3000,0,   2999,0);
	TEST_TRINOP(timespec_clamp, 3000,0,    2000,0,   3000,0,   3000,0);
	TEST_TRINOP(timespec_clamp, 3001,0,    2000,0,   3000,0,   3000,0);
	TEST_TRINOP(timespec_clamp, 3500,0,    2000,0,   3000,0,   3000,0);
	TEST_TRINOP(timespec_clamp, 4000,0,    2000,0,   3000,0,   3000,0);
	
	TEST_TRINOP(timespec_clamp, 0,1000,    0,2000,   0,3000,   0,2000);
	TEST_TRINOP(timespec_clamp, 0,1500,    0,2000,   0,3000,   0,2000);
	TEST_TRINOP(timespec_clamp, 0,1999,    0,2000,   0,3000,   0,2000);
	TEST_TRINOP(timespec_clamp, 0,2000,    0,2000,   0,3000,   0,2000);
	TEST_TRINOP(timespec_clamp, 0,2001,    0,2000,   0,3000,   0,2001);
	TEST_TRINOP(timespec_clamp, 0,2250,    0,2000,   0,3000,   0,2250);
	TEST_TRINOP(timespec_clamp, 0,2500,    0,2000,   0,3000,   0,2500);
	TEST_TRINOP(timespec_clamp, 0,2750,    0,2000,   0,3000,   0,2750);
	TEST_TRINOP(timespec_clamp, 0,2999,    0,2000,   0,3000,   0,2999);
	TEST_TRINOP(timespec_clamp, 0,3000,    0,2000,   0,3000,   0,3000);
	TEST_TRINOP(timespec_clamp, 0,3001,    0,2000,   0,3000,   0,3000);
	TEST_TRINOP(timespec_clamp, 0,3500,    0,2000,   0,3000,   0,3000);
	TEST_TRINOP(timespec_clamp, 0,4000,    0,2000,   0,3000,   0,3000);
	
	TEST_TRINOP(timespec_clamp,0,-1000,   0,-3000,  0,-2000,  0,-2000);
	TEST_TRINOP(timespec_clamp,0,-1500,   0,-3000,  0,-2000,  0,-2000);
	TEST_TRINOP(timespec_clamp,0,-1999,   0,-3000,  0,-2000,  0,-2000);
	TEST_TRINOP(timespec_clamp,0,-3000,   0,-3000,  0,-2000,  0,-3000);
	TEST_TRINOP(timespec_clamp,0,-2001,   0,-3000,  0,-2000,  0,-2001);
	TEST_TRINOP(timespec_clamp,0,-2250,   0,-3000,  0,-2000,  0,-2250);
	TEST_TRINOP(timespec_clamp,0,-2500,   0,-3000,  0,-2000,  0,-2500);
	TEST_TRINOP(timespec_clamp,0,-2750,   0,-3000,  0,-2000,  0,-2750);
	TEST_TRINOP(timespec_clamp,0,-2999,   0,-3000,  0,-2000,  0,-2999);
	TEST_TRINOP(timespec_clamp,0,-2000,   0,-3000,  0,-2000,  0,-2000);
	TEST_TRINOP(timespec_clamp,0,-3001,   0,-3000,  0,-2000,  0,-3000);
	TEST_TRINOP(timespec_clamp,0,-3500,   0,-3000,  0,-2000,  0,-3000);
	TEST_TRINOP(timespec_clamp,0,-2000,   0,-3000,  0,-2000,  0,-2000);
	
	TEST_TRINOP(timespec_clamp,0,-4000,   0,-3000,  0,3000,  0,-3000);
	TEST_TRINOP(timespec_clamp,0,-3001,   0,-3000,  0,3000,  0,-3000);
	TEST_TRINOP(timespec_clamp,0,-3000,   0,-3000,  0,3000,  0,-3000);
	TEST_TRINOP(timespec_clamp,0,-2999,   0,-3000,  0,3000,  0,-2999);
	TEST_TRINOP(timespec_clamp,0,-1500,   0,-3000,  0,3000,  0,-1500);
	TEST_TRINOP(timespec_clamp,0,   -1,   0,-3000,  0,3000,  0,   -1);
	TEST_TRINOP(timespec_clamp,0,    0,   0,-3000,  0,3000,  0,    0);
	TEST_TRINOP(timespec_clamp,0,    1,   0,-3000,  0,3000,  0,    1);
	TEST_TRINOP(timespec_clamp,0, 1500,   0,-3000,  0,3000,  0, 1500);
	TEST_TRINOP(timespec_clamp,0, 2999,   0,-3000,  0,3000,  0, 2999);
	TEST_TRINOP(timespec_clamp,0, 3000,   0,-3000,  0,3000,  0, 3000);
	TEST_TRINOP(timespec_clamp,0, 3001,   0,-3000,  0,3000,  0, 3000);
	TEST_TRINOP(timespec_clamp,0, 4000,   0,-3000,  0,3000,  0, 3000);
	
	// timespec_min
	
	TEST_BINOP(timespec_min, 0,0,	      0,0,	   0,0);
	TEST_BINOP(timespec_min, 0,0,	      1,0,	   0,0);
	TEST_BINOP(timespec_min, 1,0,	      0,0,	   0,0);
	TEST_BINOP(timespec_min, 1,0,	      1,0,	   1,0);
	TEST_BINOP(timespec_min, 10,0,	      1,0,	   1,0);
	TEST_BINOP(timespec_min, 10,0,	      3,0,	   3,0);
	TEST_BINOP(timespec_min, 10,0,	      -3,0,	   -3,0);
	TEST_BINOP(timespec_min, -10,0,	      3,0,	   -10,0);
	TEST_BINOP(timespec_min, -10,0,	      -3,0,	   -10,0);
	TEST_BINOP(timespec_min, 10,0,	      5,0,	   5,0);
	TEST_BINOP(timespec_min, 10,0,	      -5,0,	   -5,0);
	TEST_BINOP(timespec_min, -10,0,	      5,0,	   -10,0);
	TEST_BINOP(timespec_min, -10,0,	      -5,0,	   -10,0);
	TEST_BINOP(timespec_min, 1,500000000, 0,500000000, 0,500000000);
	TEST_BINOP(timespec_min, 5,500000000, 2,999999999, 2,999999999);
	TEST_BINOP(timespec_min, 0,500000000, 1,500000000, 0,500000000);
	TEST_BINOP(timespec_min, 0,0,	      1,500000000, 0,0);
	TEST_BINOP(timespec_min, 1,0,	      1,500000000, 1,0);
	TEST_BINOP(timespec_min, 1,0,	      0,1,	   0,1);
	TEST_BINOP(timespec_min, 1,123456789, 0,1000,	   0,1000);
	TEST_BINOP(timespec_min, 1,0,	      0,9999999,   0,9999999);
	TEST_BINOP(timespec_min, 12345,54321, 0,100001,	   0,100001);
	TEST_BINOP(timespec_min, LONG_MIN,0,  0,1,	   LONG_MIN,0);
	TEST_BINOP(timespec_min, LONG_MIN,0,  0,-1,	   LONG_MIN,0);
	TEST_BINOP(timespec_min, LONG_MIN,0,  LONG_MAX,0,  LONG_MIN,0);
	TEST_BINOP(timespec_min, LONG_MIN,0,  LONG_MIN,0,  LONG_MIN,0);
	TEST_BINOP(timespec_min, LONG_MAX,0,  0,1,	   0,1);
	TEST_BINOP(timespec_min, LONG_MAX,0,  0,-1,	   0,-1);
	TEST_BINOP(timespec_min, LONG_MAX,0,  LONG_MAX,0,  LONG_MAX,0);
	TEST_BINOP(timespec_min, LONG_MAX,0,  LONG_MIN,0,  LONG_MIN,0);
	
	// timespec_max
	
	TEST_BINOP(timespec_max, 0,0,	      0,0,	   0,0);
	TEST_BINOP(timespec_max, 0,0,	      1,0,	   1,0);
	TEST_BINOP(timespec_max, 1,0,	      0,0,	   1,0);
	TEST_BINOP(timespec_max, 1,0,	      1,0,	   1,0);
	TEST_BINOP(timespec_max, 10,0,	      1,0,	   10,0);
	TEST_BINOP(timespec_max, 10,0,	      3,0,	   10,0);
	TEST_BINOP(timespec_max, 10,0,	      -3,0,	   10,0);
	TEST_BINOP(timespec_max, -10,0,	      3,0,	   3,0);
	TEST_BINOP(timespec_max, -10,0,	      -3,0,	   -3,0);
	TEST_BINOP(timespec_max, 10,0,	      5,0,	   10,0);
	TEST_BINOP(timespec_max, 10,0,	      -5,0,	   10,0);
	TEST_BINOP(timespec_max, -10,0,	      5,0,	   5,0);
	TEST_BINOP(timespec_max, -10,0,	      -5,0,	   -5,0);
	TEST_BINOP(timespec_max, 1,500000000, 0,500000000, 1,500000000);
	TEST_BINOP(timespec_max, 5,500000000, 2,999999999, 5,500000000);
	TEST_BINOP(timespec_max, 0,500000000, 1,500000000, 1,500000000);
	TEST_BINOP(timespec_max, 0,0,	      1,500000000, 1,500000000);
	TEST_BINOP(timespec_max, 1,0,	      1,500000000, 1,500000000);
	TEST_BINOP(timespec_max, 1,0,	      0,1,	   1,0);
	TEST_BINOP(timespec_max, 1,123456789, 0,1000,	   1,123456789);
	TEST_BINOP(timespec_max, 1,0,	      0,9999999,   1,0);
	TEST_BINOP(timespec_max, 12345,54321, 0,100001,	   12345,54321);
	TEST_BINOP(timespec_max, LONG_MIN,0,  0,1,	   0,1);
	TEST_BINOP(timespec_max, LONG_MIN,0,  0,-1,	   0,-1);
	TEST_BINOP(timespec_max, LONG_MIN,0,  LONG_MAX,0,  LONG_MAX,0);
	TEST_BINOP(timespec_max, LONG_MIN,0,  LONG_MIN,0,  LONG_MIN,0);
	TEST_BINOP(timespec_max, LONG_MAX,0,  0,1,	   LONG_MAX,0);
	TEST_BINOP(timespec_max, LONG_MAX,0,  0,-1,	   LONG_MAX,0);
	TEST_BINOP(timespec_max, LONG_MAX,0,  LONG_MAX,0,  LONG_MAX,0);
	TEST_BINOP(timespec_max, LONG_MAX,0,  LONG_MIN,0,  LONG_MAX,0);
	
	// timespec_cmp
	
	TEST_TEST_FUNC(timespec_cmp,    0,0,    0,0, 0);
	TEST_TEST_FUNC(timespec_cmp,  100,0,  100,0, 0);
	TEST_TEST_FUNC(timespec_cmp, -100,0, -100,0, 0);
	
	TEST_TEST_FUNC(timespec_cmp,    1,0,    0,0,  1);
	TEST_TEST_FUNC(timespec_cmp,    0,0,    1,0, -1);
	TEST_TEST_FUNC(timespec_cmp,    0,1,    0,0,  1);
	TEST_TEST_FUNC(timespec_cmp,    0,0,    0,1, -1);
	TEST_TEST_FUNC(timespec_cmp,    1,0,  0,100,  1);
	TEST_TEST_FUNC(timespec_cmp,  0,100   , 1,0, -1);
	
	TEST_TEST_FUNC(timespec_cmp,          -0,-0,            0,0,  0);
	TEST_TEST_FUNC(timespec_cmp, -10,-500000000,  -11,500000000,  0);
	TEST_TEST_FUNC(timespec_cmp, -10,-500000001,  -11,499999999,  0);
	TEST_TEST_FUNC(timespec_cmp, -10,-500000001,  -11,500000001, -1);
	TEST_TEST_FUNC(timespec_cmp,  -11,500000001, -10,-500000001,  1);
	
	// timespec_eq
	
	TEST_TEST_FUNC(timespec_eq, 0,0,    0,0,    true);
	TEST_TEST_FUNC(timespec_eq, 100,0,  100,0,  true);
	TEST_TEST_FUNC(timespec_eq, -200,0, -200,0, true);
	TEST_TEST_FUNC(timespec_eq, 0,300,  0,300,  true);
	TEST_TEST_FUNC(timespec_eq, 0,-400, 0,-400, true);
	
	TEST_TEST_FUNC(timespec_eq, 100,1,  100,0,  false);
	TEST_TEST_FUNC(timespec_eq, 101,0,  100,0,  false);
	TEST_TEST_FUNC(timespec_eq, -100,0, 100,0,  false);
	TEST_TEST_FUNC(timespec_eq, 0,10,   0,-10,  false);
	
	TEST_TEST_FUNC(timespec_eq,          -0,-0,           0,0,  true);
	TEST_TEST_FUNC(timespec_eq, -10,-500000000, -11,500000000,  true);
	TEST_TEST_FUNC(timespec_eq, -10,-500000001, -11,499999999,  true);
	TEST_TEST_FUNC(timespec_eq, -10,-500000001, -11,500000001,  false);
	
	// timespec_gt
	
	TEST_TEST_FUNC(timespec_gt, 1,0, 0,0,  true);
	TEST_TEST_FUNC(timespec_gt, 0,0, -1,0, true);
	TEST_TEST_FUNC(timespec_gt, 0,1, 0,0,  true);
	TEST_TEST_FUNC(timespec_gt, 0,0, 0,-1, true);
	
	TEST_TEST_FUNC(timespec_gt, 1,0,  1,0, false);
	TEST_TEST_FUNC(timespec_gt, 1,1,  1,1, false);
	TEST_TEST_FUNC(timespec_gt, -1,0, 0,0, false);
	TEST_TEST_FUNC(timespec_gt, 0,-1, 0,0, false);
	
	TEST_TEST_FUNC(timespec_gt,            0,0,           -0,-0,  false);
	TEST_TEST_FUNC(timespec_gt, -10,-500000000,   -11,500000000,  false);
	TEST_TEST_FUNC(timespec_gt,  -11,500000000,  -10,-500000000,  false);
	TEST_TEST_FUNC(timespec_gt, -10,-500000001,   -11,499999999,  false);
	TEST_TEST_FUNC(timespec_gt,  -11,499999999,   -11,499999999,  false);
	TEST_TEST_FUNC(timespec_gt, -10,-500000001,   -11,500000001,  false);
	TEST_TEST_FUNC(timespec_gt,  -11,500000001,  -10,-500000001,  true);
	
	// timespec_ge
	
	TEST_TEST_FUNC(timespec_ge, 1,0, 0,0,  true);
	TEST_TEST_FUNC(timespec_ge, 0,0, -1,0, true);
	TEST_TEST_FUNC(timespec_ge, 0,1, 0,0,  true);
	TEST_TEST_FUNC(timespec_ge, 0,0, 0,-1, true);
	TEST_TEST_FUNC(timespec_ge, 1,0,  1,0, true);
	TEST_TEST_FUNC(timespec_ge, 1,1,  1,1, true);
	
	TEST_TEST_FUNC(timespec_ge, -1,0, 0,0, false);
	TEST_TEST_FUNC(timespec_ge, 0,-1, 0,0, false);
	
	TEST_TEST_FUNC(timespec_ge,            0,0,           -0,-0,  true);
	TEST_TEST_FUNC(timespec_ge, -10,-500000000,   -11,500000000,  true);
	TEST_TEST_FUNC(timespec_ge,  -11,500000000,  -10,-500000000,  true);
	TEST_TEST_FUNC(timespec_ge, -10,-500000001,   -11,499999999,  true);
	TEST_TEST_FUNC(timespec_ge,  -11,499999999,   -11,499999999,  true);
	TEST_TEST_FUNC(timespec_ge, -10,-500000001,   -11,500000001,  false);
	TEST_TEST_FUNC(timespec_ge,  -11,500000001,  -10,-500000001,  true);
	
	// timespec_lt
	
	TEST_TEST_FUNC(timespec_lt, 0,0,  1,0, true);
	TEST_TEST_FUNC(timespec_lt, -1,0, 0,0, true);
	TEST_TEST_FUNC(timespec_lt, 0,0,  0,1, true);
	TEST_TEST_FUNC(timespec_lt, 0,-1, 0,0, true);
	
	TEST_TEST_FUNC(timespec_lt, 1,0, 1,0,  false);
	TEST_TEST_FUNC(timespec_lt, 1,1, 1,1,  false);
	TEST_TEST_FUNC(timespec_lt, 0,0, -1,0, false);
	TEST_TEST_FUNC(timespec_lt, 0,0, 0,-1, false);
	
	TEST_TEST_FUNC(timespec_lt,            0,0,           -0,-0,  false);
	TEST_TEST_FUNC(timespec_lt, -10,-500000000,   -11,500000000,  false);
	TEST_TEST_FUNC(timespec_lt,  -11,500000000,  -10,-500000000,  false);
	TEST_TEST_FUNC(timespec_lt, -10,-500000001,   -11,499999999,  false);
	TEST_TEST_FUNC(timespec_lt,  -11,499999999,   -11,499999999,  false);
	TEST_TEST_FUNC(timespec_lt, -10,-500000001,   -11,500000001,  true);
	TEST_TEST_FUNC(timespec_lt,  -11,500000001,  -10,-500000001,  false);
	
	// timespec_le
	
	TEST_TEST_FUNC(timespec_le, 0,0, 1,0,  true);
	TEST_TEST_FUNC(timespec_le, -1,0, 0,0, true);
	TEST_TEST_FUNC(timespec_le, 0,0, 0,1,  true);
	TEST_TEST_FUNC(timespec_le, 0,-1, 0,0, true);
	TEST_TEST_FUNC(timespec_le, 1,0,  1,0, true);
	TEST_TEST_FUNC(timespec_le, 1,1,  1,1, true);
	
	TEST_TEST_FUNC(timespec_le, 0,0, -1,0, false);
	TEST_TEST_FUNC(timespec_le, 0,0, 0,-1, false);
	
	TEST_TEST_FUNC(timespec_le,            0,0,           -0,-0,  true);
	TEST_TEST_FUNC(timespec_le, -10,-500000000,   -11,500000000,  true);
	TEST_TEST_FUNC(timespec_le,  -11,500000000,  -10,-500000000,  true);
	TEST_TEST_FUNC(timespec_le, -10,-500000001,   -11,499999999,  true);
	TEST_TEST_FUNC(timespec_le,  -11,499999999,   -11,499999999,  true);
	TEST_TEST_FUNC(timespec_le, -10,-500000001,   -11,500000001,  true);
	TEST_TEST_FUNC(timespec_le,  -11,500000001,  -10,-500000001,  false);
	
	// timespec_from_double
	
	TEST_FROM_DOUBLE(0.0,   0,0);
	TEST_FROM_DOUBLE(10.0,  10,0);
	TEST_FROM_DOUBLE(-10.0, -10,0);
	TEST_FROM_DOUBLE(0.5,   0,500000000);
	TEST_FROM_DOUBLE(-0.5,  -1,500000000);
	TEST_FROM_DOUBLE(10.5,  10,500000000);
	TEST_FROM_DOUBLE(-10.5, -11,500000000);
	
	// timespec_to_double
	
	TEST_TO_DOUBLE(0,0,            0.0);
	TEST_TO_DOUBLE(10,0,           10.0);
	TEST_TO_DOUBLE(-10,0,          -10.0);
	TEST_TO_DOUBLE(0,500000000,    0.5);
	TEST_TO_DOUBLE(0,-500000000,   -0.5);
	TEST_TO_DOUBLE(10,500000000,   10.5);
	TEST_TO_DOUBLE(10,-500000000,  9.5);
	TEST_TO_DOUBLE(-10,500000000,  -9.5);
	TEST_TO_DOUBLE(-10,-500000000, -10.5);
	
	// timespec_from_timeval
	
	TEST_FROM_TIMEVAL(0,0,     0,0);
	TEST_FROM_TIMEVAL(1,0,     1,0);
	TEST_FROM_TIMEVAL(1000,0,  1000,0);
	TEST_FROM_TIMEVAL(0,0,     0,0);
	TEST_FROM_TIMEVAL(-1,0,    -1,0);
	TEST_FROM_TIMEVAL(-1000,0, -1000,0);
	
	TEST_FROM_TIMEVAL(1,1,      1,1000);
	TEST_FROM_TIMEVAL(1,1000,   1,1000000);
	TEST_FROM_TIMEVAL(1,-1,     0,999999000);
	TEST_FROM_TIMEVAL(1,-1000,  0,999000000);
	TEST_FROM_TIMEVAL(-1,-1,    -2,999999000);
	TEST_FROM_TIMEVAL(-1,-1000, -2,999000000);
	
	// timespec_to_timeval
	
	TEST_TO_TIMEVAL(0,0,   0,0);
	TEST_TO_TIMEVAL(1,0,   1,0);
	TEST_TO_TIMEVAL(10,0,  10,0);
	TEST_TO_TIMEVAL(-1,0,  -1,0);
	TEST_TO_TIMEVAL(-10,0, -10,0);
	
	TEST_TO_TIMEVAL(1,1,       1,0);
	TEST_TO_TIMEVAL(1,999,     1,0);
	TEST_TO_TIMEVAL(1,1000,    1,1);
	TEST_TO_TIMEVAL(1,1001,    1,1);
	TEST_TO_TIMEVAL(1,2000,    1,2);
	TEST_TO_TIMEVAL(1,2000000, 1,2000);
	
	TEST_TO_TIMEVAL(1,-1,       0,999999);
	TEST_TO_TIMEVAL(1,-999,     0,999999);
	TEST_TO_TIMEVAL(1,-1000,    0,999999);
	TEST_TO_TIMEVAL(1,-1001,    0,999998);
	TEST_TO_TIMEVAL(1,-2000,    0,999998);
	TEST_TO_TIMEVAL(1,-2000000, 0,998000);
	
	TEST_TO_TIMEVAL(-1,-1,       -2,999999);
	TEST_TO_TIMEVAL(-1,-999,     -2,999999);
	TEST_TO_TIMEVAL(-1,-1000,    -2,999999);
	TEST_TO_TIMEVAL(-1,-1001,    -2,999998);
	TEST_TO_TIMEVAL(-1,-2000,    -2,999998);
	TEST_TO_TIMEVAL(-1,-2000000, -2,998000);
	
	TEST_TO_TIMEVAL(1,1500000000,   2,500000);
	TEST_TO_TIMEVAL(1,-1500000000,  -1,500000);
	TEST_TO_TIMEVAL(-1,-1500000000, -3,500000);
	
	// timespec_from_ms
	
	TEST_FROM_MS(0,     0,0);
	TEST_FROM_MS(1,     0,1000000);
	TEST_FROM_MS(-1,    -1,999000000);
	TEST_FROM_MS(1500,  1,500000000);
	TEST_FROM_MS(-1000, -1,0);
	TEST_FROM_MS(-1500, -2,500000000);
	
	// timespec_to_ms
	
	TEST_TO_MS(0,0,            0);
	TEST_TO_MS(10,0,           10000);
	TEST_TO_MS(-10,0,          -10000);
	TEST_TO_MS(0,500000000,    500);
	TEST_TO_MS(0,-500000000,   -500);
	TEST_TO_MS(10,500000000,   10500);
	TEST_TO_MS(10,-500000000,  9500);
	TEST_TO_MS(-10,500000000,  -9500);
	TEST_TO_MS(-10,-500000000, -10500);
	
	// timespec_normalise
	
	TEST_NORMALISE(0,0,           0,0);
	
	TEST_NORMALISE(0,1000000000,  1,0);
	TEST_NORMALISE(0,1500000000,  1,500000000);
	TEST_NORMALISE(0,-1000000000, -1,0);
	TEST_NORMALISE(0,-1500000000, -2,500000000);
	
	TEST_NORMALISE(5,1000000000,   6,0);
	TEST_NORMALISE(5,1500000000,   6,500000000);
	TEST_NORMALISE(-5,-1000000000, -6,0);
	TEST_NORMALISE(-5,-1500000000, -7,500000000);
	
	TEST_NORMALISE(0,2000000000,  2,0);
	TEST_NORMALISE(0,2100000000,  2,100000000);
	TEST_NORMALISE(0,-2000000000, -2,0);
	TEST_NORMALISE(0,-2100000000, -3,900000000);
	
	TEST_NORMALISE(1,-500000001,  0,499999999);
	TEST_NORMALISE(1,-500000000,  0,500000000);
	TEST_NORMALISE(1,-499999999,  0,500000001);
	TEST_NORMALISE(0,-499999999,  -1,500000001);
	
	TEST_NORMALISE(-1,500000000,  -1,500000000);
	TEST_NORMALISE(-1,499999999,  -1,499999999);
	
	if(result > 0)
	{
		printf("%d tests failed\n", result);
	}
	else{
		printf("All tests passed\n");
	}
	
	return !!result; /* Don't overflow the exit status */
}
#endif
