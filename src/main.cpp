#include "glad/glad.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "cpu.hpp"
#include "ppu.hpp"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl2.h"

enum ExecMode {
    MODE_RUN,
    MODE_STEP,
    MODE_RUNBREAK,
};

uint16_t bp = 0x2ed;
uint16_t mem_beg = 0;
uint16_t mem_end = 0;
uint32_t tiles_tex;
uint32_t bgmap_tex;

void drawRegsWindow(Cpu& cpu, Ppu& ppu)
{
    ImGui::Begin("Registers");

    ImGui::Columns(2);
    char buf[20];
    sprintf(buf, "AF = %04x", cpu.af());
    ImGui::Text(buf);
    sprintf(buf, "BC = %04x", cpu.bc());
    ImGui::Text(buf);
    sprintf(buf, "DE = %04x", cpu.de());
    ImGui::Text(buf);
    sprintf(buf, "HL = %04x", cpu.hl());
    ImGui::Text(buf);

    sprintf(buf, "PC = %04x", cpu.pc);
    ImGui::Text(buf);
    sprintf(buf, "SP = %04x", cpu.sp);
    ImGui::Text(buf);
    sprintf(buf, "Z = %d", cpu.z);
    ImGui::Text(buf);
    sprintf(buf, "N = %d", cpu.n);
    ImGui::Text(buf);
    sprintf(buf, "H = %d", cpu.h);
    ImGui::Text(buf);
    sprintf(buf, "C = %d", cpu.c);
    ImGui::Text(buf);

    ImGui::NextColumn();
    sprintf(buf, "LCDC = %02x", ppu.lcdc);
    ImGui::Text(buf);
    sprintf(buf, "STAT = %02x", ppu.stat);
    ImGui::Text(buf);
    sprintf(buf, "LY = %02x", ppu.ly); 
    ImGui::Text(buf);
    sprintf(buf, "MODE = %d", ppu.stat & 3);
    ImGui::Text(buf);
    ImGui::End();
}

