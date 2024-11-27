#include <stdio.h>

#define MID_DEBUG
[[noreturn]] void Panic(const char* file, int line, const char* message)
{
	fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
	__builtin_trap();
}

#define MID_OPENXR_IMPLEMENTATION
#include "mid_openxr_runtime.h"
#undef MID_OPENXR_IMPLEMENTATION

#define MID_VULKAN_IMPLEMENTATION
#include "mid_vulkan.h"
#undef MID_VULKAN_IMPLEMENTATION

#define MID_MATH_IMPLEMENTATION
#include "mid_math.h"
#undef MID_MATH_IMPLEMENTATION

#define MID_SHAPE_IMPLEMENTATION
#include "mid_shape.h"
#undef MID_SHAPE_IMPLEMENTATION

#define MID_WINDOW_IMPLEMENTATION
#include "mid_window.h"
#undef MID_WINDOW_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION