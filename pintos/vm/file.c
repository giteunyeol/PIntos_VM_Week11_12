/* file.c: Implementation of memory backed file object (mmaped object). */

#include "round.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/vm.h"
#include "include/debug_trace.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page UNUSED = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	//if (pml4_is_dirty(thread_current ()->pml4, TODO)) {
		//file_write (TODO, TODO_VA, PGSIZE);
	//}
	// file_close (TODO); // 내부에서 file free도 해줌
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t u_length, int writable,
		struct file *file, off_t offset) {
	DEG_CALL ("addr=%p u_length=%zu writable=%d file=%p offset=%d",
			addr, u_length, writable, (void *) file, offset);

	struct supplemental_page_table spt = thread_current ()->spt;
	struct page *page = spt_find_page (&spt, addr);
	bool page_already_exists = page != NULL;
	DEG_BRANCH ("page_already_exists", page_already_exists);
	if (page_already_exists) {
		DEG_RETURN ("value=%p cause=page_already_exists", NULL);
		return NULL;
	}

	off_t f_length = file_length (file);
	DEG_NOTE ("chk", "f_length=%d", f_length);

	size_t read_bytes;
	size_t zero_bytes;
	// 페이지 사이즈가 1000
	// 파일 크기가 2500
	// 사용자 요청이 3400 이라고 치면
	// 총 4000 = 2500 / 1500 이여야 함
	// 반대로 사용자의 파일 크기가 더 작은 경우 일단은 패닉, 나중에 필요하면 고려
	// u_length: 사용자 요청 길이
	// f_length: 실제 길이
	if (u_length >= f_length) {
		read_bytes = f_length;
		// f_length를 PGSIZE 단위로 올림.
		zero_bytes = ROUND_UP(u_length, PGSIZE) - f_length;
	} else {
		PANIC ("no: u_length >= f_length");
	}

	DEG_NOTE ("chk", "read_bytes=%zu zero_bytes=%zu", read_bytes, zero_bytes);

	bool ok = load_segment (file, offset, addr, read_bytes, zero_bytes, writable);
	DEG_BRANCH ("load_segment ok", ok);
	if (!ok) {
		DEG_RETURN ("value=%p cause=load_segment_failed", NULL);
		return NULL;
	}

	page = spt_find_page (&spt, addr);
	if (page == NULL) {
		PANIC ("page must exists");
	}
	page->mmaped_size = (read_bytes + zero_bytes) / PGSIZE;
	DEG_RETURN ("value=%p mmaped_size=%d", addr, page->mmaped_size);

	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	DEG_CALL ("addr=%p", addr);

	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = spt_find_page (spt, addr);
	bool is_not_head = page->mmaped_size <= 0;
	DEG_BRANCH ("is_not_head", is_not_head);
	if (is_not_head) {
		DEG_RETURN ("void cause=not_head mmaped_size=%d", page->mmaped_size);
		return;
	}

	DEG_LOOP_START ("dealloc pages", "mmaped_size=%d", page->mmaped_size);
	for (int i = 0; i < page->mmaped_size; i++) {
		page = spt_find_page (spt, addr + (i * PGSIZE));
		DEG_LOOP ("dealloc pages", "i=%d page=%p va=%p",
				i, (void *) page, page->va);
		vm_dealloc_page (page);
	}
	DEG_LOOP_END ("dealloc pages", "mmaped_size=%d", page->mmaped_size);
	DEG_RETURN ("void");
}
