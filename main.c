// CHIP-8 interpreter
#include <stdint.h>

#define MEM_SIZE 4096
#define STCK_SIZE 16
#define SCRN_SIZE 64*32

// common-use registers
uint8_t V[16]; //V[0]-V[0xF]
uint8_t DT, ST; // delay timer and song timer
uint16_t PC, I; // instruction pointer and memory register
uint8_t SP; // stack pointer

uint8_t MEMORY[MEM_SIZE];
uint16_t STACK[STCK_SIZE]; // stack
uint8_t SCREEN[SCRN_SIZE]; // screen
uint8_t KEYBOARD[16]; 

void init_machine(void)
{
    /*Init all variables and memory*/
    PC = 0x200;
    SP = 0;
    DT = 0;
    ST = 0;

    for(int i = 0; i < MEM_SIZE; i++) MEMORY[i] = 0;
    for(int i = 0; i < 16; i++) {
        // initialize V and keyboard in one loop because they have same size
        V[i] = 0;
        KEYBOARD[i] = 0;
    }
    for(int i = 0; i < SCRN_SIZE; i++) SCREEN[i] = 0; //black screen
    for(int i = 0; i < STCK_SIZE; i++) STACK[i] = 0;

    // TODO: add first 512 hardcored bytes into memory
}

int main(void){
    
    // main cycle
    while(1){
        uint16_t opcode = MEMORY[PC] << 8 | MEMORY[PC + 1]; // memory consists of 8-bits chunks, so we need to read 2
        // TODO: decode
        PC += 2;
    }

    return 0;
}
