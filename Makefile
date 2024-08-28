CC = gcc
CFLAGS = -std=c99 -fmacro-prefix-map=$(CURDIR)/= -fopt-info-vec -include globals.h
LDFLAGS = -L"C:/VulkanSDK/1.3.290.0/Lib" -lvulkan-1 -lws2_32 -v
INCLUDES = -Isrc -Ithird_party -IC:/VulkanSDK/1.3.290.0/Include
DEFINES = -DMOXAIC_COMPOSITOR
TARGET = moxaic

SRC_DIRS = src third_party
SRC_FILES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJ_FILES = $(SRC_FILES:.c=.o)

SHADER_COMPILER = C:/VulkanSDK/1.3.290.0/Bin/glslc.exe
SHADER_OPTIMIZER = C:/VulkanSDK/1.3.290.0/Bin/spirv-opt.exe
SHADER_DIR = shaders
SHADER_FILES = $(wildcard $(SHADER_DIR)/*.*)
SHADER_OUTPUT_DIR = build/shaders
SHADER_OUTPUT_FILES = $(patsubst $(SHADER_DIR)/%,$(SHADER_OUTPUT_DIR)/%.o,$(SHADER_FILES))

default: $(TARGET)

moxaic: $(OBJ_FILES)
	$(CC) $(OBJ_FILES) $(LDFLAGS) -o $@

