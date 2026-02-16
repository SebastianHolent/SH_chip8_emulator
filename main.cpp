#include <iostream>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <random>
#include <string>
#include <cstring>
#include "raylib.h"

const int videoBufferWidth = 64;
const int videoBufferHeight = 32;
const int videoScale = 10;
const int screenWidth = videoBufferWidth * videoScale;
const int screenHeight = videoBufferHeight * videoScale;
bool quit = false;


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

class Chip8
{
public:
    uint8_t memory[4096];
    uint8_t V[16];
    uint16_t i_reg;
    uint16_t PC;
    uint16_t stack[16];
    uint8_t stack_pointer;
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t keypad[16];
    uint32_t display[64 * 32]{};
    uint16_t opcode;

    const uint16_t STARTING_ADRESS = 0x200;
    const uint8_t MEMORY_FONT_START_ADRESS = 0x50;

    std::default_random_engine randGen;
    std::uniform_int_distribution<uint8_t> randByte;


    typedef void (Chip8::*Chip8Func)();

    Chip8Func table[0xF + 1];
    Chip8Func table0[0xE + 1];
    Chip8Func table8[0xE + 1];
    Chip8Func tableE[0xE + 1];
    Chip8Func tableF[0x65 + 1];

    Chip8(): 
        randGen(std::chrono::system_clock::now().time_since_epoch().count()), 
        randByte(std::uniform_int_distribution<uint8_t>(0, 255U))
    {
        PC = 0;
        opcode = 0;
        i_reg = 0;
        stack_pointer = 0;
        delay_timer = 0;
        sound_timer = 0;

        table[0x0] = &Chip8::Table0;
        table[0x1] = &Chip8::OP_1nnn;
        table[0x2] = &Chip8::OP_2nnn;
        table[0x3] = &Chip8::OP_3xkk;
        table[0x4] = &Chip8::OP_4xkk;
        table[0x5] = &Chip8::OP_5xy0;
        table[0x6] = &Chip8::OP_6xkk;
        table[0x7] = &Chip8::OP_7xkk;
        table[0x8] = &Chip8::Table8;
        table[0x9] = &Chip8::OP_9xy0;
        table[0xA] = &Chip8::OP_Annn;
        table[0xB] = &Chip8::OP_Bnnn;
        table[0xC] = &Chip8::OP_Cxkk;
        table[0xD] = &Chip8::OP_Dxyn;
        table[0xE] = &Chip8::TableE;
        table[0xF] = &Chip8::TableF;

        for (size_t i = 0; i <= 0xE; i++)
        {
            table0[i] = &Chip8::OP_NULL;
            table8[i] = &Chip8::OP_NULL;
            tableE[i] = &Chip8::OP_NULL;
        }

        table0[0x0] = &Chip8::OP_00E0;
        table0[0xE] = &Chip8::OP_00EE;

        table8[0x0] = &Chip8::OP_8xy0;
        table8[0x1] = &Chip8::OP_8xy1;
        table8[0x2] = &Chip8::OP_8xy2;
        table8[0x3] = &Chip8::OP_8xy3;
        table8[0x4] = &Chip8::OP_8xy4;
        table8[0x5] = &Chip8::OP_8xy5;
        table8[0x6] = &Chip8::OP_8xy6;
        table8[0x7] = &Chip8::OP_8xy7;
        table8[0xE] = &Chip8::OP_8xyE;

        tableE[0x1] = &Chip8::OP_ExA1;
        tableE[0xE] = &Chip8::OP_Ex9E;

        for (size_t i = 0; i <= 0x65; i++)
        {
            tableF[i] = &Chip8::OP_NULL;
        }

        tableF[0x07] = &Chip8::OP_Fx07;
        tableF[0x0A] = &Chip8::OP_Fx0A;
        tableF[0x15] = &Chip8::OP_Fx15;
        tableF[0x18] = &Chip8::OP_Fx18;
        tableF[0x1E] = &Chip8::OP_Fx1E;
        tableF[0x29] = &Chip8::OP_Fx29;
        tableF[0x33] = &Chip8::OP_Fx33;
        tableF[0x55] = &Chip8::OP_Fx55;
        tableF[0x65] = &Chip8::OP_Fx65;
    }

    void Table0()
    {
        ((*this).*(table0[opcode & 0x000Fu]))();
    }

