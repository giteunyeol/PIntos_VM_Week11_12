/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "debug_trace.h"
#include "userprog/process.c"

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
//초기화 이후걸 가져오면 uninit일때는 체크를 못해줌
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type); //페이지 현재 타입
	switch (ty) {
		case VM_UNINIT: 
			return VM_TYPE (page->uninit.type); //페이지가 될 타입
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
unsigned page_hash (const struct hash_elem *e, void *aux);// 페이지 엘엠을 받아서 페이지 밖으로 이동 후 va 찾아서 헤시 키로 변환.
bool page_less (const struct hash_elem *a,const struct hash_elem *b, void *aux); //버킷 안의 주소 비교 -> 같은 키인지 반환
void destroy_page(struct hash_elem *e, void *aux); //spt_page_table_kill 헬퍼함수

	/* Create the pending page object with initializer. If you want to create a
	 * page, do not create it directly and make it through this function or
	 * `vm_alloc_page`. */
	bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
										vm_initializer *init, void *aux)
{

	ASSERT (VM_TYPE(type) != VM_UNINIT) //그러면 얘는 나중에 될 타입 판정하는거네... uninit -> ?가 될건지

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
	
		bool (*initializer)(struct page *, enum vm_type, void *);
		if(VM_TYPE(type) == VM_ANON) {
			initializer = anon_initializer;
		}
		else if(VM_TYPE(type) == VM_FILE) {
			initializer = file_backed_initializer;
		}
		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;
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
	struct hash_elem *e = hash_find(&spt->pages, &page.elem);
	/* TODO: Fill this function. */
	//해당 주소를 가지고 있는 페이지가 있는지 해시를 뒤져서
	//있으면 그 페이지 떤져주고, 없으면 에러(널 리턴)
	if (!e) {
		return NULL;
	}
	struct page *found = hash_entry(e, struct page, elem);
	return found; //해시 뒤졌는데 페이지 찾음
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	/* TODO: Fill this function. */
	//spt에 페이지를 삽입 하는데, 이게 있는지 체크해서 없으면 넣기, (중복 페이지가)있으면 false리턴
	struct hash_elem *old = hash_insert(&spt->pages, &page->elem);
	if(old == NULL) {
		return true;
	}
	return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	if (hash_delete(&spt->pages, &page->elem)) {
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
vm_stack_growth (void *addr) {
	vm_alloc_page(VM_ANON, addr, true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {

	if(!not_present) {
		return false; 
	}
	//write얘는 페이지가 존재하는지 안하는지 근데 우리가 페이지 안에 writable선언
	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	struct thread *cur = thread_current();
	uintptr_t address = (uintptr_t)addr;
	uintptr_t user_rsp;

	if (user) {
		user_rsp = f->rsp;
	} else {
		user_rsp = cur->saved_user_rsp;
	}
	struct page *page = spt_find_page(spt, addr);

	if (page == NULL) {
		if (USER_STACK - STACK_LIMIT <= address  && address < USER_STACK && address >= user_rsp - 8) {
			vm_stack_growth(pg_round_down(addr));
			page = spt_find_page(spt, addr);
		} else {
			return false;
		}
	}

	if (!(page->writable) && write) {
		return false;
	}

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
	page = spt_find_page(&current->spt, va);
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
	//성공하면 true, 메모리 할당이면 false를 반환합니다
	if (!pml4_set_page(current->pml4, page->va, frame->kva, page->writable)) {
		free(frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	DEG_CALL ("spt=%p pages_addr=%p", spt, &spt->pages);
	hash_init(&spt->pages, page_hash, page_less, NULL);
	DEG_RETURN ("value=void buckets=%p elem_cnt=%d",
			spt->pages.buckets, (int) spt->pages.elem_cnt);
}
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {

	//struct hash copy_page; //페이지 카피할 해시형 변수 생성
	//hash_init(&copy_page, page_hash, page_less, NULL); //copy_page 초기화

	//해시 순회
	struct hash_iterator i; 
	hash_first(&i, &src->pages); //해시 처음 가져오기
	while(hash_next(&i)) { //해시 다 돌때까지 순회
		struct page * pg = hash_entry(hash_cur(&i), struct page, elem); //페이지 순회하면서 삽입
		enum vm_type cur_type = pg->operations->type; //cur_type : 현재 페이지 타입

		// 부모 페이지 타입에 따라 분류
		if (cur_type == VM_UNINIT)
		{ // 현재 타입이 uninit인 케이스
			// 나중에 될 인자로 페이지 하나 생성해서 넣어줌, page_get_type은 uninit이 나중에 될 페이지를 리턴함
			struct page * new_page = malloc(sizeof(struct page));
			if(!new_page) { //new_page 할당 실패시
				return false;
			}

			struct aux *new_aux = malloc(sizeof(struct aux)); // vm_alloc_page_with_initializer 함수에서따옴
			*new_aux = *((struct aux *)(pg->uninit.aux));	  // struct로 형변환하고 삽입
			// new_page의 원소들에 각각 삽입, 부모의 페이지를
			uninit_new(new_page, pg->va, pg->uninit.init, page_get_type(pg), new_aux, pg->uninit.page_initializer);//페이지 초기화
			if(!spt_insert_page(dst, new_page)) { //삽입 실패시
				free(new_page);
				free(new_aux);
				return false;
			}
		}
		else if(cur_type == VM_ANON){ //부모 타입이 anon 
			//아니 시벌 새로운 페이지 생성을 어케 함? 이미 바뀌어버린거잖아
			//둘다 그냥 vm_alloc_page로 생성한 후 ANON/FILE_BACKED로 바꾸게 페이지 폴트를 일부러 한번 일으켜야하나?
			//근데 그건 말이 안되는데?? 원래 그냥 spt에 올리고 lazy 하는거잖음 
			struct page *new_page = malloc(sizeof(struct page));
			if(!new_page) {
				return false;
			}
			//기본값들 채워주기
			new_page->va = pg->va;
			new_page->writable = pg->writable;
			new_page->frame = NULL;

			if(!anon_initializer(new_page, VM_ANON, NULL)) { //uninit이 아니게 되서 uninit_new를 못사용하니까 anon 이니셜라이저 사용.
				free(new_page);
				return false; // 초기화 실패한 경우
			} 
			if(!spt_insert_page(dst, new_page)) { //spt에 삽입
				free(new_page);
				return false;
			}
			if(!vm_do_claim_page(new_page)){ //바로 물리프레임 올리기
				free(new_page);
				return false;
			}
			//부모 프레임 복사
			memcpy(new_page->frame->kva, pg->frame->kva, PGSIZE);
		}
		else if(cur_type == VM_FILE) { //file_backed인 경우
			struct page *new_page = malloc(sizeof(struct page));
			if(!new_page) {
				return false;
			}
			//기본값들 채워주기
			new_page->va = pg->va;
			new_page->writable = pg->writable;
			new_page->frame = NULL;

			if(!file_backed_initializer(new_page, VM_FILE, NULL)) { //uninit이 아니게 되서 uninit_new를 못사용하니까 anon 이니셜라이저 사용.
				free(new_page);
				return false; // 초기화 실패한 경우
			} 
			if(!spt_insert_page(dst, new_page)) { //spt에 삽입
				free(new_page);
				return false;
			}
			if(!vm_do_claim_page(new_page)){ //바로 물리프레임 올리기
				free(new_page);
				return false;
			}
			//부모 프레임 복사
			memcpy(new_page->frame->kva, pg->frame->kva, PGSIZE);
		}
	}
	//++)아니 아 vm_alloc_page에서 현재 스레드 spt에 삽입해주고있는데, 이걸 뭐 어케함? 다른 유틸함수가 없잖아;;;
	//포크에서는 세마로 부모 스레드를 재우고 자식 스레드가 동작해서 같이 공유하는 전광판을 체크해줘서 부모 스레드를 웨이터스로 바꿔줬잖아
	//그러면 여기선 어캄? 존나게 근본적인 문제가 생겨버렸는데;; 
	//아,,,,,,,,,, vm alloc page말고 걍 하나 구조체 생성해서 직접삽입............ ㅋㅋㅋㅋㅋㅋㅋㅋ
		/*
		if(!vm_alloc_page(pg->operations->type, pg->va, pg->writable)) { //새로운 페이지를 할당해줌.
			return false;
			//이녀석 함수 첫번째 인자는 이제 uninit이 나중에 어떤 페이지로 바뀔지 저장해주는인자임
			//처음에 이런식으로 해줄랬는데, vm_alloc_page()는 램에 올려주는 함수기때문에.
			//열어보면 UNINIT타입을 못받게 ASSERT가 내부에 걸려있음.
			그래서 page_get_type()을 사용.
		}
		*/
	/*
		//각각의 케이스에 따라서 할당, uninit, anon, file_backed
		//anon, file_backed인 경우는 페이지 테이블에 올려줘야함
		if(&new_page->operations->type != VM_UNINIT) { //페이지가 anon, file_backed인경우
			vm_claim_page(new_page->va); //claim
			new_page->frame = pg->frame;
		}
		*/
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	
	//mmap이 아직 안돼서 최소구현으로 단순하게 SPT순회하면서 페이지별 destroy, spt table emtpy

	hash_destroy(&spt->pages, destroy_page);
}

unsigned
page_hash (const struct hash_elem *e, void *aux )  {
	struct page *something = hash_entry(e, struct page, elem);
	return hash_bytes ( &something -> va, sizeof something -> va) ;
}

bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux) {
	const struct page *a = hash_entry (a_, struct page, elem);
	const struct page *b = hash_entry (b_, struct page, elem);
	
	return a->va < b->va;
}

void destroy_page(struct hash_elem * e, void * aux UNUSED) {
	//해시 원소 하나를 받아 해당 원소가 포함된 struct page를 찾고, 페이지 정리 후 free
	struct page * page = hash_entry(e, struct page, elem);
	vm_dealloc_page(page);
}