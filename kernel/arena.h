// kernel/arena.h

# ifndef ARENA_H
# define ARENA_H

# include <stdint.h>
# include <stddef.h>

// AIOS 물리 메모리 매핑 규격 사항
# define ARENA_START_ADDRESS 0x04000000 // 64MB 지점부터 아레나 시작
# define ARENA_MAX_SIZE (900 * 1024 * 1024) // 약 900MB 공간 확보
// 1024MB(1GB) - 64MB(start point) = 960MB, 900MB 정도만 사용

// tensor arena 시스템 초기화
void arena_init(void);

// arena 풀에서 지정된 크기만큼 연속된 물리 메모리 할당
// AVX-512 가속을 위해서 모든 주소는 64바이트 단위로 자동 정렬됨
// AVX-512: Advanced Vector Extensions 512 bit
// cpu 내부에 일반 레지스터의 8배 크기인 512비트짜리 전용 레지스터 가동
void* arena_alloc(size_t size);

// arena의 할당 포인터를 처음으로 되돌려서 메모리를 전부 해제
// 오버헤드가 0 클럭인 초고속 리셋 연산
void arena_reset(void);

// 현재 아레나의 메모리 사용량 반환
size_t arena_get_used_memory(void);

# endif