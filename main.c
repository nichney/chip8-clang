// CHIP-8 interpreter
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#define MEM_SIZE 4096
#define STCK_SIZE 16
#define SCRN_SIZE 64*32

// SDL globals
SDL_Window* sdlWindow = NULL;
SDL_Renderer* sdlRenderer = NULL;
SDL_Texture* sdlTexture = NULL;

// Mapping SDL scancodes to CHIP-8 keys (adjust as needed for your desired layout)
uint8_t sdl_key_map[SDL_NUM_SCANCODES]; // Max number of scancodes

// hidden registers
int waiting_for_key = 0, key_dest = 0;
int draw_screen_flag = 0; // 1 when DRW called

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

int load_rom(const char* filename){
    // load the rom from real file into MEMORY
    FILE *rom_file = fopen(filename, "rb");
    if(rom_file == NULL){
        printf("Error: could not open ROM file '%s'\n", filename);
        return 0;
    }

    /*here we get file size*/
    fseek(rom_file, 0, SEEK_END);
    long rom_size = ftell(rom_file);
    rewind(rom_file); // go to beggining

    /*check if rom is suitable for memory*/
    if(rom_size > (MEM_SIZE - 0x200)){
        printf("Error: ROM file '$s' is too large for memory. Size: $ld bytes\n", filename, rom_size);
        fclose(rom_file);
        return 0;
    }
    
    /*read rom into memory*/
    size_t bytes_read = fread(&MEMORY[0x200], 1, rom_size, rom_file);
    if(bytes_read != rom_size){
        printf("Warning: Mismatch in bytes read for ROM '%s'. Expected %ld, got %zu\n", filename, rom_size, bytes_read);
    }

    fclose(rom_file);
    printf("Successfully loaded ROM '%s' (%ld bytes)\n", filename, rom_size);
    return 1; 

}

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

    srand(time(NULL));

    // Init SDL
    if(SDL_Init(SDL_INIT_EVERYTHING) < 0){
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create window
    sdlWindow = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 640, 320, SDL_WINDOW_SHOWN); // 10x scale
    if (sdlWindow == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create renderer
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
    if (sdlRenderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create texture for the screen (format is ARGB8888 for easier pixel manipulation)
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, 64, 32);
    if (sdlTexture == NULL) {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Clear renderer
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255); // Black background
    SDL_RenderClear(sdlRenderer);
    SDL_RenderPresent(sdlRenderer);
}

void draw_graphics() {
    // Create a pixel buffer for SDL Texture
    uint32_t pixels[SCRN_SIZE]; // ARGB8888 format (32 bits per pixel)

    for (int i = 0; i < SCRN_SIZE; i++) {
        // If pixel is on (1), make it white; otherwise, black.
        // ARGB8888: 0xAARRGGBB
        pixels[i] = SCREEN[i] ? 0xFFFFFFFF : 0xFF000000; // White: 0xFFFFFFFF, Black: 0xFF000000 (opaque alpha)
    }

    // Update the SDL texture with the pixel data
    SDL_UpdateTexture(sdlTexture, NULL, pixels, 64 * sizeof(uint32_t));

    // Clear the renderer
    SDL_RenderClear(sdlRenderer);
    // Copy the texture to the renderer (scales it to fit the window)
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
    // Present the renderer
    SDL_RenderPresent(sdlRenderer);
}

void setup_key_map() {
    // Initialize all to 0xFF (invalid key)
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        sdl_key_map[i] = 0xFF;
    }

    // Map common keyboard keys to CHIP-8 hex keypad
    // CHIP-8 Layout:
    // 1 2 3 C
    // 4 5 6 D
    // 7 8 9 E
    // A 0 B F

    // My suggested mapping (can be customized):
    // 1 2 3 4
    // Q W E R
    // A S D F
    // Z X C V

    sdl_key_map[SDL_SCANCODE_1] = 0x1;
    sdl_key_map[SDL_SCANCODE_2] = 0x2;
    sdl_key_map[SDL_SCANCODE_3] = 0x3;
    sdl_key_map[SDL_SCANCODE_4] = 0xC; // C

    sdl_key_map[SDL_SCANCODE_Q] = 0x4;
    sdl_key_map[SDL_SCANCODE_W] = 0x5;
    sdl_key_map[SDL_SCANCODE_E] = 0x6;
    sdl_key_map[SDL_SCANCODE_R] = 0xD; // D

    sdl_key_map[SDL_SCANCODE_A] = 0x7;
    sdl_key_map[SDL_SCANCODE_S] = 0x8;
    sdl_key_map[SDL_SCANCODE_D] = 0x9;
    sdl_key_map[SDL_SCANCODE_F] = 0xE; // E

    sdl_key_map[SDL_SCANCODE_Z] = 0xA; // A
    sdl_key_map[SDL_SCANCODE_X] = 0x0;
    sdl_key_map[SDL_SCANCODE_C] = 0xB; // B
    sdl_key_map[SDL_SCANCODE_V] = 0xF; // F
}


