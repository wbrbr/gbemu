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

uint16_t bp = -1;

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
    ImGui::InputScalar("Breakpoint", ImGuiDataType_U16, &bp, NULL, NULL, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
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


    uint32_t pixels[160*144];
    memset(pixels, 0, sizeof(pixels));
    glEnable(GL_TEXTURE_2D);
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 160, 144, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SDL_Event e;

    Cpu cpu;
    cpu.load(argv[1]);
    
    Ppu ppu;
    cpu.ppu = &ppu;

    ExecMode mode = MODE_STEP;

    bool go_step = false;
    bool running = true;

    const int CYCLES_PER_FRAME = 69905; // 4194304 / 60;

    while (running) {

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            if (mode == MODE_STEP && e.type == SDL_KEYDOWN) {
                switch(e.key.keysym.scancode) {
                    case SDL_SCANCODE_N:
                        go_step = true;
                        break;

                    case SDL_SCANCODE_C:
                        mode = MODE_RUNBREAK;

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
            }
        } else {
            for (int i = 0; i < CYCLES_PER_FRAME;)
            {
                SideEffects eff = cpu.cycle();
                ppu.exec(eff.cycles);
                i += eff.cycles;
                if (mode == MODE_RUNBREAK && cpu.pc == bp) {
                    mode = MODE_STEP;
                    go_step = false;
                    break;
                }
            }
        }

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        if (mode == MODE_STEP) {
            drawRegsWindow(cpu, ppu);
            drawInstrWindow(cpu);
            // ImGui::ShowDemoWindow();
        }


        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

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
