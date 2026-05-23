#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* Uninitlialized page. The type for implementing the
 * "Lazy loading". */
//레이지 로딩을 수행하기 위한 함수니까. 
//vm_initializer * init은 초기화될 타입인 것 같은데?
//page_initializer는 그렇게 초기화한 페이지pa를 va에 매핑하는듯
struct uninit_page {
	/* Initiate the contets of the page */ //페이지의 내용을 초기화한다?? 예?
	vm_initializer *init; //초기화될 타입?
	enum vm_type type;
	void *aux;
	/* Initiate the struct page and maps the pa to the va */
	//페이지 구조체를 초기화하고 pa를 va에 매핑한다?? 이게 뭔소리임?
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