void drawInstrWindow(Cpu& cpu)
{
    ImGui::Begin("Instructions");
    char buf[20];
    for (uint16_t i = cpu.pc - 2; i < cpu.pc + 10; i++)
    {
        char b;
        if (i == cpu.pc ) b = '>';
        else if (i == bp) b = '*';
        else b = ' ';
        sprintf(buf, "%c %04x %02x", b, i, cpu.mem(i));
        ImGui::Text(buf);
    }
    ImGui::Separator();
    ImGui::InputScalar("Breakpoint", ImGuiDataType_U16, &bp, NULL, NULL, "%04x", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::End();
}

void drawMemWindow(Cpu& cpu)
{
    ImGui::Begin("Memory");
    ImGui::InputScalar("##beg", ImGuiDataType_U16, &mem_beg, NULL, NULL, "%04x", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::Text("-");
    ImGui::SameLine();
    ImGui::InputScalar("##end", ImGuiDataType_U16, &mem_end, NULL, NULL, "%04x", ImGuiInputTextFlags_CharsHexadecimal);
    char buf[100];
    for (uint16_t i = mem_beg >> 4; i <= mem_end >> 4; i++)
    {
        sprintf(buf, "%04x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                i * 16, cpu.mem(i*16, true), cpu.mem(i*16+1, true), cpu.mem(i*16+2, true), cpu.mem(i*16+3, true), cpu.mem(i*16+4, true), cpu.mem(i*16+5, true),
                cpu.mem(i*16+6, true), cpu.mem(i*16+7, true), cpu.mem(i*16+8, true), cpu.mem(i*16+9, true), cpu.mem(i*16+10, true), cpu.mem(i*16+11, true),
                cpu.mem(i*16+12, true), cpu.mem(i*16+13, true), cpu.mem(i*16+14, true), cpu.mem(i*16+15, true));
        ImGui::Text(buf);
    }
    ImGui::End();
}

void drawTilesWindow(Ppu& ppu)
{
    ImGui::Begin("Tiles");
    uint32_t pixels[8*16*8*24];

    uint8_t* p = ppu.vram;
    // 16 x 24
    for (int i = 0; i < 384; i++)
    {

        int xi = i % 16;
        int yi = i / 16;

        for (int y = 0; y < 8; y++)
        {
            uint8_t b1 = *p;
            p++;
            uint8_t b2 = *p;
            p++;
            for (int x = 0; x < 8; x++)
            {
                uint8_t lsb = (b1 >> (7 - x)) & 1;
                uint8_t msb = (b2 >> (7 - x)) & 1;
                uint8_t pal = lsb | (msb << 1);

                // uint32_t values[] = { 0x081820ff, 0x346856ff, 0x88c070ff, 0xe0f8d0ff };
                uint32_t values[] = { 0x000000ff, 0x346856ff, 0x88c070ff, 0xe0f8d0ff };
                pixels[8*16*(8*yi+y)+(8*xi+x)] = values[pal];
                // pixels[8*16*(8*yi+y)+(8*xi+x)] = 0;
                // pixels[8*16*y+x] = 0;

            }
        }
    }

    ImVec2 size;
    size.x = 16*8;
    size.y = 24*8;

    glBindTexture(GL_TEXTURE_2D, tiles_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16*8, 24*8, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    ImGui::Image((void*)(intptr_t)tiles_tex, size);
    ImGui::End();
    // TODO: don't stream the texture each frame
}

void drawBGMapWindow(Ppu& ppu)
{
    ImGui::Begin("BG Map");
    uint32_t pixels[256*256];
    uint32_t values[] = { 0xff000000, 0x346856ff, 0x88c070ff, 0xe0f8d0ff };

    // TODO: addressing modes
    
    for (int yi = 0; yi < 32; yi++)
    {
        for (int xi = 0; xi < 32; xi++)
        {
            int tile_index = ppu.vram[0x1800+yi*32+xi];
            for (int y = 0; y < 8; y++)
            {
                uint8_t b1 = ppu.vram[tile_index * 16 + 2*y];
                uint8_t b2 = ppu.vram[tile_index * 16 + 2*y + 1];
                for (int x = 0; x < 8; x++)
                {
                    uint8_t lsb = (b1 >> (7 - x)) & 1;
                    uint8_t msb = (b2 >> (7 - x)) & 1;
                    uint8_t pal = lsb | (msb << 1);

                    pixels[(yi*8+y)*256+xi*8+x] = values[pal];
                }
            }
        }    
    }

    ImVec2 size;
    size.x = 256;
    size.y = 256;

    glBindTexture(GL_TEXTURE_2D, bgmap_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    ImGui::Image((void*)(intptr_t)bgmap_tex, size);
    ImGui::End();    
}

void drawOAMWindow(Ppu& ppu)
{
    ImGui::Begin("OAM");
    char buf[100];
    for (uint8_t i = 0; i < 40; i++)
    {
        uint8_t y = ppu.oam[i*4];
        uint8_t x = ppu.oam[i*4+1];
        uint8_t tile = ppu.oam[i*4+2];
        uint8_t flags = ppu.oam[i*4+3];
        if (x == 0 && y == 0 && tile == 0 && flags == 0) continue;
        snprintf(buf, 100, "X = %02x\nY = %02x\nTile = %02x\nFlags = %02x\n", x, y, tile, flags);
        ImGui::Text(buf);
        ImGui::Separator();
    }
    ImGui::End();
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <rom file>\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s", SDL_GetError());
        return 1;
    }

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("gbemu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 576, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    gladLoadGL();

	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();	


    glEnable(GL_TEXTURE_2D);
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 160, 144, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &tiles_tex);
    glBindTexture(GL_TEXTURE_2D, tiles_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8*16, 8*24, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &bgmap_tex);
    glBindTexture(GL_TEXTURE_2D, bgmap_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SDL_Event e;

    Cpu cpu;
    cpu.load(argv[1]);
    
    Ppu ppu;

    cpu.ppu = &ppu;
    ppu.cpu = &cpu;

    ExecMode mode = MODE_STEP;

    bool go_step = false;
    bool go_step_back = false;
    bool running = true;

    const int CYCLES_PER_FRAME = 69905; // 4194304 / 60;
    uint64_t instr_num = 0;

    while (running) {

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            if (mode == MODE_STEP && e.type == SDL_KEYDOWN) {
                switch(e.key.keysym.scancode) {
                    case SDL_SCANCODE_F6:
                        go_step_back = true;
                        break;

                    case SDL_SCANCODE_F7:
                        go_step = true;
                        break;

                    case SDL_SCANCODE_F9:
                        mode = MODE_RUNBREAK;
                        break;

                    case SDL_SCANCODE_F10:
                        mode = MODE_RUN;
                        break;

                    default:
                        break;
                }
            }
            if (mode == MODE_RUN || mode == MODE_RUNBREAK) {
                switch(e.key.keysym.scancode) {
                    case SDL_SCANCODE_F8:
                        mode = MODE_STEP;
                        go_step = false;
                        break;

                    default:
                        break;
                }
            }
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (mode == MODE_STEP) {
            if (go_step) {
                SideEffects eff = cpu.cycle();
                ppu.exec(eff.cycles);
                go_step = false;
                instr_num++;
            } else if (go_step_back) {
                uint64_t target = instr_num;
                cpu.reset();
                ppu.reset();
                instr_num = 0;
                while (instr_num + 1 != target)
                {
                    SideEffects eff = cpu.cycle();
                    ppu.exec(eff.cycles);
                    instr_num++;
                }
                go_step_back = false;
            }
        } else {
            for (int i = 0; i < CYCLES_PER_FRAME;)
            {
                SideEffects eff = cpu.cycle();
                ppu.exec(eff.cycles);
                instr_num++;
                if (instr_num == 0) puts("overflow");
                i += eff.cycles;
                if (mode == MODE_RUNBREAK && cpu.pc == bp) {
                    mode = MODE_STEP;
                    go_step = false;
                    break;
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 160, 144, GL_RGBA, GL_UNSIGNED_BYTE, ppu.framebuf);

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        if (mode == MODE_STEP) {
            drawRegsWindow(cpu, ppu);
            drawInstrWindow(cpu);
            drawMemWindow(cpu);
            drawTilesWindow(ppu);
            drawBGMapWindow(ppu);
            drawOAMWindow(ppu);
            // ImGui::ShowDemoWindow();
        }


        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.f, 1.f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(1.f, 1.f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(1.f, -1.f);
        glTexCoord2f(.0f, 1.0f);
        glVertex2f(-1.f, -1.f);
        glEnd();

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
