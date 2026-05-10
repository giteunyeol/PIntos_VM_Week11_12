/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"

#include "debug_trace.h"
#include "vm/inspect.h"

static bool spt_hash_cmp_va_less(const struct hash_elem *a,
		const struct hash_elem *b, void *aux UNUSED);
static uint64_t spt_hash_hash(const struct hash_elem *e, void *aux UNUSED);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
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

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	void *aligned_va = pg_round_down (va);

	// 임시 탐색용 page
	struct page temp_pg = {
		.va = aligned_va,
	};

	struct page *page = hash_entry (hash_find(&spt->table, &temp_pg.elem),
		struct page, elem);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	void *exist_or_null = hash_entry (hash_insert(&spt->table, &page->elem),
			struct page, elem);
	bool is_already_exist = exist_or_null != NULL;
	ASSERT (!is_already_exist); // 이런 경우가 있을지 모르겠지만? 일단 막기
	if (is_already_exist) {
		return true;
	}
	return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	void *removed_or_null = hash_entry (hash_delete(&spt->table, &page->elem),
			struct page, elem);
	bool is_not_found = removed_or_null == NULL;
	ASSERT (is_not_found); // 이런 경우가 있을지 모르겠지만? 일단 막기

	// TODO: 만약에?? ref cnt 추가하면 여기만 바꾸면 되긴 할 듯?
	vm_dealloc_page (page);
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
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
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
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	DEG_CALL ("f=%p addr=%p user=%d write=%d not_present=%d",
		(void *) f, addr, user, write, not_present);
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
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
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->table, spt_hash_hash, spt_hash_cmp_va_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// TODO: 프로세스와 page의 관계나 그런게 아직 확실하게 생각나지 않음.
	// fork, exit 등으로 2개 이상에서 동일한 page를 볼 수 있고, 제거할 때도 고민되는데
	// ref cnt가 지금 당장 생각하기에는 확실한 후보인데 구현 하면서 바뀔 수 있어서 구현 미룸
	// kill도 마찬가지
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