    void Table8()
    {
        ((*this).*(table8[opcode & 0x000Fu]))();
    }

    void TableE()
    {
        ((*this).*(tableE[opcode & 0x000Fu]))();
    }

    void TableF()
    {
        ((*this).*(tableF[opcode & 0x00FFu]))();
    }

    void OP_NULL()
    {}

    void LoadFonts(){
        for(int i = 0; i <= 79; i++){
            memory[MEMORY_FONT_START_ADRESS + i] = fontset[i];
        };
    };

    void LoadRom(char const* filename){
        std::ifstream file(filename, std::ios::binary | std::ios::ate);

        if(file.is_open()){
            std::streampos size = file.tellg();
            char* buffer = new char[size];

            file.seekg(0, std::ios::beg);
            file.read(buffer, size);
            file.close();

            for(long i = 0; i < size; i ++){
                memory[STARTING_ADRESS + i] = buffer[i];
            }

            delete[] buffer;

            std::cout << "Rom loaded" << '\n';
        }
    };

    void Init(){
        PC = STARTING_ADRESS;
        LoadFonts();
        LoadRom("Tetris [Fran Dachille, 1991].ch8");

    }

    void OP_00E0() {
        memset(display, 0, sizeof(display));
    }

    void OP_00EE() {
        --stack_pointer;
        PC = stack[stack_pointer];
    }
    
    void OP_1nnn() { 
        uint16_t address = opcode & 0x0FFFu;
        PC = address;
    }

    void OP_2nnn() { 
        uint16_t address = opcode & 0x0FFFu;
        stack[stack_pointer] = PC;
        ++stack_pointer;
        PC = address;
    }

    void OP_3xkk() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;
    
