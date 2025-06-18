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

const uint8_t fontset[80] = {
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


void init_machine(void)
{
    /*Init all variables and memory*/
    PC = 0x200;
    SP = 0;
    I = 0;
    DT = 0;
    ST = 0;
    
    for(int i = 0; i < 80; i++) MEMORY[i] = fontset[i]; // load fonts into memory
    for(int i = 80; i < MEM_SIZE; i++) MEMORY[i] = 0;
    for(int i = 0; i < 16; i++) {
        // initialize V and keyboard in one loop because they have same size
        V[i] = 0;
        KEYBOARD[i] = 0;
    }
    for(int i = 0; i < SCRN_SIZE; i++) SCREEN[i] = 0; //black screen
    for(int i = 0; i < STCK_SIZE; i++) STACK[i] = 0;
}

void inst_cls(){
    // CLS - clear the display
    for(int i = 0; i < SCRN_SIZE; i++) SCREEN[i] = 0; // black screen
    PC += 2;
}

void inst_ret(){
    // RET - return from a subroutine
    PC = STACK[SP]; // set the address for the top of the stack
    SP--; // substract 1 from stack pointer
}

void inst_jp(uint16_t address){
    // JP addr - jump to a location address
    PC = address;
}

void inst_call(uint16_t address){
    // CALL addr - call a subroutine at addr
    SP += 1;
    STACK[SP] = PC;
    PC = address;
}

void inst_se(uint8_t x, uint8_t kk){
    // SE Vx, byte - skip next instruction if Vx = kk
    if(V[x] == kk) PC += 4;
    else{
        PC += 2;
    }
}

void inst_sne(uint8_t x, uint8_t kk){
    // SNE Vx, byte - skip next instruction if Vx != kk
    if(V[x] != kk) PC += 4;
    else{
        PC += 2;
    }
}

void inst_ld(uint8_t x, uint8_t kk){
    // LD Vx, byte - put the value kk into register Vx
    V[x] = kk;
    PC += 2;
}

void inst_add(uint8_t x, uint8_t kk){
    // ADD Vx, byte - adds the value byte to value of register Vx and stores result in Vx
    V[x] += kk;
    PC += 2;
}

void decodeAndExecute(uint16_t opcode){
    /*
    opcode = 
               fb      sb
            ________|________
            8 bits  | 8 bits

    fb =
            fs    ss
            ____|____
            4-b | 4-b

    sb = 
            ts    fs
            ____|____
            4-b | 4-b
     */
    uint8_t fb = (opcode >> 8) & 0xff; //first byte
    uint8_t fs = (fb >> 4) & 0x0f; //first symbol
    uint8_t ss = fb & 0x0f; //second symbol

    uint8_t sb = opcode & 0x00ff; //second byte
    uint8_t ts = (sb >> 4) & 0x0f; // third symbol
    uint8_t ffs = sb & 0x0f; // fourth symbol
    // some instructions requires 2-nd, 3-rd and 4-th bytes as address
    uint16_t nnn = opcode & 0x0fff;

    switch(fs){
        case 0x0: // CLS or SYS or RET
            if(sb == 0xe0){
                inst_cls();
            }
            else if(sb == 0xee){
                inst_ret();
            }
            else{
                // TODO: SYS addr ([0]=ss, [1]=ts, [2]=ffs)
                // As Cowgos's Technical reference says, this instruction is only used on the old computers and is not supported in modern interpreters, but I will add it for backward compatibility
            }
            break;
        case 0x1: // JP addr ([0]=ss, [1]=ts, [2] = ffs)
            inst_jp(nnn);
            break;
        case 0x2: // CALL addr ([0]=ss, [1,2]=sb)
            inst_call(nnn);
            break;
        case 0x3: // SE Vx (x=ss), byte (byte=sb)
            inst_se(ss, sb);
            break;
        case 0x4: // SNE Vx (x=ss), byte (byte=sb)
            inst_sne(ss, sb);
            break;
        case 0x5: // SE Vx (x=ss), Vy (y=ts)
            inst_se(ss, V[ts]);
            break;
        case 0x6: // LD Vx (x=ss), byte (byte=sb)
            inst_ld(ss, sb);
            break;
        case 0x7: // ADD Vx (xx=ss), byte (byte=sb)
            inst_add(ss, sb);
            break;
        case 0x8: 
            switch(ffs){
                case 0x0: // LD Vx (x=ss), Vy (y=ts)
                    inst_ld(ss, V[ts]);
                    break;
                case 0x1: // OR Vx (x=ss), Vy (y=ts)
                    break;
                case 0x2: // AND Vx, Vy
                    break;
                case 0x3: // XOR Vx, Vy
                    break;
                case 0x4: // ADD Vx, Vy
                    break;
                case 0x5: // SUB Vx, Vy
                    break;
                case 0x6: // SHR Vx, Vy
                    break;
                case 0x7: // SUBN Vx, Vy
                    break;
                case 0xe: // SHL Vx, Vy
                    break;
            }
            break;
        case 0x9: // SNE Vx (x=ss), Vy (y=ts)
            break;
        case 0xa: // LD I, addr ([0]=ss, [1,2]=sb)
            break;
        case 0xb: // JP V0, addr ([0]=ss, [1,2]=sb)
            break;
        case 0xc: // RND Vx (x=ss), byte (kk=sb)
            break;
        case 0xd: // DRW Vx (x=ss), Vy (y=ts), nibble (nibble=ffs)
            break;
        case 0xe:
            if(sb == 0x9e){
                // SKP Vx (x=ss)
            }
            else if (sb == 0xa1){
                // SKNP Vx (x=ss)
            }
            break;
        case 0xf:
            switch(sb){
                case 0x07: // LD Vx, DT
                    break;
                case 0x0a: // LD Vx, K
                    break;
                case 0x15: // LD DT, Vx
                    break;
                case 0x18: // LD ST, Vx
                    break;
                case 0x1e: // ADD I, Vx
                    break;
                case 0x29: // LD F, Vx
                    break;
                case 0x33: // LD B, Vx
                    break;
                case 0x55: // LD [I], Vx
                    break;
                case 0x65: // LD Vx, [I]
                    break;
            }
            break;
    }
}

int main(void){
    
    init_machine();
    // main cycle
    while(1){
        uint16_t opcode = MEMORY[PC] << 8 | MEMORY[PC + 1]; // memory consists of 8-bits chunks, so we need to read 2
        decodeAndExecute(opcode); 
    }

    return 0;
}
