#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/palloc.h"
#include "threads/mmu.h"
#include "intrinsic.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "threads/malloc.h"
#include "debug_trace.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

static bool copy_in_string (char *buf, const char *command, size_t size);
static struct lock filesys_lock;
static struct fd_entry *find_fd_entry(int fd);
static void validate_user_ptr(const void *ptr);
static void validate_user_buffer(const void *buffer, size_t size);
static void validate_user_string(const char *str);
static void kill_process_due_to_bad_user_memory(void);

/* 시스템 호출.
 *
 * 예전에는 시스템 호출 서비스를 인터럽트 핸들러가 처리했다
 * (예: Linux의 int 0x80). 하지만 x86-64에서는 CPU 제조사가 시스템 호출을
 * 요청하는 더 효율적인 경로로 `syscall` 명령어를 제공한다.
 *
 * syscall 명령어는 모델 전용 레지스터(MSR)에 저장된 값을 읽어 동작한다.
 * 자세한 내용은 매뉴얼을 참고하라. */

#define MSR_STAR 0xc0000081         /* 세그먼트 셀렉터 MSR */
#define MSR_LSTAR 0xc0000082        /* 롱 모드 SYSCALL 대상 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크 */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* syscall_entry가 사용자 영역 스택을 커널 모드 스택으로 바꾸기 전까지는
	 * 인터럽트 서비스 루틴이 어떤 인터럽트도 처리하면 안 된다. 그래서
	 * FLAG_FL을 마스킹한다. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	lock_init(&filesys_lock);
}


/* user가 요청한 fd를 읽어 buffer에 내용을 저장 */
int read(int fd, void *buffer, unsigned size) {
	DEG_CALL ("fd=%d buffer=%p size=%u", fd, buffer, size);
	DEG_NOTE ("range", "start=%p end=%p start_page=%p end_page=%p",
			buffer, size > 0 ? (uint8_t *) buffer + size - 1 : buffer,
			pg_round_down(buffer),
			size > 0 ? pg_round_down((uint8_t *) buffer + size - 1) : pg_round_down(buffer));
	/* 인자 기본 검사 */
	if (fd < 0) {
		DEG_RETURN ("value=-1 reason=bad-fd fd=%d", fd);
		return -1;
	}

	if (fd == 1) { 	// fd == 1: 실패
		DEG_RETURN ("value=-1 reason=stdout-read");
		return -1;
	}
	// size 검사
		if (size == 0) {
		DEG_RETURN ("value=0 reason=zero-size");
		return 0;
	}

	validate_user_buffer(buffer, size);
	DEG_NOTE ("valid", "buffer=%p size=%u passed validation", buffer, size);

	/* 실제 read를 수행 */
	size_t i;
	char *buf = buffer;

	if (fd == 0) { // stdin이면 input_getc로 buffer에 size만큼 쓰기
		for (i=0; i<size; i++) {
			buf[i] = input_getc();
		}
		DEG_RETURN ("value=%u reason=stdin", size);
		return size;
	}
	if (fd >= 2) {
		struct fd_entry *entry = find_fd_entry(fd); // file이면 fd로 entry를 찾는다
		if (entry == NULL || entry->file == NULL) {
			DEG_RETURN ("value=-1 reason=no-entry fd=%d", fd);
			return -1;
		}
		DEG_NOTE ("file", "before file_read fd=%d file=%p buffer=%p size=%u",
				fd, entry->file, buffer, size);
		lock_acquire(&filesys_lock); // file이면 filesys lock 획득 후 file_read
		off_t read_size = file_read(entry->file, buffer, size);
		lock_release(&filesys_lock); // file이면 filesys lock 해제

		DEG_RETURN ("value=%d reason=file-read fd=%d buffer=%p size=%u",
				(int) read_size, fd, buffer, size);
		return read_size; // 읽은 바이트 수 반환
	}
	DEG_RETURN ("value=-1 reason=unhandled-fd fd=%d", fd);
	return -1;
}

/* fd를 받아 fd_entry를 찾아서 반환한다 */
static struct fd_entry *
find_fd_entry(int fd) {
	struct thread *curr = thread_current();
	struct list_elem *e;

	for (e = list_begin(&curr->fd_list);
		 e != list_end(&curr->fd_list);
		 e = list_next(e)) {
		struct fd_entry *entry = list_entry(e, struct fd_entry, elem);

		if (entry->fd == fd) {
			return entry;
		}
	}
	return NULL;
}


