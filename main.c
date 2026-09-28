#include<stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define REFRESH_DISPLAY 1
#define KEYPRESS_NOT_READY 2
#define INSTRUCTION_RATES 660
#define FPS 60.0

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;


bool vF_reset_quirk = true; 
bool memory_quirk = true; 
bool clipping_quirk = true;
bool shifting_quirk = false;
bool jump_quirk = false;
bool display_wait_quirk = true; 




typedef struct{
    uint8_t memory[4096];
    uint32_t displayArray[32][64];
    uint16_t PC;
    uint16_t I;
    uint16_t stack[16];
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t V[16];
    uint8_t keypad[16];
    
}components;

uint16_t push(uint16_t value, uint16_t stack[16]){

    for(int i = 0; i < 16;i++){
        if(stack[i] == 0){
            stack[i] = value;
            return 1;
        }
    }
    return 0;
}

uint16_t pop(uint16_t stack[16]){
    uint16_t value;
    for(int i = 15; i >= 0; i--){
        if(stack[i] != 0){
            value = stack[i];
            stack[i] = 0;
            return value;
        }
    }
    return 0;
}




bool init_display(){
    bool success = true;

    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        success = false;
    }
    else
    {
        if(SDL_CreateWindowAndRenderer(640, 320, SDL_WINDOW_SHOWN, &window, &renderer) != 0)
        {
            printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
            success = false;
        }
    }
      return success;
}


void display(uint32_t displayArray[32][64])
    {
        
    SDL_Texture *texture = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGB888,
    SDL_TEXTUREACCESS_STREAMING,
    64,32
    );

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_UpdateTexture(texture,NULL,displayArray, 64 * sizeof(uint32_t));
    SDL_RenderCopy(renderer,texture,NULL,NULL);
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(texture);

}

