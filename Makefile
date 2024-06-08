# Compiler
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra
LDFLAGS =
TARGET_NAME = moxaic

# Paths
VULKAN_SDK_PATH = C:/VulkanSDK/1.3.280.0

# Source files
SRC_DIR = src
SRC_FILES = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*.h) \
            $(wildcard third_party/*.c) $(wildcard third_party/*.h)

# Targets
all: $(SRC_FILES) CompileShaders
	$(CC) $(CFLAGS) -o $@ $(SRC_FILES) $(LDFLAGS)

CompileShaders: $(SHADER_OUTPUT_FILES)
	@echo "Shaders compiled"

$(CMAKE_BINARY_DIR)/shaders/%.o: shaders/% $(SHADER_INCLUDE_FILES)
	$(SHADER_COMPILER) $< --target-spv=spv1.4 -o $@
	$(SHADER_OPTIMIZER) -O $@ -o $(patsubst %.o,%.spv,$@)

.PHONY: clean

clean:
	rm -f $(TARGET_NAME) $(SHADER_OUTPUT_FILES)