        if(V[Vx] == byte){
            PC += 2;
        }
    }

    void OP_4xkk() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;
    
        if(V[Vx] != byte){
            PC += 2;
        }
    }

    void OP_5xy0() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        if(V[Vx] == V[Vy]){
            PC += 2;
        }
    }

    void OP_6xkk() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;

        V[Vx] = byte;
    }

    void OP_7xkk() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;

        V[Vx] += byte;
    }

    void OP_8xy0() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        V[Vx] = V[Vy];
    }

    void OP_8xy1() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        V[Vx] |= V[Vy];
    }

    void OP_8xy2() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        V[Vx] &= V[Vy];
    }

    void OP_8xy3() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        V[Vx] ^= V[Vy];
    }

    void OP_8xy4() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        uint16_t sum = V[Vx] + V[Vy];

        if(sum > 255U){
            V[0xF] = 1;
        }
        else{
            V[0xF] = 0;
        }

        V[Vx] = sum & 0xFFu;
    }

    void OP_8xy5() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        if(V[Vx] > V[Vy]){
            V[0xF] = 1;
        }
        else{
            V[0xF] = 0;
        }

        V[Vx] -= V[Vy];
    }

    void OP_8xy6() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;

        V[0xF] = (V[Vx] & 0x1u);
        V[Vx] >>= 1;
    }

    void OP_8xy7() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        if(V[Vy] > V[Vx]){
            V[0xF] = 1;
        }
        else{
            V[0xF] = 0;
        }

        V[Vx] = V[Vy] - V[Vx];
    }    

    void OP_8xyE() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;

        V[0xF] = (V[Vx] & 0x80u) >> 7u;
        V[Vx] <<= 1;
    }

    void OP_9xy0() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;

        if(V[Vx] != V[Vy]){
            PC += 2;
        }
    }

    void OP_Annn() {
        uint16_t address = opcode & 0x0FFFu;
        i_reg = address;
    }

    void OP_Bnnn() {
        uint16_t address = opcode & 0x0FFFu;
        PC = V[0] + address;
    }

    void OP_Cxkk() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t byte = opcode & 0x00FFu;

        V[Vx] = randByte(randGen) & byte;
    }

    void OP_Dxyn() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t Vy = (opcode & 0x00F0u) >> 4u;
        uint8_t height = opcode & 0x000Fu;    
        
        uint8_t xPos = V[Vx] % videoBufferWidth;
        uint8_t yPos = V[Vy] % videoBufferHeight;

        V[0xF] = 0;

        for(unsigned int row = 0; row < height; ++row)
        {
            uint8_t spriteByte = memory[i_reg + row];

            for(unsigned int col = 0; col < 8; ++col){
                uint8_t spritePixel = spriteByte & (0x80u >> col);
                uint32_t* screenPixel = &display[(yPos + row) * videoBufferWidth + (xPos + col)];

                if (spritePixel)
                {
                    if (*screenPixel == 0xFFFFFFFF)
                    {
                        V[0xF] = 1;
                    }

                    *screenPixel ^= 0xFFFFFFFF;
                }
            }
        }
    }

    void OP_Ex9E() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t key = V[Vx];

        if (keypad[key])
        {
            PC += 2;
        }
    }

    void OP_ExA1() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t key = V[Vx];

        if (!keypad[key])
        {
            PC += 2;
        }
    }

    void OP_Fx07() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        V[Vx] = delay_timer;
    }

    void OP_Fx0A() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;

        if (keypad[0])
        {
            V[Vx] = 0;
        }
        else if (keypad[1])
        {
            V[Vx] = 1;
        }
        else if (keypad[2])
        {
            V[Vx] = 2;
        }
        else if (keypad[3])
        {
            V[Vx] = 3;
        }
        else if (keypad[4])
        {
            V[Vx] = 4;
        }
        else if (keypad[5])
        {
            V[Vx] = 5;
        }
        else if (keypad[6])
        {
            V[Vx] = 6;
        }
        else if (keypad[7])
        {
            V[Vx] = 7;
        }
        else if (keypad[8])
        {
            V[Vx] = 8;
        }
        else if (keypad[9])
        {
            V[Vx] = 9;
        }
        else if (keypad[10])
        {
            V[Vx] = 10;
        }
        else if (keypad[11])
        {
            V[Vx] = 11;
        }
        else if (keypad[12])
        {
            V[Vx] = 12;
        }
        else if (keypad[13])
        {
            V[Vx] = 13;
        }
        else if (keypad[14])
        {
            V[Vx] = 14;
        }
        else if (keypad[15])
        {
            V[Vx] = 15;
        }
        else
        {
            PC -= 2;
        }
    }

    void OP_Fx15() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        delay_timer = V[Vx];
    }

    void OP_Fx18() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        sound_timer = V[Vx];
    }

    void OP_Fx1E() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        i_reg += V[Vx];
    }

    void OP_Fx29() {
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t digit = V[Vx];

        i_reg = MEMORY_FONT_START_ADRESS + (5 * digit);
    }

    void OP_Fx33() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;
        uint8_t value = V[Vx];

        memory[i_reg + 2] = value % 10;
        value /= 10;

        memory[i_reg + 1] = value % 10;
        value /= 10;

        memory[i_reg] = value % 10;
    }

    void OP_Fx55() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;

        for (uint8_t i = 0; i <= Vx; ++i)
        {
            memory[i_reg + i] = V[i];
        }        
    }

    void OP_Fx65() { 
        uint8_t Vx = (opcode & 0x0F00u) >> 8u;

        for (uint8_t i = 0; i <= Vx; ++i)
        {
            V[i] = memory[i_reg + i];
        }        
    }
    void updateDisplay(){
        for(int y = 0; y < videoBufferHeight; y++){
            for(int x = 0; x < videoBufferWidth; x++){
                int pixelIndex = y * videoBufferWidth + x;
                
                if(display[pixelIndex] != 0){  
                    DrawRectangle(x * videoScale, y * videoScale, videoScale, videoScale, WHITE);
                }
            }
        }
    }
    bool ProcessInput(uint8_t* keys)
    {
        if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE))
        {
            quit = true;
        }

        if (IsKeyPressed(KEY_X)) { keys[0] = 1; std::cout << "X key was pressed\n"; }
        if (IsKeyPressed(KEY_ONE)) { keys[1] = 1; std::cout << "1 key was pressed\n"; }
        if (IsKeyPressed(KEY_TWO)) { keys[2] = 1; std::cout << "2 key was pressed\n"; }
        if (IsKeyPressed(KEY_THREE)) { keys[3] = 1; std::cout << "3 key was pressed\n"; }
        if (IsKeyPressed(KEY_Q)) { keys[4] = 1; std::cout << "Q key was pressed\n"; }
        if (IsKeyPressed(KEY_W)) { keys[5] = 1; std::cout << "W key was pressed\n"; }
        if (IsKeyPressed(KEY_E)) { keys[6] = 1; std::cout << "E key was pressed\n"; }
        if (IsKeyPressed(KEY_A)) { keys[7] = 1; std::cout << "A key was pressed\n"; }
        if (IsKeyPressed(KEY_S)) { keys[8] = 1; std::cout << "S key was pressed\n"; }
        if (IsKeyPressed(KEY_D)) { keys[9] = 1; std::cout << "D key was pressed\n"; }
        if (IsKeyPressed(KEY_Z)) { keys[0xA] = 1; std::cout << "Z key was pressed\n"; }
        if (IsKeyPressed(KEY_C)) { keys[0xB] = 1; std::cout << "C key was pressed\n"; }
        if (IsKeyPressed(KEY_FOUR)) { keys[0xC] = 1; std::cout << "4 key was pressed\n"; }
        if (IsKeyPressed(KEY_R)) { keys[0xD] = 1; std::cout << "R key was pressed\n"; }
        if (IsKeyPressed(KEY_F)) { keys[0xE] = 1; std::cout << "F key was pressed\n"; }
        if (IsKeyPressed(KEY_V)) { keys[0xF] = 1; std::cout << "V key was pressed\n"; }

        if (IsKeyReleased(KEY_X)) { keys[0] = 0; std::cout << "X key was released\n"; }
        if (IsKeyReleased(KEY_ONE)) { keys[1] = 0; std::cout << "1 key was released\n"; }
        if (IsKeyReleased(KEY_TWO)) { keys[2] = 0; std::cout << "2 key was released\n"; }
        if (IsKeyReleased(KEY_THREE)) { keys[3] = 0; std::cout << "3 key was released\n"; }
        if (IsKeyReleased(KEY_Q)) { keys[4] = 0; std::cout << "Q key was released\n"; }
        if (IsKeyReleased(KEY_W)) { keys[5] = 0; std::cout << "W key was released\n"; }
        if (IsKeyReleased(KEY_E)) { keys[6] = 0; std::cout << "E key was released\n"; }
        if (IsKeyReleased(KEY_A)) { keys[7] = 0; std::cout << "A key was released\n"; }
        if (IsKeyReleased(KEY_S)) { keys[8] = 0; std::cout << "S key was released\n"; }
        if (IsKeyReleased(KEY_D)) { keys[9] = 0; std::cout << "D key was released\n"; }
        if (IsKeyReleased(KEY_Z)) { keys[0xA] = 0; std::cout << "Z key was released\n"; }
        if (IsKeyReleased(KEY_C)) { keys[0xB] = 0; std::cout << "C key was released\n"; }
        if (IsKeyReleased(KEY_FOUR)) { keys[0xC] = 0; std::cout << "4 key was released\n"; }
        if (IsKeyReleased(KEY_R)) { keys[0xD] = 0; std::cout << "R key was released\n"; }
        if (IsKeyReleased(KEY_F)) { keys[0xE] = 0; std::cout << "F key was released\n"; }
        if (IsKeyReleased(KEY_V)) { keys[0xF] = 0; std::cout << "V key was released\n"; }

        return quit;
    }
    void Cycle(){
        opcode = (memory[PC] << 8u) | memory[PC + 1];

        PC += 2;

        ((*this).*(table[(opcode & 0xF000u) >> 12u]))();

        if (delay_timer > 0)
        {
            --delay_timer;
        }

        if (sound_timer > 0)
        {
            --sound_timer;
        }        
    }

};


Chip8 chip8;

int main(int argc, char *argv[]){
    if (argc != 2)
        {
            std::cerr << "Usage: " << argv[0] << "<Delay> \n";
            std::exit(EXIT_FAILURE);
        }
    int cycleDelay = std::stoi(argv[1]);
    
    InitWindow(screenWidth, screenHeight, "CHIP-8 Emulator");
        
    chip8.Init();

    SetTargetFPS(60);
    auto lastCycleTime = std::chrono::high_resolution_clock::now();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        chip8.ProcessInput(chip8.keypad);

		auto currentTime = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - lastCycleTime).count();

		if (dt > cycleDelay)
		{
			lastCycleTime = currentTime;

			chip8.Cycle();
		}
        
        chip8.updateDisplay();
        
        EndDrawing();
    }

    CloseWindow();
  return 0;
}