int exec_opcode(uint16_t opcode, components* chip_8){
    
    uint16_t firstNibble = opcode >> 12;
    uint16_t X = (opcode & 0x0F00) >> 8;
    uint16_t Y = (opcode & 0x00F0) >> 4;
    uint16_t N = opcode & 0xF;
    uint16_t NN = opcode & 0xFF;
    uint16_t NNN = opcode & 0xFFF;
    int flag = 0;
    uint8_t value;
 
    switch(firstNibble){
        case 0x0:
            if(opcode == 0x00E0){
                memset(chip_8->displayArray,0,sizeof(chip_8->displayArray));
                flag = REFRESH_DISPLAY;
            }else if (opcode == 0x00EE){
                chip_8 -> PC = pop(chip_8 -> stack);
            }
                
            break;
        case 0x1:
            chip_8 -> PC = NNN;
            break;
        case 0x2:
            push(chip_8 -> PC, chip_8 -> stack);
            chip_8 -> PC = NNN;
            break;
        case 0x3:
            chip_8 -> PC += (chip_8 -> V[X] == NN) ? 2 : 0;
            break;
        case 0x4:
            chip_8 -> PC += (chip_8 -> V[X] != NN) ? 2 :0;
            break;
        case 0x5:
            chip_8 -> PC += (chip_8 -> V[X] == chip_8 -> V[Y]) ? 2 : 0;
            break;
        case 0x6:
            chip_8 -> V[X] = NN;
            break;
        case 0x7:
            chip_8 -> V[X] += NN;
            break;
        case 0x8:
            switch(N){
                case 0x0:
                    chip_8 -> V[X] = chip_8 -> V[Y];
                    break;
                case 0x1:
                    chip_8 -> V[X] = chip_8-> V[X] | chip_8 -> V[Y];
                    if(vF_reset_quirk){
                        chip_8 -> V[0xF] = 0;
                    }
                    break;
                case 0x2:
                    chip_8 -> V[X] = chip_8 -> V[X] & chip_8 -> V[Y];
                     if(vF_reset_quirk){
                        chip_8 -> V[0xF] = 0;
                    }
                    break;
                case 0x3:
                    chip_8 -> V[X] = chip_8 -> V[X] ^ chip_8 -> V[Y];
                     if(vF_reset_quirk){
                        chip_8 -> V[0xF] = 0;
                    }
                    break;
                case 0x4:
                    value = chip_8 -> V[X];
                    chip_8 -> V[X] += chip_8 -> V[Y];
                    chip_8 -> V[0xF] = (value <= chip_8 -> V[X]) ? 0 : 1;
                    break;
                case 0x5:
                    value = chip_8 -> V[X];
                    chip_8 -> V[X] -= chip_8 -> V[Y];
                    chip_8 -> V[0xF] = (value >= chip_8 -> V[Y]) ? 1 : 0;
                    break;
                case 0x6:
                    if(!shifting_quirk){
                        chip_8 -> V[X] = chip_8 -> V[Y];
                    }
                    value = chip_8 -> V[X] & 0x1;
                    chip_8 -> V[X] >>= 1;
                    chip_8 -> V[0xF] = (value == 1) ? 1 : 0; 
                    break;
                case 0x7:
                    value = chip_8 -> V[X];
                    chip_8 -> V[X] = chip_8 -> V[Y] - chip_8 -> V[X];
                    chip_8 -> V[0xF] = (chip_8 -> V[Y] >= value) ? 1 : 0;
                    break;
                case 0xE:
                    if(!shifting_quirk){
                       chip_8 -> V[X] = chip_8 -> V[Y];

                    }
                    value = (chip_8 -> V[X] & 0x80) >> 7;
                    chip_8 -> V[X] <<= 1;
                    chip_8 -> V[0xF] = (value == 1) ? 1 : 0; 
                    break;  
            }
            break;
        case 0x9:
            chip_8 -> PC += (chip_8 -> V[X] != chip_8 -> V[Y]) ? 2 : 0;
            break;
        case 0xA:
            chip_8 -> I = NNN;
            break;
        case 0xB:
            if(!jump_quirk){
            chip_8 -> PC = NNN + chip_8 -> V[0x0];
            }else{
                chip_8->PC = NNN + chip_8 -> V[X];
            }
            break;
        case 0xC:
            int random = rand() % 256;
            chip_8 -> V[X] = random & NN;
            break;
        case 0xD:
            
            int vx = (chip_8 -> V[X]) % 64;
            int vy = (chip_8 -> V[Y]) % 32;
            chip_8 -> V[0xF] = 0;
            for(int i = 0; i < N; i++){
                uint8_t sprite = chip_8 -> memory[chip_8 -> I +i];

                for(int j = 0; j < 8;j++){
                    if((sprite & (0x80 >> j))!= 0){
                        if(clipping_quirk){
                            if(vy + i < 32 && vx + j < 64){
                                if(chip_8 -> displayArray[vy + i][vx + j] == 0){
                                    chip_8 -> displayArray[vy+ i][vx + j] = 0xFFFFFFFF;
                                }else{
                                    chip_8 -> displayArray[vy+ i][vx+j] = 0;
                                    chip_8 -> V[0xF] = 1;
                                }
                            }
                        }else{
                            if(chip_8 -> displayArray[(vy + i) % 32][(vx + j) % 64] == 0){
                                    chip_8 -> displayArray[(vy+ i)%32][(vx + j)%64] = 0xFFFFFFFF;
                                }else{
                                    chip_8 -> displayArray[(vy+ i%32)][(vx+j)%64] = 0;
                                    chip_8 -> V[0xF] = 1;
                                }
                        }
                            
                      
                    }
                    
                }
            }
            flag = REFRESH_DISPLAY;
            break;
        case 0xE:
            if(NN == 0x9E){
                uint8_t key = chip_8 -> V[X];
                chip_8 -> PC = (chip_8 -> keypad[key] == 1) ? chip_8 -> PC + 2 : chip_8 -> PC;
            }else if(NN == 0xA1){
                uint8_t key = chip_8 -> V[X];
                chip_8 -> PC = (chip_8 -> keypad[key] != 1) ? chip_8 -> PC + 2 : chip_8 -> PC;
            }
            break;
        case 0xF:
            switch(NN){
                case 0x07:
                    chip_8 -> V[X] = chip_8 -> delay_timer;
                    break;
                case 0x15:  
                    chip_8 -> delay_timer = chip_8 -> V[X];
                    break;
                case 0x18:
                    chip_8 -> sound_timer = chip_8 -> V[X];
                    break;
                case 0x1E:   
                    chip_8 -> I += chip_8 -> V[X];
                    chip_8 -> V[0xF] = (chip_8 -> I >= 0x1000) ? 1 : 0;
                    break;
                case 0x0A:
                    flag = KEYPRESS_NOT_READY;
                    for(int i = 0; i < 16;i++){
                        if(chip_8 -> keypad[i] == 1){
                            flag = 0;
                            chip_8 -> V[X] = i;
                            break;
                        }
                    }
                    break;
                case 0x29:
                    chip_8 -> I = 0x50 + (chip_8 -> V[X] & 0xF) * 5;
                    break;
                case 0x33:
                    value = chip_8 -> V[X];
                    uint8_t firstDigit = 0;
                    uint8_t secondDigit = 0;
                    uint8_t thirdDigit = 0;

                    if(value == 0){

                    }else if (value < 10){
                       thirdDigit = value;
                    }else if (value < 100){
                        secondDigit = value / 10;
                        thirdDigit = value % 10;
                    }else{
                        firstDigit = value / 100;
                        secondDigit = (value / 10) % 10;
                        thirdDigit = value % 10;
                    }
                    chip_8 -> memory[chip_8 -> I] = firstDigit;
                    chip_8 -> memory[(chip_8 -> I) + 1] = secondDigit;
                    chip_8 -> memory[(chip_8 -> I) + 2] = thirdDigit;
                    break;
                case 0x55:
                    for(int i = 0 ; i <= X; i++){
                        
                        if(!memory_quirk){
                            chip_8 -> memory[chip_8 -> I + i] = chip_8 -> V[i];
                            
                        }else{
                            chip_8 -> memory[chip_8 -> I] = chip_8 -> V[i];
                            (chip_8 -> I)++;
                        }
                      
                       
                    }
                    break;
                case 0x65:
                    for(int i = 0 ; i <= X; i++){
                        if(!memory_quirk){
                            chip_8 -> V[i] = chip_8 -> memory[chip_8 -> I + i];
                        }else{
                            chip_8 -> V[i] = chip_8 -> memory[chip_8 -> I];
                            (chip_8 -> I)++;
                        }
                        
                    }
                    break;
                    break;
            }

            break;
            
        default:
            printf("Erreur opcode");

    }
    return flag;
 
}

