/* file.c: 메모리에 매핑된 파일 객체(mmap 객체)의 구현. */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "lib/string.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_file(struct page *page, void *lazy_load_aux);

/* 이 구조체는 수정하지 않는다. */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

struct lazy_load_aux {
	struct file *file;
	off_t offset;
	size_t read_bytes;
	size_t zero_bytes;	
};

/* 파일 VM 서브시스템 초기화 함수. */
void
vm_file_init (void) {
}

/* 파일 기반 페이지를 초기화한다. */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 페이지 처리 함수 테이블을 설정한다. */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* 파일에서 내용을 읽어 페이지를 swap in 한다. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
	if (file_read_at(file_page->file, kva, file_page->read_bytes, file_page->offset) != file_page->read_bytes) {
		return false;
	}
	uintptr_t kva_start_addr = (uintptr_t)kva;
	memset((void *)(kva_start_addr + file_page->read_bytes), 0, file_page->zero_bytes);
	return true;
}

/* 페이지 내용을 파일에 다시 써서 swap out 한다. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 파일 기반 페이지를 정리한다. PAGE 자체는 호출자가 해제한다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* mmap을 수행한다. */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* munmap을 수행한다. */
void
do_munmap (void *addr) {
}

static bool lazy_load_file(struct page *page, void *lazy_load_aux){
	struct lazy_load_aux *aux = lazy_load_aux;
	struct file_page *file_page = &page->file;
	file_page->file = aux->file;
	file_page->offset = aux->offset;
	file_page->read_bytes = aux->read_bytes;
	file_page->zero_bytes = aux->zero_bytes;
	free(lazy_load_aux);
	return true;
}