int handle_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return 0; // Signal to quit the emulator
        }
        if (event.type == SDL_KEYDOWN) {
            uint8_t chip8_key = sdl_key_map[event.key.keysym.scancode];
            if (chip8_key != 0xFF) { // If it's a mapped key
                KEYBOARD[chip8_key] = 1;
            }
        }
        if (event.type == SDL_KEYUP) {
            uint8_t chip8_key = sdl_key_map[event.key.keysym.scancode];
            if (chip8_key != 0xFF) { // If it's a mapped key
                KEYBOARD[chip8_key] = 0;
            }
        }
    }
    return 1; // Continue emulation
}


void inst_cls(){
    // CLS - clear the display
    for(int i = 0; i < SCRN_SIZE; i++) SCREEN[i] = 0; // black screen
    draw_screen_flag = 1; // clear the screen immidietly
    PC += 2;
}

void inst_ret(){
    // RET - return from a subroutine
    PC = STACK[SP]; // set the address for the top of the stack
    SP--; // substract 1 from stack pointer
    PC += 2;
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

void inst_or(uint8_t x, uint8_t y){
    // OR Vx, Vy - bitwise OR on the values Vx and Vy and store result in Vx
    V[x] = V[x] | V[y];
    PC += 2;
}

void inst_and(uint8_t x, uint8_t y){
    // AND Vx, Vy - bitwise AND on the values Vx and Vy and store result in Vx
    V[x] = V[x] & V[y];
    PC += 2;
}

void inst_xor(uint8_t x, uint8_t y){
    // XOR Vx, Vy - bitwise XOR on the values Vx and Vy and store result in Vx
    V[x] = V[x] ^ V[y];
    PC += 2;
}

void inst_add_vx_vy(uint8_t x, uint8_t y){
    // ADD Vx, Vy - The values of Vx and Vy are added together, if the result is greater that 8 bits then VF is set to 1, otherwise 0
    uint16_t sum = 0;
    sum = V[x] + V[y];
    V[x] = sum & 0x00ff; // only 8 bits are stored in V[x]
    if((sum & 0xff00) == 0x0000){ // no overflow
        V[0xf] = 0;
    }
    else{
        V[0xf] = 1;
    }
    PC += 2;
}

void inst_sub_vx_vy(uint8_t x, uint8_t y){
    // SUB Vx, Vy - Vx minus Vy and store in Vx, set VF to 1 if Vx > Vy
    if(V[x] > V[y]){
        V[0xf] = 1;
    }
    else{
        V[0xf] = 0;
    }
    V[x] -= V[y];
    PC += 2;
}

void inst_shr(uint8_t x){
    // SHR Vx - If the least-significant bit of Vx is 1, then VF is set to 1, then Vx is divided by 2
    V[0xf] = V[x] & 0x1;
    V[x] >>= 1;
    PC += 2;
}

void inst_subn_vx_vy(uint8_t x, uint8_t y){
    // SUBN Vx, Vy - Vy minus Vx and store in Vx, set VF to 1 if Vy > Vx
    if(V[x] < V[y]){
        V[0xf] = 1;
    }
    else{
        V[0xf] = 0;
    }
    V[x] = V[y] - V[x];
    PC += 2;
}

void inst_shl(uint8_t x){
    // SHL Vx - if the most-significant bit of Vx is 1, then VF = 1, then Vx multiplied by 2
    V[0xf] = (V[x] >> 7) & 0x1;
    V[x] <<=1;
    PC += 2;
}

void inst_ld_addr(uint16_t addr){
    // LD I, addr - I is set to addr
    I = addr;
    PC += 2;
}

void inst_jp_v0(uint16_t addr){
    // JP V0, addr - jump to location addr + V0
    PC = addr + V[0x0];
}

void inst_rnd(uint8_t x, uint8_t kk){
    // RND Vx, byte - Generate random number, AND it with kk and store in Vx
    int randomNumber = (rand() % 256) & kk;
    V[x] = randomNumber;
    PC += 2;
}

void inst_drw(uint8_t x, uint8_t y, uint8_t n){
    // DRW Vx, Vy, nibble - display n-byte sprite starting at memory location I at (Vx, Vx), set VF = collision
    uint8_t vx = V[x] % 64; // width
    uint8_t vy = V[y] % 32; // height
    V[0xf] = 0; // reset the collision flag
    for(int row = 0; row < n; row++){
        uint8_t sprite_byte = MEMORY[I+row];
        for(int col = 0; col < 8; col++){
            uint8_t pixel_sprite = (sprite_byte >> (7 - col)) & 0x1;

            uint8_t x_coord = (vx + col) % 64;
            uint8_t y_coord = (vy + row) % 32;
            int screen_index = y_coord * 64 + x_coord;

            if(pixel_sprite){
                if(SCREEN[screen_index] == 1){
                    V[0xf] = 1; // collision detected
                }
                SCREEN[screen_index] ^= 1;
            }
        }
    }
    draw_screen_flag = 1;
    PC += 2;
}

void inst_skp(uint8_t x){
    // SKP Vx - skip next instruction if key is pressed (key value is stored in Vx)
    if(KEYBOARD[V[x]]){
        PC += 4;
    }
    else{
        PC += 2;
    }
}

void inst_sknp(uint8_t x){
    // SKNP Vx - skip next instruction if key is not pressed
    if(!KEYBOARD[V[x]]){
        PC += 4;
    }
    else{
        PC += 2;
    }
}

void inst_ld_dt(uint8_t x){
    // LD Vx, DT - set Vx to delay timer value
    V[x] = DT;
    PC += 2;
}

void inst_ld_k(uint8_t x){
    // LD Vx, K - wait for a key press, store the value of key in Vx
    waiting_for_key = 1;
    key_dest = x;
}

void inst_dt_ld(uint8_t x){
    // LD DT, Vx - set display timer to Vx
    DT = V[x];
    PC += 2;
}

void inst_st_ld(uint8_t x){
    // LD ST, Vx - set sound timer to Vx
    ST = V[x];
    PC += 2;
}

void inst_add_i(uint8_t x){
    // ADD I, Vx - set I = I + Vx
    I += V[x];
    PC += 2;
}

void inst_f_ld(uint8_t x){
    // LD F, Vx - set I = location of sprite for digit Vx
    I = V[x] * 5; // sprites are 5 bytes long
    PC += 2;
}

void inst_bcd_ld(uint8_t x){
    // LD B, Vx - store BCD representation of Vx in memory locations I, I+1 and I+2
    MEMORY[I] = (V[x] % 1000 - V[x] % 100) / 100;
    MEMORY[I+1] = (V[x] % 100 - V[x] % 10) / 10;
    MEMORY[I+2] = V[x] % 10;
    PC += 2;
}

void inst_store_registers(uint8_t x){
    // LD [I], Vx - copy registers to memory
    for(int i = 0; i < x+1; i++){
        MEMORY[I+i] = V[i];
    }
    PC += 2;
}

void inst_read_registers(uint8_t x){
    // LD Vx, [I] - read from memory to registers
    for(int i = 0; i < x+1; i++){
        V[i] = MEMORY[I+i];
    }
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
                //inst_jp(nnn);
                PC += 2;
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
                    inst_or(ss, ts);
                    break;
                case 0x2: // AND Vx, Vy
                    inst_and(ss, ts);
                    break;
                case 0x3: // XOR Vx, Vy
                    inst_xor(ss, ts);
                    break;
                case 0x4: // ADD Vx, Vy
                    inst_add_vx_vy(ss, ts);
                    break;
                case 0x5: // SUB Vx, Vy
                    inst_sub_vx_vy(ss, ts);
                    break;
                case 0x6: // SHR Vx, Vy
                    inst_shr(ss);
                    break;
                case 0x7: // SUBN Vx, Vy
                    inst_subn_vx_vy(ss, ts);
                    break;
                case 0xe: // SHL Vx, Vy
                    inst_shl(ss);
                    break;
                default:
                    inst_cls();
                    break;
            }
            break;
        case 0x9: // SNE Vx (x=ss), Vy (y=ts)
            inst_sne(ss, V[ts]);
            break;
        case 0xa: // LD I, addr ([0]=ss, [1,2]=sb)
            inst_ld_addr(nnn);
            break;
        case 0xb: // JP V0, addr ([0]=ss, [1,2]=sb)
            inst_jp_v0(nnn);
            break;
        case 0xc: // RND Vx (x=ss), byte (kk=sb)
            inst_rnd(ss, sb);
            break;
        case 0xd: // DRW Vx (x=ss), Vy (y=ts), nibble (nibble=ffs)
            inst_drw(ss, ts, ffs);
            break;
        case 0xe:
            if(sb == 0x9e){
                // SKP Vx (x=ss)
                inst_skp(ss);
            }
            else if (sb == 0xa1){
                // SKNP Vx (x=ss)
                inst_sknp(ss);
            }
            break;
        case 0xf:
            switch(sb){
                case 0x07: // LD Vx, DT
                    inst_ld_dt(ss);
                    break;
                case 0x0a: // LD Vx, K
                    inst_ld_k(ss);
                    break;
                case 0x15: // LD DT, Vx
                    inst_dt_ld(ss);
                    break;
                case 0x18: // LD ST, Vx
                    inst_st_ld(ss);
                    break;
                case 0x1e: // ADD I, Vx
                    inst_add_i(ss);
                    break;
                case 0x29: // LD F, Vx
                    inst_f_ld(ss);
                    break;
                case 0x33: // LD B, Vx
                    inst_bcd_ld(ss);
                    break;
                case 0x55: // LD [I], Vx
                    inst_store_registers(ss);
                    break;
                case 0x65: // LD Vx, [I]
                    inst_read_registers(ss);
                    break;
                default:
                    inst_cls();
                    break;
            }
            break;
        default:
            inst_cls();
            break;
    }
}


