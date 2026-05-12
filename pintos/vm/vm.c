/* vm.c: 가상 메모리 객체를 위한 공통 인터페이스. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "kernel/hash.h"
#include "vm/inspect.h"

bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED);

/* 각 하위 시스템의 초기화 코드를 호출해 가상 메모리 하위 시스템을
 * 초기화한다. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* project 4용 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* 위쪽 줄은 수정하지 말 것. */
	/* TODO: 여기에 코드를 작성한다. */
}

/* page의 타입을 얻는다. 이 함수는 page가 초기화된 뒤 어떤 타입이 될지
 * 알고 싶을 때 유용하다.
 * 이 함수는 이미 완전히 구현되어 있다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* 보조 함수들 */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* initializer를 가진 대기 상태의 page 객체를 만든다. page를 만들고 싶다면
 * 직접 생성하지 말고 이 함수나 `vm_alloc_page`를 통해 만들어야 한다. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 사용 중인지 확인한다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: page를 만들고, VM 타입에 맞는 initializer를 가져온 뒤,
		 * TODO: uninit_new를 호출해 "uninit" page 구조체를 만든다.
		 * TODO: uninit_new 호출 이후에는 필요한 필드를 수정해야 한다. */

		/* TODO: page를 spt에 삽입한다. */
	}
err:
	return false;
}

/* spt에서 VA에 해당하는 page를 찾아 반환한다. 실패하면 NULL을 반환한다. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	void *rva = pg_round_down(va);
	struct page target_page;
	target_page.va = rva;
	struct hash_elem *elem = hash_find(&spt->pages, &target_page.hash_elem);	// stp 안에 실제로 들어있던 page의 elem 반환

	if (elem == NULL) {
		return NULL;
	} else {
		return hash_entry(elem, struct page, hash_elem);
	}
}

/* 검증을 거쳐 PAGE를 spt에 삽입한다. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	if (!hash_insert(&spt->pages, &page->hash_elem)) {
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	if (hash_delete(&spt->pages, &page->hash_elem)) {
		vm_dealloc_page(page);
	}
	return;
}

/* 교체(evict)할 struct frame을 얻는다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: eviction 정책은 직접 정한다. */

	return victim;
}

/* page 하나를 evict하고 그에 해당하는 frame을 반환한다.
 * 오류가 나면 NULL을 반환한다. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: victim을 swap out하고 evict된 frame을 반환한다. */

	return NULL;
}

/* palloc()으로 frame을 얻는다. 사용할 수 있는 page가 없으면 page를 evict한
 * 뒤 그 frame을 반환한다. 이 함수는 항상 유효한 주소를 반환해야 한다.
 * 즉, user pool 메모리가 가득 찼다면 frame을 evict해서 사용 가능한
 * 메모리 공간을 확보한다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof (struct frame));
	if (frame == NULL) {
		PANIC("todo");
	}
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 확장한다. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* write-protected page에서 발생한 fault를 처리한다. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* 성공하면 true를 반환한다. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: fault가 유효한지 검증한다. */
	/* TODO: 여기에 코드를 작성한다. */

	return vm_do_claim_page (page);
}

/* page를 해제한다.
 * 이 함수는 수정하지 말 것. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 page를 claim한다. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	struct thread *cur = thread_current();
	/* TODO: 이 함수를 채운다. */
	page = spt_find_page(&cur->spt, va);
	if (page == NULL) {
		return false;
	}

	return vm_do_claim_page (page);
}

/* PAGE를 claim하고 MMU를 설정한다. */
static bool
vm_do_claim_page (struct page *page) {
	struct thread *cur = thread_current();
	struct frame *frame = vm_get_frame ();

	/* 연결 관계를 설정한다. */
	frame->page = page;
	page->frame = frame;

	/* TODO: page의 VA를 frame의 PA에 매핑하도록 page table entry를 삽입한다. */
	if (!pml4_set_page(cur->pml4, page->va, frame->kva, page->writable)) {
		return false;
	}
	return swap_in (page, frame->kva);
}

/* 새 supplemental page table을 초기화한다. */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* src의 supplemental page table을 dst로 복사한다. */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* supplemental page table이 보유한 자원을 해제한다. */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: thread가 보유한 모든 supplemental_page_table을 파괴하고,
	 * TODO: 수정된 모든 내용을 저장소에 다시 기록한다. */
}

uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&page->va, sizeof page->va);
}

bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct page *pa = hash_entry(a, struct page, hash_elem);
	struct page *pb = hash_entry(b, struct page, hash_elem);

	return pa->va < pb->va;
}