float delta(struct timeval t1, struct timeval t2){
    float delta = (t2.tv_sec - t1.tv_sec) * 1000;
    delta += (t2.tv_usec - t1.tv_usec ) / 1000.0;
    return delta;  
}

double get_time_ms(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void init_chip8(components *chip_8){
    
    const uint8_t FONT_ARRAY[80] = {	

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


    memset(chip_8, 0, sizeof(*chip_8));   
    for (int i = 0; i < 80; i++){

        chip_8 -> memory[0x50 + i] = FONT_ARRAY[i]; 
    }
    chip_8 -> PC = 0x200;
    
}

int load_rom(components *chip_8, char **argv){
    FILE *rom = fopen(argv[1],"rb");
    if(rom == NULL){
        perror("Cant open/found rom");
        return EXIT_FAILURE;
    }

      if(fseek(rom,0L,SEEK_END) != 0){
        perror("erreur fseek");
        return EXIT_FAILURE;
      } 
      long size = ftell(rom);
     
      rewind(rom);
      if(fread(chip_8 -> memory + 512,1,size,rom) < 1){
        
            perror("erreur fread");
            ferror(rom);    
            return EXIT_FAILURE;
      }
      

      fclose(rom);

      return EXIT_SUCCESS;
}


void update_timers(components *chip_8){
    chip_8 -> delay_timer -= (chip_8 ->delay_timer > 0) ? 1 : 0;
    chip_8 ->sound_timer -= (chip_8 -> sound_timer > 0) ? 1 : 0;
}

int main(int argc, char** argv){

     double t1,t2;
     bool pause;
    SDL_Event event;
    uint16_t opcode;
    bool redraw;
    bool quit = false;
    components chip_8;
    srand(time(NULL));
 
    const SDL_Scancode KEYCODE_ARRAY[16] = {
        SDL_SCANCODE_X,SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
        SDL_SCANCODE_Q,SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,
        SDL_SCANCODE_S,SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C,
        SDL_SCANCODE_4,SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V,
         
    };

    init_display();
    init_chip8(&chip_8);
    if(argc != 2){
        printf("Usage : ./main rom_file");
        return EXIT_FAILURE;
    }
    load_rom(&chip_8, argv);

    while(!quit){
        t1 = get_time_ms();   

        while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            exit(EXIT_SUCCESS); 
        }else if (event.type == SDL_KEYDOWN){
            for(int i = 0; i < 16;i++){
                if(KEYCODE_ARRAY[i] == event.key.keysym.scancode){
                    chip_8.keypad[i] = 1;
                }else if(event.key.keysym.scancode == SDL_SCANCODE_ESCAPE){
                    quit = true;
                }
            }
            if (event.key.repeat == 0){
                switch (event.key.keysym.scancode)
                {
                case SDL_SCANCODE_5:
                      display_wait_quirk = !display_wait_quirk;
                      if(display_wait_quirk){
                        printf("Display Wait Enabled\n");
                      }else{
                        printf("Display Wait Disabled\n");
                      }
                    break;
                case SDL_SCANCODE_6:
                    vF_reset_quirk = !vF_reset_quirk;
                    if(vF_reset_quirk){
                        printf("vF Reset Enabled\n");
                    }else{
                        printf("vF Reset Disabled\n");
                    }
                    break;
                case SDL_SCANCODE_T:
                    memory_quirk = !memory_quirk;
                    if(memory_quirk){
                        printf("Memory Quirk Enabled\n");
                    }else{
                        printf("Memory Quirk Disabled\n");
                    }
                    break;
                case SDL_SCANCODE_Y:
                    clipping_quirk = !clipping_quirk;
                    if(clipping_quirk){
                        printf("Clipping Enabled\n");
                    }else{
                        printf("Clipping Disabled\n");
                    }
                    break;
                case SDL_SCANCODE_G:
                    shifting_quirk = !shifting_quirk;
                    if(shifting_quirk){
                        printf("Shifting Enabled\n");
                    }else{
                        printf("Shifting Disabled\n");
                    }
                    break;
                case SDL_SCANCODE_H:
                    jump_quirk = !jump_quirk;
                    if(jump_quirk){
                        printf("Jump Quirk Enabled\n");
                    }else{
                        printf("Jump Quirk Disabled\n");
                    }
                    break;
                case SDL_SCANCODE_SPACE:
                    pause = !pause;
                    if(pause){
                        printf("The game is paused\n");
                    }else{
                        printf("The game is running\n");
                    }
                default:
                    break;
                }
                  
            }

        }else if(event.type == SDL_KEYUP){
            for(int i = 0; i< 16;i++){
                if(KEYCODE_ARRAY[i] == event.key.keysym.scancode){
                    chip_8.keypad[i] = 0;
                }
                }
            }
        }


      
        if(!pause){

            redraw = false;

            for(int i = 0; i < INSTRUCTION_RATES / (int)FPS; i++){
            opcode = chip_8.memory[chip_8.PC] << 8 | chip_8.memory[chip_8.PC + 1];
            chip_8.PC += 2;
             int flag = exec_opcode(opcode, &chip_8);
            if(flag == REFRESH_DISPLAY){
                redraw = true;      
                if(display_wait_quirk){
                    break;
                }
            }if(flag == KEYPRESS_NOT_READY){
                chip_8.PC -= 2;
            }
        }
        if(redraw){
            display(chip_8.displayArray);

        }

        update_timers(&chip_8);
        }
        


        double targetTime = t1 + (1000.0 / FPS);
        t2 = get_time_ms();
        
        double timeToWait = (targetTime - t2 - 1.0);
        if (timeToWait > 0) {
            SDL_Delay(((Uint32) timeToWait)); 
        }

        while(get_time_ms() < targetTime){

        }
        
    }
    return 0; 
}

