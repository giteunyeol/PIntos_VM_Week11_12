/* file.c: Implementation of memory backed file object (mmaped object). */

#include "round.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "vm/vm.h"

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

	struct file_page *file_page = &page->file;
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
	struct file_page *file_page = &page->file;
	//if (pml4_is_dirty(thread_current ()->pml4, TODO)) {
		//file_write (TODO, TODO_VA, PGSIZE);
	//}
	// file_close (TODO); // 내부에서 file free도 해줌
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct supplemental_page_table spt = thread_current ()->spt;
	struct page *page = spt_find_page (&spt, addr);
	if (page != NULL) {
		return NULL;
	}

	size_t read_bytes = length;
	size_t zero_bytes = ROUND_UP(length, PGSIZE) - length;
	//
	// if (!load_segment(file, offset, addr, read_bytes, zero_bytes, writable)) {
	// 	return NULL;
	// }

	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	// struct supplemental_page_table spt = thread_current ()->spt;
	// struct page *page = spt_find_page (&spt, addr);
	// // ASSERT (page != NULL);
	// if (page != NULL) {
	// 	return;
	// }
	//
	// uint64_t cnt = page->file.size;
	// while (cnt > 0) {
	// 	vm_dealloc_page (page);
	// 	page = spt_find_page (&spt, page->va + PGSIZE);
	// 	cnt--;
	// }
}

