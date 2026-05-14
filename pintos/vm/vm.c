/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "userprog/process.h"
#include "vm/vm.h"

#include "debug_trace.h"
#include "string.h"
#include "threads/mmu.h"
#include "vm/inspect.h"

struct list frame_table; // 관리 주체가 애매해서 일단 vm에 둠.

static bool spt_hash_cmp_va_less(const struct hash_elem *a,
		const struct hash_elem *b, void *aux UNUSED);
static uint64_t spt_hash_hash(const struct hash_elem *e, void *aux UNUSED);
static void spt_hash_destroy_item (struct hash_elem *e, void *aux UNUSED);
static void destroy_frame_if_exists(struct page *page);
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

	ASSERT (pg_round_down (upage) == upage);
	// UNITNIT은 생성 요청 타입으로서 쓸 수 없음. page_get_type() 참고하면 더 이해하기 쉬움
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) != NULL) {
		PANIC ("page found in vm_alloc_init");
	}

	struct page *new_page = malloc(sizeof (struct page));
	if (new_page == NULL) {
		PANIC ("out of memory - new_page");
	}

	switch (VM_TYPE(type)) {
		case VM_ANON:
			uninit_new (new_page, upage, init, VM_ANON, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new (new_page, upage, init, VM_FILE, aux, file_backed_initializer);
			break;
		#ifdef EFILESYS  /* For project 4 */
		case VM_PAGE_CACHE:
			uninit_new (new_page, upage, init, VM_PAGE_CACHE, aux, page_cache_initializer);
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
	bool is_found = found != NULL;
	ASSERT (is_found);

	spt_hash_destroy_item (found, NULL);
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

	void *kva = palloc_get_page (PAL_USER);
	if (kva == NULL) {
		PANIC ("todo - need_evict");
	}

	struct frame *frame = malloc (sizeof (struct frame));
	if (frame == NULL) {
		PANIC ("out of memory - frame");
	}

	frame->kva = kva;
	frame->page = NULL;

	memset (frame->kva, 0, PGSIZE); // 보안을 위해 0으로 초기화

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	DEG_RETURN ("value=%p", (void *) frame);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
	DEG_CALL ("addr=%p", addr);
	// 스택의 writable은 항상 참이여야함
	if (!vm_alloc_page (VM_ANON | VM_MARKER_0, addr, true)) {
		goto panic;
	}
	if (!vm_claim_page (addr)) {
		goto panic;
	}

	DEG_RETURN ("void");
	return;
panic:
	PANIC ("FAILED vm_stack_growth");
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
	uintptr_t sp = thread_current ()->rsp_at_syscall;
	struct page *page = NULL;
	void *va = pg_round_down (addr);

	page = spt_find_page (spt, va);

	bool is_not_found = page == NULL;
	bool is_stack_access = (addr + 8) == (void *) sp;
	bool need_stack_growth = is_stack_access && is_not_found;


	void *upva = pg_round_down (addr+PGSIZE); // 경계 영역이면 그대로라서 항상 올림을 위해서
	DEG_NOTE ("stk", "is_stack_access=%d sp_sys=%p va=%p upva=%p addr=%p rsp=%p", is_stack_access, sp, va, upva, addr, f->rsp);
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);
	if (stack_bottom == upva) {
		upva = stack_bottom;
	}
	struct page *uppage = spt_find_page (spt, upva);
	if (uppage != NULL) {
		DEG_NOTE ("stk2", "upva=%p va=%p uppage=%p type=%d", upva, va, uppage->va, uppage->operations->type);
	}

	DEG_BRANCH ("need_stack_growth", need_stack_growth);
	if (need_stack_growth) {
		vm_stack_growth (va);
		DEG_RETURN ("value=%d cause=need_stack_growth", true);
		return true;
	}

	DEG_BRANCH ("is_not_found", is_not_found);
	if (is_not_found) {
		DEG_RETURN ("value=%d cause=is_not_found", false);
		return false;
	}

	bool is_writable_dismach = page->writeable != write;
	DEG_BRANCH ("is_writable_dismach", is_not_found);
	if (is_writable_dismach) {
		DEG_RETURN ("value=%d cause=is_writable_dismach", false);
		return false;
	}

	//TODO: not_present의 의미가 뭔지 모르겠음. 뭐 나중에 뒤지다 보면 나오려나? 핸들링 추가 필요
	// 이거 execption.c에 있음 /* 참이면 페이지 부재, 거짓이면 읽기 전용 페이지에 쓰기. */
	// 엄 일단 오케이 읽기 전용 페이지가 뭔 말인진 모르겠지만 나중에 개발할 때 참고할 수 있을 듯?


	bool result = vm_do_claim_page (page);
	DEG_RETURN ("value=%d cause=success", result);
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
	list_push_back(&frame_table, &frame->elem); // 새거니까 추가

	/* Set links */
	frame->page = page;
	page->frame = frame;

	pml4_set_page (thread_current ()->pml4, page->va, frame->kva,
			page->writeable);

	bool result = swap_in (page, frame->kva);

	DEG_RETURN ("value=%d result=%d page=%p frame=%p kva=%p",
				false, result, (void *) page, (void *) frame, frame->kva);
	return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	DEG_CALL ("init spt=%p", (void *) spt);
	hash_init (&spt->table, spt_hash_hash, spt_hash_cmp_va_less, NULL);
	DEG_RETURN ("void");
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	DEG_CALL ("copy dst=%p src=%p", (void *) dst, (void *) src);
	struct hash_iterator i;

	hash_first (&i, &src->table);
	while (hash_next (&i)) {
		struct page *src_page = hash_entry (hash_cur (&i), struct page, elem);
		void *uva = src_page->va; // 둘 다 동일한 user virtual addr을 공유

		bool is_uninit = VM_TYPE (src_page->operations->type) == VM_UNINIT;

		if (is_uninit) {
			// uninit 상태이므로 page만 복사 (깊은 복사)

			//TODO: 나중에 aux 타입 바뀔 수 있으면 어떻게 할지 생각해봐야 함
			struct page_lazy_load_aux *src_aux = src_page->uninit.aux;
			struct page_lazy_load_aux *dst_aux = malloc (sizeof (struct page_lazy_load_aux));
			ASSERT (dst_aux != NULL);

			memcpy (dst_aux, src_aux, sizeof *src_aux);
			// aux의 file은 load() 에서 이미 복사되어있는 상태라서 처리 필요 없음.
			ASSERT(dst_aux->file != NULL);

			vm_alloc_page_with_initializer (page_get_type (src_page), uva,
					src_page->writeable, src_page->uninit.init, dst_aux);
		} else {
			// page와 매핑된 frame까지 새로 만들어서 데이터 복사
			ASSERT (src_page->frame != NULL);
			void *src_kva = src_page->frame->kva;

			vm_alloc_page (page_get_type (src_page), uva, src_page->writeable);
			vm_claim_page (uva);
			struct page *dst_page = spt_find_page (dst, uva);

			ASSERT (dst_page != NULL);
			ASSERT (dst_page->frame != NULL);
			void *dst_kva = dst_page->frame->kva;
			memcpy (dst_kva, src_kva, PGSIZE);
		}
	}
	DEG_RETURN ("value=true");
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	DEG_CALL ("kill spt=%p", (void *) spt);
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy (&spt->table, spt_hash_destroy_item);
	DEG_RETURN ("void");
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

static void
spt_hash_destroy_item (struct hash_elem *e, void *aux UNUSED) {
	//TODO: 나중에는 이거 ref cnt로 바뀔수도?
	struct page *page = hash_entry (e, struct page, elem);
	destroy_frame_if_exists (page);
	vm_dealloc_page (page);
}

static void init_frame_table(void) {
	DEG_CALL ("frame_table=%p", (void *) &frame_table);
	list_init (&frame_table);
	DEG_RETURN ("void");
}

static void
destroy_frame_if_exists(struct page* page) {
	if (page->frame != NULL) {
		list_remove(&page->frame->elem);
		pml4_clear_page (thread_current ()->pml4, page->va);
		palloc_free_page (page->frame->kva);
		free (page->frame);
	}
}