/* 메인 시스템 호출 인터페이스 */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.
	thread_current()->saved_user_rsp = f->rsp;

	struct thread *t = thread_current();
	DEG_CALL ("sysno=%llu rdi=%p rsi=%p rdx=%p thread=%p",
			(unsigned long long) f->R.rax,
			(void *) f->R.rdi, (void *) f->R.rsi, (void *) f->R.rdx, t);

	switch (f->R.rax)
	{
	case SYS_EXIT: {
		uint64_t status = f->R.rdi;
		DEG_NOTE ("exit", "status=%d", (int) status);
		t->exit_status = status;
		thread_exit ();
		break;
	}

	case SYS_CREATE: {
		const char *file = (const char *) f->R.rdi;
		unsigned size = f->R.rsi;
		validate_user_string(file);
		lock_acquire(&filesys_lock);
		bool result = filesys_create(file, size);
		lock_release(&filesys_lock);
		f->R.rax = result;
		break;
	}

	case SYS_OPEN: {
		const char *file = (const char *) f->R.rdi;
		validate_user_string(file);
		lock_acquire(&filesys_lock);
		struct file *opened = filesys_open(file);
		lock_release(&filesys_lock);
		if (opened == NULL) {
			f->R.rax = -1;
			break;
		}
		struct fd_entry *entry = malloc(sizeof *entry);
		if (entry == NULL) {
			file_close(opened);
			f->R.rax = -1;
			break;
		}
		entry->fd = t->next_fd;
		entry->file = opened;
		t->next_fd ++;
		list_push_back(&t->fd_list, &entry->elem);
		f->R.rax = entry->fd;
		break;

	}

	case SYS_FILESIZE: {
		int fd = (int) f->R.rdi;
		struct fd_entry *entry = find_fd_entry(fd);
		if (entry == NULL || entry->file == NULL) {
			f->R.rax = -1;
			break;
		}
		lock_acquire(&filesys_lock);
		f->R.rax = file_length(entry->file);
		lock_release(&filesys_lock);
		break;
	}

	case SYS_WRITE: {
		int fd = (int) f->R.rdi;
		const char *buffer = (const void *) f->R.rsi;
		size_t size = (size_t) f->R.rdx;
		DEG_NOTE ("write", "fd=%d buffer=%p size=%d", fd, buffer, (int) size);

		if (size == 0) {
			f->R.rax = 0;
			DEG_NOTE ("write", "return=0 reason=zero-size");
			break;
		}

		if (fd == 1) {
			validate_user_buffer(buffer, size);
			putbuf(buffer, size);
			f->R.rax = size;
			DEG_NOTE ("write", "return=%d reason=stdout", (int) f->R.rax);
		} else if (fd >= 2) {
			struct fd_entry *entry = find_fd_entry(fd);
			if (entry == NULL || entry->file == NULL) {
				f->R.rax = -1;
				DEG_NOTE ("write", "return=-1 reason=no-entry fd=%d", fd);
				break;
			}
			validate_user_buffer(buffer, size);
			lock_acquire(&filesys_lock);
			f->R.rax = file_write(entry->file, buffer, size);
			lock_release(&filesys_lock);
			DEG_NOTE ("write", "return=%d reason=file", (int) f->R.rax);
		} else {
			f->R.rax = -1;
			DEG_NOTE ("write", "return=-1 reason=bad-fd fd=%d", fd);
		}
		break;
	}

	case SYS_READ: {
		/* read(fd, buffer, size)의 인자는 syscall_entry가 저장한 레지스터에서
		 * 꺼낸다. rdi는 fd, rsi는 사용자 버퍼 주소, rdx는 읽을 바이트 수다.
		 * 시스템 콜 반환값도 rax로 돌아가므로 read() 결과를 f->R.rax에 저장한다. */
		f->R.rax = read((int) f->R.rdi, (void *) f->R.rsi, (unsigned) f->R.rdx);
		break;
	}

	case SYS_SEEK: {
		int fd = (int)f->R.rdi;
		off_t position = (off_t)f->R.rsi;
		struct fd_entry *entry = find_fd_entry(fd);

		if (entry == NULL || entry->file == NULL || position < 0) {
			break;
		}

		lock_acquire(&filesys_lock);
		file_seek(entry->file, position);
		lock_release(&filesys_lock);
		break;
	}

	case SYS_CLOSE: {
		int fd = (int) f->R.rdi;
		struct list_elem *e;
		for (e = list_begin(&t->fd_list);
			 e != list_end(&t->fd_list);
			 e = list_next(e)) {
			struct fd_entry *entry = list_entry(e, struct fd_entry, elem);
			if (entry->fd == fd) {
				lock_acquire(&filesys_lock);
				file_close(entry->file);
				lock_release(&filesys_lock);
				list_remove(&entry->elem);
				free(entry);
				break;
			}
 		}
		break;
	}
	
	case SYS_HALT: {
		power_off();
	}
	
	case SYS_FORK: {
		const char *thread_name = (const char *) f->R.rdi;
		char *kbuf;
		
		kbuf = palloc_get_page(0);

		if (kbuf == NULL) {
			f->R.rax = TID_ERROR;
			break;
		}
		
		if (!copy_in_string(kbuf, thread_name, PGSIZE))	{
			palloc_free_page(kbuf);
			t->exit_status = -1;
    		thread_exit();
		}

		f->R.rax = process_fork(kbuf, f);
		break;
	}

	case SYS_WAIT: {
		tid_t child_tid = (tid_t)f->R.rdi;
		f->R.rax = process_wait(child_tid);
		break;
	}

	case SYS_EXEC: {
		const char *cmd_line = (const char *)f->R.rdi;
		char *kbuf;

		kbuf = palloc_get_page(0);
		if (kbuf == NULL) {
			f->R.rax = -1;
			break;
		}

		if (!copy_in_string(kbuf, cmd_line, PGSIZE)) {
			palloc_free_page(kbuf);
			t->exit_status = -1;
    		thread_exit();
		}

		int ret = process_exec(kbuf);
		if (ret == -1) {
			t->exit_status = -1;
			thread_exit();
		}
		f->R.rax = ret;
		break;
	}
	
	default:
		break;
	}
}

