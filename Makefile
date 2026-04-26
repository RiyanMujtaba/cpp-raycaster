CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall $(shell sdl2-config --cflags) -I/usr/local/include/SDL2
LIBS     = $(shell sdl2-config --libs) -lSDL2_mixer -lSDL2_ttf

raycaster: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o raycaster $(LIBS)

clean:
	rm -f raycaster
