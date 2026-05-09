#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#include <debug.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#ifndef DEG_TRACE_ENABLED
#define DEG_TRACE_ENABLED 1
#endif

#define DEG_THREAD_MAGIC 0xcd6abf4b

static inline int
deg_trace_enabled (void) {
	return DEG_TRACE_ENABLED;
}

static inline uint64_t
deg_trace_rsp (void) {
	uint64_t rsp;
	__asm __volatile("movq %%rsp, %0" : "=g"(rsp));
	return rsp;
}

static inline struct thread *
deg_trace_thread (void) {
	struct thread *t = (struct thread *) pg_round_down (deg_trace_rsp ());
	return t != NULL && t->magic == DEG_THREAD_MAGIC ? t : NULL;
}

static inline void *
deg_trace_pml4 (struct thread *t) {
#ifdef USERPROG
	return t != NULL ? (void *) t->pml4 : NULL;
#else
	(void) t;
	return NULL;
#endif
}

static inline const char *
deg_trace_run (struct thread *t) {
	if (intr_context ())
		return "intr";
	if (t != NULL)
		return "thrd";
	return "none";
}

#define DEG_LOG(kind_, fmt_, ...)                                               \
	do {                                                                        \
		if (deg_trace_enabled ()) {                                             \
			struct thread *deg_t_ = deg_trace_thread ();                        \
			printf ("[DEG] r=%-4.4s tid=%2d p4=%12p k=%-6.6s fn=%-12.12s "      \
					fmt_ "\n", deg_trace_run (deg_t_),                          \
					deg_t_ != NULL ? deg_t_->tid : TID_ERROR,                   \
					deg_trace_pml4 (deg_t_), kind_, __func__, ##__VA_ARGS__);   \
		}                                                                       \
	} while (0)

#define DEG_CALL(fmt_, ...)                                                     \
	DEG_LOG ("call", "args={" fmt_ "}", ##__VA_ARGS__)

#define DEG_NOTE(kind_, fmt_, ...)                                              \
	DEG_LOG (kind_, "vals={" fmt_ "}", ##__VA_ARGS__)

#define DEG_BRANCH(expr_, value_)                                               \
	DEG_LOG ("branch", "expr=\"%s\" value=%d", expr_, (int) (value_))

#define DEG_LOOP_START(expr_, fmt_, ...)                                        \
	DEG_LOG ("loop.s", "expr=\"%s\" " fmt_, expr_, ##__VA_ARGS__)

#define DEG_LOOP(expr_, fmt_, ...)                                              \
	DEG_LOG ("loop", "expr=\"%s\" " fmt_, expr_, ##__VA_ARGS__)

#define DEG_LOOP_END(expr_, fmt_, ...)                                          \
	DEG_LOG ("loop.e", "expr=\"%s\" " fmt_, expr_, ##__VA_ARGS__)

#define DEG_RETURN(fmt_, ...)                                                   \
	DEG_LOG ("return", fmt_, ##__VA_ARGS__)

#define DEG_BACKTRACE()                                                         \
	do {                                                                        \
		if (deg_trace_enabled ()) {                                             \
			debug_backtrace ();                                                 \
		}                                                                       \
	} while (0)

#endif /* debug_trace.h */
