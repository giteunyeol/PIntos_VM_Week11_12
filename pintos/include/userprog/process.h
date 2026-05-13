#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

struct page_lazy_load_aux {
    struct file *file;
    off_t ofs;
    size_t read_bytes;
    size_t zero_bytes;
};

struct child_status {
    tid_t tid;                     /* 자식 스레드 tid */
    int exit_status;               /* 자식이 exit()할 때 남긴 종료 코드 */
    bool exited;                   /* 자식이 종료했는지 여부 */
    bool waited;                   /* 부모가 자식에 대해서 wait() 했는지 여부 */
    int ref_cnt;
    struct lock lock;
    struct semaphore wait_sema;    /* 부모가 자식 종료를 기다릴 때 사용하는 세마포어 */
    struct list_elem elem;		   /* child list 리스트 노드 */
};


tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

#endif /* userprog/process.h */
