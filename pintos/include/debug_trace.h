#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#include <debug.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#ifndef DEG_TRACE_ENABLED
#define DEG_TRACE_ENABLED 0 
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

/*
   DEG trace 사용법

   켜기/끄기:
    - DEG_TRACE_ENABLED 값을 1로 바꾸면 출력하고, 0이면 아무것도 출력하지 않는다.
    - 내부 분기에서 막는 방식이라 호출부의 DEG_* 코드를 지우지 않아도 된다.

   공통 출력 형식:
      [DEG] r=thrd tid= 3 p4=0x8004246000 fn=load         k=call   ...

   공통 필드:
    - r: 실행 문맥. thrd는 스레드 문맥, intr는 인터럽트 문맥, none은 현재
      스택에서 유효한 thread 구조체를 찾지 못한 경우다.
    - tid: 현재 스레드 tid. 스레드가 없으면 TID_ERROR가 나온다.
    - p4: 현재 스레드의 pml4. USERPROG가 아니거나 스레드가 없으면 0이다.
    - fn: 매크로를 호출한 C 함수 이름. 12글자까지만 보여주고 길면 잘린다.
    - k: 로그 종류. 6글자 폭으로 출력되므로 호출자가 정하는 kind는 짧게 쓴다.

   DEG_CALL(fmt, ...):
    - 함수에 들어온 직후 인자를 확인할 때 쓴다.
    - 출력 본문은 args={...} 형태다.
    예:

      DEG_CALL ("file_name=\"%s\" if_=%p", file_name, (void *) if_);
      DEG_CALL ("file=%p writable=%d", (void *) file, writable);

   DEG_NOTE(kind, fmt, ...):
    - 특정 지점에서 임시 값, goto 전후 상태, 계산 중간값을 대충 확인할 때 쓴다.
    - kind는 호출자가 정한다. 6글자 폭이므로 "goto", "chk", "tmp"처럼 짧게 쓴다.
    - 출력 본문은 vals={...} 형태다. 함수 인자가 아니라 관찰값이라는 뜻이다.
    예:

      DEG_NOTE ("goto", "success=%d file=%p", success, (void *) file);
      DEG_NOTE ("chk", "argc=%d argv0=\"%s\"", argc, argv[0]);

   DEG_BRANCH(expr, value):
    - if/switch 같은 분기로 들어가기 전에 조건식이 어떤 값인지 확인할 때 쓴다.
    - expr에는 사람이 읽을 조건 문자열을 넣고, value에는 실제 평가값을 넣는다.
    예:

      DEG_BRANCH ("!success", !success);
      if (!success) { ... }

      DEG_BRANCH ("new_pml4 != NULL", new_pml4 != NULL);
      if (new_pml4 != NULL) { ... }

   DEG_LOOP_START(expr, fmt, ...):
    - while/for 루프에 들어가기 직전 초기 상태를 찍는다.
    - 루프 조건과 시작 값을 같이 남기면 반복이 왜 시작됐는지 보기 쉽다.
    예:

      DEG_LOOP_START ("i < argc", "i=%d argc=%d", i, argc);
      DEG_LOOP_START ("read_bytes > 0 || zero_bytes > 0",
              "read_bytes=%u zero_bytes=%u", read_bytes, zero_bytes);

   DEG_LOOP(expr, fmt, ...):
    - 루프 본문 맨 앞에서 현재 반복 상태를 찍는다.
    - 인덱스가 꼭 필요한 매크로가 아니다. 바뀌는 값을 fmt에 직접 넣는다.
    예:

      DEG_LOOP ("i < argc", "i=%d argv=\"%s\"", i, argv[i]);
      DEG_LOOP ("read_bytes > 0 || zero_bytes > 0",
              "upage=%p read_bytes=%u", (void *) upage, read_bytes);

   DEG_LOOP_END(expr, fmt, ...):
    - 루프가 끝난 직후 최종 상태를 찍는다.
    - 루프 조건 값이 0이 된 이유나 누적 결과를 확인할 때 쓴다.
    예:

      DEG_LOOP_END ("i < argc", "i=%d argc=%d", i, argc);
      DEG_LOOP_END ("read_bytes > 0 || zero_bytes > 0",
              "upage=%p read_bytes=%u zero_bytes=%u",
              (void *) upage, read_bytes, zero_bytes);

   DEG_RETURN(fmt, ...):
    - return 직전에 반환값과 중요한 상태를 찍는다.
    - early return 경로마다 찍어두면 어디서 빠져나갔는지 grep하기 쉽다.
    예:

      DEG_RETURN ("value=-1");
      DEG_RETURN ("value=%d success=%d", result, success);

   DEG_BACKTRACE():
    - 현재 호출 스택이 필요할 때만 쓴다. 출력량이 많으므로 좁은 위치에 잠깐 둔다.
    예:

      DEG_NOTE ("bt", "before pml4_destroy pml4=%p", (void *) pml4);
      DEG_BACKTRACE ();
*/

#define DEG_LOG(kind_, fmt_, ...)                                               \
	do {                                                                        \
		if (deg_trace_enabled ()) {                                             \
			struct thread *deg_t_ = deg_trace_thread ();                        \
			printf ("[DEG] r=%-4.4s tid=%2d p4=%12p fn=%-12.12s k=%-6.6s "      \
					fmt_ "\n", deg_trace_run (deg_t_),                          \
					deg_t_ != NULL ? deg_t_->tid : TID_ERROR,                   \
					deg_trace_pml4 (deg_t_), __func__, kind_, ##__VA_ARGS__);   \
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
