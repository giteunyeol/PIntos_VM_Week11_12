#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/off_t.h"

struct page_lazy_load_aux {
    struct file *file;
    off_t ofs;
    size_t read_bytes;
    size_t zero_bytes;
    size_t is_first_page;
};

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);


bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
            uint32_t read_bytes, uint32_t zero_bytes, bool writable);

#endif /* userprog/process.h */
