/* anon.c: 비디스크 이미지 페이지의 구현 (익명 페이지라고도 함). */

#include "vm/vm.h"
#include "devices/disk.h"

/* 아래 줄을 수정하지 마시오 */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* 이 구조체를 수정하지 마시오 */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* 익명 페이지의 데이터 초기화 */
void vm_anon_init(void)
{
	/* TODO: swap_disk를 설정하시오. */
	swap_disk = NULL;
}

/* 파일 매핑 초기화 */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* 핸들러 설정 */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	if (anon_page == NULL)
	{
		return false;
	}
	return true;
}

/* 스왑 디스크에서 내용을 읽어 페이지를 스왑 인. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	ASSERT(page != NULL);
	struct anon_page *anon_page = &page->anon;
	return true;
}

/* 스왑 디스크에 내용을 써서 페이지를 스왑 아웃. */
static bool
anon_swap_out(struct page *page)
{
	ASSERT(page != NULL);
	struct anon_page *anon_page = &page->anon;
	return true;
}

/* 익명 페이지를 제거한다. PAGE는 호출자에 의해 해제된다. */
static void
anon_destroy(struct page *page)
{
	ASSERT(page != NULL);
	struct anon_page *anon_page = &page->anon;
	return;
}