static bool copy_in_string (char *buf, const char *command, size_t size) {
    size_t i;
    struct thread *t = thread_current ();
	DEG_CALL ("buf=%p command=%p size=%d", buf, command, (int) size);

    if (command == NULL) {
		DEG_RETURN ("value=0 reason=null-command");
        return false;
	}

    for (i = 0; i < size; i++) {
        const char *uaddr = command + i;

        if (!is_user_vaddr (uaddr)) {
			DEG_RETURN ("value=0 reason=kernel-addr uaddr=%p", uaddr);
            return false;
		}
        if (pml4_get_page(t->pml4, uaddr) == NULL) {
			DEG_RETURN ("value=0 reason=unmapped uaddr=%p pml4=%p", uaddr, t->pml4);
            return false;
		}

        buf[i] = *uaddr;
        if (buf[i] == '\0') {
			DEG_RETURN ("value=1 copied=%d", (int) i + 1);
            return true;
		}
    }

	DEG_RETURN ("value=0 reason=no-null size=%d", (int) size);
    return false;
}

static void
validate_user_ptr(const void *ptr) {
	struct thread *cur = thread_current();
	bool is_user_addr = ptr != NULL && is_user_vaddr(ptr);

	if (is_user_addr) {
		if(!pml4_get_page(cur->pml4, ptr)) {
			if (!vm_claim_page((void *) ptr)) {
				kill_process_due_to_bad_user_memory();
			} 
		}
	} else {
		DEG_NOTE("badptr", "ptr=%p is_user=%d pml4=%p",
				 ptr, is_user_addr, cur->pml4);
		kill_process_due_to_bad_user_memory();
	}
}

static void
validate_user_buffer(const void *buffer, size_t size) {
	const uint8_t *p = buffer;
	DEG_CALL ("buffer=%p size=%d", buffer, (int) size);

	if (size == 0) {
		DEG_RETURN ("value=void reason=zero-size");
		return;
	}
	for (size_t i = 0; i < size; i++) {
		if (i == 0 || i == size - 1 || pg_ofs(p + i) == 0) {
			DEG_NOTE ("chkbuf", "i=%d addr=%p page=%p ofs=%u",
					(int) i, p + i, pg_round_down(p + i),
					(unsigned) pg_ofs(p + i));
		}
		validate_user_ptr(p + i);
	}
	DEG_RETURN ("value=void buffer=%p size=%d", buffer, (int) size);
}

static void
validate_user_string(const char *str) {
	const char *p = str;
	DEG_CALL ("str=%p", str);

	while (true) {
		validate_user_ptr(p);
		if (*p == '\0') {
			DEG_RETURN ("value=void len=%d", (int) (p - str));
			return;
		}
		p++;
	}
}

static void
kill_process_due_to_bad_user_memory(void) {
	DEG_NOTE ("kill", "exit_status=-1");
	thread_current()->exit_status = -1;
	thread_exit();
}
