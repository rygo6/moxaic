cmake_minimum_required(VERSION 3.26)
project(moxaic)

set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME moxaic)

file(GLOB SRC_FILES
        src/*.cpp
        src/*.hpp
        src/*.c
        src/*.h
        third_party/*.cpp
        third_party/*.hpp
        third_party/*.c
        third_party/*.h
        src/descriptors/*.cpp
        src/descriptors/*.hpp
        src/descriptors/*.c
        src/descriptors/*.h
        src/pipelines/*.cpp
        src/pipelines/*.hpp
        src/pipelines/*.c
        src/pipelines/*.h
        src/utilities/*.cpp
        src/utilities/*.hpp
        src/utilities/*.c
        src/utilities/*.h
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
        src
        src/descriptors
        src/pipelines
        src/utilities
        third_party
        "${VULKAN_SDK_PATH}/Include"
)

target_link_libraries(${TARGET_NAME}
        SDL2main
        SDL2
        vulkan-1
)

#target_compile_options(${TARGET_NAME} PUBLIC -fno-exceptions)

set(SHADER_COMPILER "${VULKAN_SDK_PATH}/Bin/glslc.exe")
file(GLOB SHADER_SOURCE_FILES
        shaders/*.vert
        shaders/*.frag
)
foreach(SHADER IN LISTS SHADER_SOURCE_FILES)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${SHADER_COMPILER} ${SHADER} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling ${SHADER_NAME}"
    )
    list(APPEND SHADER_OUTPUT_FILES ${SHADER_OUTPUT})
endforeach()
add_custom_target(CompileShaders ALL DEPENDS ${SHADER_OUTPUT_FILES})
add_dependencies(${TARGET_NAME} CompileShaders)