// kernel/keyboard.h
# ifndef KEYBOARD_H
# define KEYBOARD_H

// PS/2 키보드 및 i8042 컨트롤러 초기화
int keyboard_init(void);
// 성공 -> 1
// 실패 -> 0

// IRQ1 어셈블리 래퍼에서 호출하는 c언어 핸들러
void keyboard_handler(void);

# endif