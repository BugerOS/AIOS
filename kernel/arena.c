// kernel/arena.c
# include "arena.h"

// arena 제어용 내부 하드웨어 주소 주소
static uint8_t* arena_base_pointer = (uint8_t*)ARENA_START_ADDRESS;
static size_t arena_offset = 0;

void arena_init(void){
    arena_base_pointer = (uint8_t*)ARENA_START_ADDRESS;
    arena_offset = 0;
}

void* arena_alloc(size_t size){
    if(size == 0){
        return NULL;
    }

    // AVX 가속 필수 조항
    // 하드웨어 버스 타이밍 정렬을 위해서 요청 크기를 64바이트 배수로 올림 팩킹
    // EX: 1 바이트 요청 시에도 64바이트 할당
    size = (size + 63) & ~((size_t)63);

    // 하드웨어 한계 바운더리 검문
    if(arena_offset + size > ARENA_MAX_SIZE){
        return NULL; // out of memory
    }

    // 현재 베이스 주소에 오프셋을 더해서 물리 주소 확정
    void* allocated_address = (void*)(arena_base_pointer + arena_offset);

    // fragmentation 없이 포인터를 청크 크기만큼 앞으로 전진
    arena_offset = arena_offset + size;

    return allocated_address;
}

void arena_reset(void){
    // 복잡한 linked list 순회 없이 그냥 offset을 0으로 설정
    // -> 해제와 같은 효과
    arena_offset = 0;
}

size_t arena_get_used_memory(void){
    return arena_offset;
}