/* hw_log.h — macro-gated tracing for the hardware layer (IHwLayer).
 *
 * JD's doctrine (formalized 2026-10-24): extensive tracing, ON in debug
 * builds, OFF in release, zero CPU cost in release numbers. Off means the
 * macro expands to nothing: arguments are not evaluated, no strings are
 * emitted, no call is made.
 *
 *   LITHOHW_TRACE_ENABLED  1 when NDEBUG is not defined (debug), else 0.
 *                          Define it yourself (0/1) to override either way.
 *   LITHOHW_TRACE(fmt, ...) one line to stderr, prefixed "[hw] ".
 *
 * Trace points: lifecycle transitions, surface creation, input-sink flow,
 * present calls. Loud boot errors stay loud: errors are NOT routed
 * through this macro and print in every build.
 */
#pragma once

#ifndef LITHOHW_TRACE_ENABLED
#ifdef NDEBUG
#define LITHOHW_TRACE_ENABLED 0
#else
#define LITHOHW_TRACE_ENABLED 1
#endif
#endif

#if LITHOHW_TRACE_ENABLED
#include <stdio.h>
#define LITHOHW_TRACE(...) \
  do { fputs("[hw] ", stderr); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)
#else
#define LITHOHW_TRACE(...) ((void)0)
#endif