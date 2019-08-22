CFLAGS = -Wall -Wextra -Iinclude/ -Iexternal/include -g
LDFLAGS = -lSDL2 -g -ldl -lGL

all: main.o imgui.o imgui_demo.o imgui_draw.o imgui_widgets.o imgui_impl_sdl.o imgui_impl_opengl2.o glad.o cpu.o ppu.o
	g++ $^ -o gbemu $(LDFLAGS)

%.o: src/%.cpp
	g++ -c $< -o $@ $(CFLAGS)

%.o: external/src/%.cpp
	g++ -c $< -o $@ $(CFLAGS)
