CFLAGS = -g -I/opt/homebrew/include
LFLAGS = -L/opt/homebrew/lib -lglfw

main: bin/main

debug: bin/main
	lldb bin/main

bin/main: src/main.cpp
	clang++ $(CFLAGS) $(LFLAGS) src/main.cpp -o bin/main
