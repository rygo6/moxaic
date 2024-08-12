cmake_minimum_required(VERSION 3.28)
project(moxaic C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET_NAME moxaic)

#set(THREADS_PREFER_PTHREAD_FLAG ON)
#set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(VULKAN_SDK_PATH "C:/VulkanSDK/1.3.290.0")

file(GLOB SRC_FILES
        src/*.c
        src/*.h
        third_party/*.c
        third_party/*.h
)

add_executable(${TARGET_NAME}
        ${SRC_FILES}
)

target_link_directories(${TARGET_NAME} PUBLIC
        "${VULKAN_SDK_PATH}/Lib"
)

target_include_directories(${TARGET_NAME} PUBLIC
        src
        third_party
        "${VULKAN_SDK_PATH}/Include"
)

target_link_libraries(${TARGET_NAME}
        vulkan-1
        ws2_32
)

target_compile_definitions(${TARGET_NAME} PRIVATE
    MOXAIC_COMPOSITOR
)

target_compile_options(${TARGET_NAME} PRIVATE
        "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/="
        "-fopt-info-vec"
)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    if (MSVC)
        message("ClangCL")
        target_compile_options(${TARGET_NAME} PRIVATE -Wl,-pdb:out.pdb)
    else ()
        message("Clang")
    endif ()
endif ()

set(SHADER_COMPILER "${VULKAN_SDK_PATH}/Bin/glslc.exe")
set(SHADER_OPTIMIZER "${VULKAN_SDK_PATH}/Bin/spirv-opt.exe")
file(GLOB SHADER_SOURCE_FILES
        shaders/*.vert
        shaders/*.frag
        shaders/*.mesh
        shaders/*.task
        shaders/*.comp
        shaders/*.tese
        shaders/*.tesc
)
file(GLOB SHADER_INCLUDE_FILES
        shaders/*.glsl
)
foreach (SHADER IN LISTS SHADER_SOURCE_FILES)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.o")
    set(SHADER_OUTPUT_OPT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMENT "Compiling ${SHADER_NAME}"
            COMMAND ${SHADER_COMPILER} ${SHADER} --target-spv=spv1.4 -o ${SHADER_OUTPUT}
            COMMENT "Optimizing ${SHADER_NAME}"
            COMMAND ${SHADER_OPTIMIZER} -O ${SHADER_OUTPUT} -o ${SHADER_OUTPUT_OPT}
            DEPENDS ${SHADER} ${SHADER_INCLUDE_FILES}
    )
    list(APPEND SHADER_OUTPUT_FILES ${SHADER_OUTPUT})
endforeach ()
add_custom_target(CompileShaders ALL DEPENDS ${SHADER_OUTPUT_FILES})
add_dependencies(${TARGET_NAME} CompileShaders)