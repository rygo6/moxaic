cmake_minimum_required(VERSION 3.26)
project(moxaic)

set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME moxaic)

file(GLOB SRC_FILES
        src/*.cpp
        src/*.hpp
        src/*.c
        src/*.h
        src/descriptors/*.cpp
        src/descriptors/*.hpp
        src/descriptors/*.c
        src/descriptors/*.h
)

add_executable(${TARGET_NAME}
        ${SRC_FILES}
)

set(VULKAN_SDK_PATH "C:/VulkanSDK/1.3.261.1")

target_link_directories(${TARGET_NAME} PUBLIC
        lib
        "${VULKAN_SDK_PATH}/Lib"
)

target_include_directories(${TARGET_NAME} PUBLIC
        include
        "${VULKAN_SDK_PATH}/Include"
)

target_link_libraries(${TARGET_NAME}
        SDL2main
        SDL2
        vulkan-1
)

#target_compile_options(${TARGET_NAME} PUBLIC -fno-exceptions)