int main(int argc, char* argv[]){
    // Check if a ROM file path was provided as a command-line argument
    if(argc < 2){
        printf("Usage: %s <path_to_rom>\n", argv[0]);
        return 1;
    }

    // Initialize the CHIP-8 machine and SDL
    init_machine();
    // Set up the keyboard mapping for SDL scancodes to CHIP-8 keys
    setup_key_map();

    // Attempt to load the specified ROM file
    if(!load_rom(argv[1])){
        // If ROM loading fails, exit with an error
        return 1;
    }

    // Main emulation loop variables
    int quit = 0; // Flag to control the main loop (0 to continue, 1 to quit)

    // Timestamps for managing frame rate and timer updates
    uint32_t last_timer_update = SDL_GetTicks(); // Time of last timer decrement
    uint32_t last_frame_time = SDL_GetTicks();   // Time of last screen render/frame sync

    // Configuration for emulation speed: number of CHIP-8 cycles to run per 60Hz frame
    const int CYCLES_PER_FRAME = 10;
    int cycles_executed_this_frame = 0; // Counter for cycles executed within the current frame

    // Main emulation loop
    while(!quit){
        // Handle user input (keyboard and window close events)
        if(!handle_input()){
            quit = 1; // If handle_input returns 0, quit the emulator
            continue; // Skip the rest of the loop and terminate
        }

        // --- CHIP-8 Emulation Cycle ---
        if(!waiting_for_key){
            // If not waiting for a key press, execute opcodes
            if (cycles_executed_this_frame < CYCLES_PER_FRAME) {
                // Fetch the 16-bit opcode from memory (PC points to the first byte)
                uint16_t opcode = MEMORY[PC] << 8 | MEMORY[PC + 1];
                decodeAndExecute(opcode); // Decode and execute the instruction
                cycles_executed_this_frame++; // Increment cycle counter
            }

            // If a DRW instruction was executed and signaled a screen redraw
            if(draw_screen_flag){
                draw_graphics();      // Perform the drawing operation
                draw_screen_flag = 0; // Reset the flag
            }
        } else {
            // If the emulator is in a 'waiting for key' state (FX0A instruction)
            int key_found = 0;
            // Iterate through all possible CHIP-8 keys to check for a press
            for(int i = 0; i < 16; i++){
                if(KEYBOARD[i]){ // If a key is currently pressed
                    V[key_dest] = i;       // Store the pressed key's value in the target Vx register
                    waiting_for_key = 0;   // Reset the waiting flag
                    PC += 2;               // Advance PC (as specified by FX0A behavior)
                    key_found = 1;         // Mark that a key was found
                    break;                 // Exit the loop (only one key is needed)
                }
            }
            // If no key was found yet, pause briefly and continue waiting
            if (!key_found) {
                SDL_Delay(10); // Small delay to prevent busy-waiting while waiting for key
                continue;      // Skip timer updates and frame rate control until a key is pressed
            }
        }

        // --- Timer Update and Frame Rate Control ---
        uint32_t current_time = SDL_GetTicks();

        // Update timers (DT and ST) at approximately 60 Hz
        if (current_time - last_timer_update >= (1000 / 60)) {
            if (DT > 0) DT--; // Decrement Delay Timer if active
            if (ST > 0) {
                ST--; // Decrement Sound Timer if active
                // TODO: Add actual sound output here when ST > 0 (e.g., play a beep)
            }
            last_timer_update = current_time; // Reset timer update timestamp
        }

        // Synchronize drawing and reset cycle counter for the next frame
        // This ensures the display updates at a consistent rate (approx. 60 FPS)
        if (current_time - last_frame_time >= (1000 / 60)) {
            cycles_executed_this_frame = 0; // Reset cycles for the new frame
            last_frame_time = current_time;   // Reset frame time
        } else {
            // If we're ahead of schedule for the 60FPS frame rate,
            // introduce a small delay to prevent busy-waiting.
            SDL_Delay(1);
        }
    }

    // --- Cleanup SDL Resources ---
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit(); // Quit SDL subsystems

    return 0; // Program exited successfully
}
