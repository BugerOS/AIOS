// kernel/matrix.h
// 행렬 구조체 명세와 인프라 인터페이스 선언

# ifndef MATRIX_H
# define MATRIX_H

# include <stddef.h>
# include <stdint.h>

// AIOS C contiguous 2차원 행렬 데이터 구조체
typedef struct{
    int rows;
    int cols;
    float* data; // tensor arena의 물리 주소를 가리킬 연속된 포인터
} matrix_t;

// tensor arena 공간을 분할 받아서 지정된 크기의 행렬을 생성
matrix_t* matrix_create(int rows, int cols);

// 특정 좌표(row, col)에 정밀 실수 데이터 주입
void matrix_set(matrix_t* matrix, int row, int col, float value);

// cpu L1 캐시 적중률 최적화형 행렬 곱셈
int matrix_mul(const matrix_t* A, const matrix_t* B, matrix_t* C);

// matrix에서 특정 원소 가져오기
int matrix_get(const matrix_t* matrix, int row, int col, float* out_value);

# endif