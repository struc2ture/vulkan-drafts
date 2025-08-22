CFLAGS = -g -I/opt/homebrew/include -I/usr/local/include
LFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lglfw -lvulkan

export VK_ICD_FILENAMES = /usr/local/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH = /usr/local/share/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH = /usr/local/lib:$DYLD_LIBRARY_PATH

main: bin/main

debug: bin/main
	lldb bin/main

bin/main: src/main.cpp
	clang++ $(CFLAGS) $(LFLAGS) src/main.cpp -o bin/main
