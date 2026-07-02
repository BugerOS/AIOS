// kernel/matrix.c
// cpu가 행렬의 데이터를 메모리 수직 점프 없이 가로 방향 스트리밍으로만
// 연속 스캔하게 강제하는 cache friendly core engine

# include "matrix.h"
# include "arena.h"

// tensor arena 공간을 분할 받아서 지정된 크기의 행렬을 생성
matrix_t* matrix_create(int rows, int cols){
    if(rows<=0 || cols<=0){
        return NULL;
    }

    // arena에서 구조체 공간 확보
    matrix_t* matrix = (matrix_t*)arena_alloc(sizeof(matrix_t));
    if(matrix == NULL){
        return NULL;
    }

    matrix->rows = rows;
    matrix->cols = cols;
    // arena에서 원소들이 할당될 c-contiguous 공간 확보
    matrix->data = (float*)arena_alloc(rows * cols * sizeof(float));
    if(matrix->data == NULL){
        return NULL;
    }
    else{
        return matrix;
    }
}

// 특정 좌표(row, col)에 정밀 실수 데이터 주입
void matrix_set(matrix_t* matrix, int row, int col, float value){
    if(row>=0 && row<(matrix->rows) && col>=0 && col<(matrix->cols)){
        matrix->data[row * matrix->cols + col] = value;
    }

}

// cpu L1 캐시 적중률 최적화형 행렬 곱셈
int matrix_mul(const matrix_t* A, const matrix_t* B, matrix_t* C){
    // matrix_t* C = matrix_create(A->rows, B->cols);
    // free 구현 x -> 임시 연산이나 반복문 루프에서도 메모리 낭비
    // -> 안정성 x

    // 행렬 연산 가능 여부 확인 (A의 열 크기 == B의 행 크기)
    if(A->cols != B->rows || C->rows != A->rows || C->cols != B->cols){
        return 0; //오류
    }

    for (int i = 0; i < C->rows * C->cols; i++) {
        C->data[i] = 0.0f;
    }

    for (int i = 0; i < A->rows; i++) {
        for (int k = 0; k < A->cols; k++) {
            float a_val = A->data[i * A->cols + k];
            
            for (int j = 0; j < B->cols; j++) {
                C->data[i * C->cols + j] += a_val * B->data[k * B->cols + j];
            }
        }
    }

    return 1;
}

// matrix에서 특정 원소 가져오기
int matrix_get(const matrix_t* matrix, int row, int col, float* out_value){
    if(row>=0 && row<(matrix->rows) && col>=0 && col<(matrix->cols)){
        *out_value = matrix->data[row * matrix->cols + col];
        return 1;
    }
    else{
        return 0;
    }
}