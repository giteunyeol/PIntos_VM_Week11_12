/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"

#include "debug_trace.h"
#include "threads/mmu.h"
#include "vm/inspect.h"

struct list frame_table; // 관리 주체가 애매해서 일단 vm에 둠.

static bool spt_hash_cmp_va_less(const struct hash_elem *a,
		const struct hash_elem *b, void *aux UNUSED);
static uint64_t spt_hash_hash(const struct hash_elem *e, void *aux UNUSED);
static void init_frame_table(void);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	DEG_CALL ("void");
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	init_frame_table ();
	DEG_RETURN ("void");
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
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

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	DEG_CALL ("type=%d upage=%p writable=%d init=%p aux=%p",
			type, upage, writable, (void *) init, aux);

	// UNITNIT은 생성 요청 타입으로서 쓸 수 없음. page_get_type() 참고하면 더 이해하기 쉬움
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	// TODO: upage가 항상(모든 테스트 케이스) alined 된 상태라면 pg_round_down 제거
	void *va = pg_round_down (upage);
	//DEG_NOTE ("stat", "upage=%p va=%p", upage, va); //TODO: 나중에 켜서 체크

	//DEG_NOTE ("stat", "hash_size=%ld", hash_size(&spt->table));

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, va) != NULL) {
		PANIC ("page found in vm_alloc_init");
	}

	struct page *new_page = malloc(sizeof (struct page));
	if (new_page == NULL) {
		PANIC ("out of memory - new_page");
	}

	switch (VM_TYPE(type)) {
		case VM_ANON:
			uninit_new (new_page, va, init, VM_ANON, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new (new_page, va, init, VM_FILE, aux, file_backed_initializer);
			break;
		#ifdef EFILESYS  /* For project 4 */
		case VM_PAGE_CACHE:
			uninit_new (new_page, va, init, VM_PAGE_CACHE, aux, page_cache_initializer);
			break;
		#endif
		default:
			PANIC ("Unsupported VM Type(%d)", VM_TYPE(type));
			break;
	}

	new_page->writeable = writable;

	spt_insert_page (spt, new_page);

	DEG_RETURN ("value=true");
	return true;
err:
	DEG_RETURN ("value=false");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	DEG_CALL ("spt=%p va=%p", (void *) spt, va);

	ASSERT ((uintptr_t) va == (uintptr_t) pg_round_down (va)) // must aligned

	// 임시 탐색용 page
	struct page temp_pg = {
		.va = va,
	};

	struct hash_elem *found = hash_find (&spt->table, &temp_pg.elem);

	bool is_not_found = found == NULL;
	DEG_BRANCH ("is_not_found", is_not_found);
	if (is_not_found) {
		DEG_RETURN ("value=%p", NULL);
		return NULL;
	}

	struct page *page = hash_entry (found, struct page, elem);

	DEG_RETURN ("value=%p", (void *) page);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(spt != NULL);
	ASSERT(page != NULL);
	DEG_CALL ("spt=%p page=%p va=%p", (void *) spt, (void *) page, page->va);

	bool result = true;

	struct hash_elem *old = hash_insert (&spt->table, &page->elem);

	//TODO: 나중에 요소 있는 경우에는 false 여야하면 ASSERT 지우고 early return 추가
	bool is_already_exist = old != NULL;
	ASSERT(!is_already_exist);

	DEG_RETURN ("value=%d", result);
	return result;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(spt != NULL);
	ASSERT(page != NULL);
	DEG_CALL ("spt=%p page=%p va=%p", (void *) spt, (void *) page, page->va);

	struct hash_elem *found = hash_delete (&spt->table, &page->elem);

	//TODO: 나중에 요소 없는 경우도 지원해야 하면 ASSERT 지우고 early return 추가
	bool is_not_found = found != NULL;
	ASSERT (!is_not_found);

	vm_dealloc_page (page);

	DEG_RETURN ("void");
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	DEG_CALL ("void");
	struct frame *frame = NULL;

	frame = palloc_get_page (PAL_USER);

	bool need_evict = frame == NULL;
	if (need_evict) {
		PANIC ("todo - need_evict");
	}

	// init
	frame->page = NULL;
	frame->kva = frame;

	list_push_front(&frame_table, &frame->elem); // 새거니까 추가

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	DEG_RETURN ("value=%p", (void *) frame);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,bool user, bool write,
		bool not_present) {
	DEG_CALL ("f=%p addr=%p user=%d write=%d not_present=%d",
				(void *) f, addr, user, write, not_present);

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	void *va = pg_round_down (addr);

	page = spt_find_page (spt, va);
	bool is_not_found = page == NULL;

	DEG_BRANCH ("is_not_found", is_not_found);
	if (is_not_found) {
		DEG_RETURN ("value=%d", false);
		return false;
	}

	bool result = vm_do_claim_page (page);
	DEG_RETURN ("value=%d", result);
	return result;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	DEG_CALL ("va=%p", va);
	struct page *page = NULL;

	struct supplemental_page_table *spt = &thread_current ()->spt;
	page = spt_find_page (spt, va);

	ASSERT (page != NULL);
	bool result = vm_do_claim_page (page);
	DEG_RETURN ("value=%d page=%p", result, (void *) page);
	return result;
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	DEG_CALL ("page=%p va=%p", (void *) page, page != NULL ? page->va : NULL);
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	DEG_NOTE("temp", "page->va=%p frame->kva=%p", page->va, frame->kva);
	pml4_set_page (thread_current ()->pml4, page->va, frame->kva,
			page->writeable);
	DEG_NOTE("here", "0");

	bool result = swap_in (page, frame->kva);

	DEG_NOTE("here", "1");
	DEG_RETURN ("value=%d page=%p frame=%p kva=%p",
				result, (void *) page, (void *) frame, frame->kva);
	return result;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	DEG_CALL ("spt=%p", (void *) spt);
	hash_init (&spt->table, spt_hash_hash, spt_hash_cmp_va_less, NULL);
	DEG_RETURN ("void");
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	DEG_CALL ("dst=%p src=%p", (void *) dst, (void *) src);
	// TODO: 프로세스와 page의 관계나 그런게 아직 확실하게 생각나지 않음.
	// fork, exit 등으로 2개 이상에서 동일한 page를 볼 수 있고, 제거할 때도 고민되는데
	// ref cnt가 지금 당장 생각하기에는 확실한 후보인데 구현 하면서 바뀔 수 있어서 구현 미룸
	// kill도 마찬가지
	DEG_RETURN ("value=false");
	return false;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

static bool
spt_hash_cmp_va_less(const struct hash_elem *a, const struct hash_elem *b,
			 void *aux UNUSED) {
	const struct page *pga = hash_entry (a, struct page, elem);
	const struct page *pgb = hash_entry (b, struct page, elem);

	return (uintptr_t) pga->va < (uintptr_t) pgb->va; // 정수형 비교가 직관적
}

static uint64_t
spt_hash_hash(const struct hash_elem *e, void *aux UNUSED) {
	struct page *pg = hash_entry (e, struct page, elem);
	return hash_bytes (&pg->va, sizeof &pg->va);
}

static void init_frame_table(void) {
	DEG_CALL ("frame_table=%p", (void *) &frame_table);
	list_init (&frame_table);
	DEG_RETURN ("void");
}
