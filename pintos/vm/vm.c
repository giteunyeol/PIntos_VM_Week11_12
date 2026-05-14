/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
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
unsigned page_hash (const struct hash_elem *e, void *aux);// 페이지 엘엠을 받아서 페이지 밖으로 이동 후 va 찾아서 헤시 키로 변환.
bool page_less (const struct hash_elem *a_,const struct hash_elem *b_, void *aux); //버킷 안의 주소 비교 -> 같은 키인지 반환 

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page * page = malloc(sizeof(struct page));
		if(page == NULL) {
			return false;
		}
		page -> writable = writable;
		//타입에따라서 변수에 실행할 함수를 저장해주고 그걸 넘기라고?
		bool (*initializer)(struct page *, enum vm_type, void *);
		if(VM_TYPE(type) == VM_ANON) {
			initializer = anon_initializer;
		}
		else if(VM_TYPE(type) == VM_FILE) {
			initializer = file_backed_initializer;
		}
		uninit_new(page, upage, init, type, aux, initializer);
		/* TODO: Insert the page into the spt. */
		if(!spt_insert_page(spt, page)) {
			free(page);
			return false;
		}
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page page;
	va = pg_round_down(va); //va에 패딩 맞춰줌
	page.va = va;
	struct hash_elem *e = hash_find(spt->pages, &page.elem);
	/* TODO: Fill this function. */
	//해당 주소를 가지고 있는 페이지가 있는지 해시를 뒤져서
	//있으면 그 페이지 떤져주고, 없으면 에러(널 리턴)
	if (!e) {
		return NULL;
	}
	return hash_entry(e, struct page, elem); //해시 뒤졌는데 페이지 찾음
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	/* TODO: Fill this function. */
	//spt에 페이지를 삽입 하는데, 이게 있는지 체크해서 없으면 넣기, (중복 페이지가)있으면 false리턴
	if(hash_insert(spt->pages, &page->elem) == NULL) {
		return true;
	}
	return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	if (hash_delete(spt->pages, &page->elem)) {
		vm_dealloc_page (page);
	}
	return;
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
	struct frame *frame = malloc(sizeof (struct frame));
	/* TODO: Fill this function. */
	if (frame == NULL) {
		PANIC ("todo");
	}
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;

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
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread * current = thread_current();
	page = spt_find_page(&current->spt,va);
	if (page == NULL){
		return false;
	}
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
	struct thread *current = thread_current();
	//공하면 true, 메모리 할당이면 false를 반환합니다
	if (!pml4_set_page(current->pml4, page->va, frame->kva, page->writable)) {
		free(frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(spt->pages, page_hash, page_less, NULL);
}
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

unsigned
page_hash (const struct hash_elem *e, void *aux )  {
	struct page *something = hash_entry(e, struct page, elem);
	return hash_bytes ( &something -> va, sizeof something -> va) ;
}

bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}