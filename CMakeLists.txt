cmake_minimum_required(VERSION 3.28)
project(moxaic C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(TARGET_NAME moxaic)

file(GLOB SRC_FILES
        src/*.c
        src/*.h
        third_party/*.c
        third_party/*.h
)

set(SANITIZER_FLAGS -fsanitize=address -fsanitize=undefined -fsanitize=leak)
set(SECURITY_FLAGS -fstack-protector-all -D_FORTIFY_SOURCE=2 -fPIE -pie)
set(DEBUG_FLAGS -fno-omit-frame-pointer)
set(WARNING_FLAGS -Wall -Wextra -Werror -Wformat -Wformat-security)
set(DISABLE_WARNINGS
        -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
        -Wno-error=unused-value -Wno-error=unused-function -Wno-error=unused-variable
        -Wno-error=unused-parameter -Wno-error=unused-const-variable -Wno-error=pedantic)

if(WIN32)

    set(VULKAN_SDK_PATH "C:/VulkanSDK/1.4.313.1")
    set(OPENXR_SDK_INCLUDE_PATH "C:/OpenXRSDK/openxr_loader_windows-1.1.42/include")
    set(OPENXR_SDK_LINK_PATH "C:/OpenXRSDK/openxr_loader_windows-1.1.42/x64/bin")

    add_executable(${TARGET_NAME} ${SRC_FILES})

    target_link_directories(${TARGET_NAME} PUBLIC
            "${VULKAN_SDK_PATH}/Lib"
            "${OPENXR_SDK_LINK_PATH}"
    )
    target_include_directories(${TARGET_NAME} PUBLIC
            src
            third_party
            "${VULKAN_SDK_PATH}/Include"
            "${OPENXR_SDK_INCLUDE_PATH}"
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        set_target_properties(${TARGET_NAME} PROPERTIES
                LINK_FLAGS "-static-libgcc -Wl,-Bstatic,--whole-archive -lpthread -Wl,--no-whole-archive,-Bdynamic"
        )
        set(PTHREAD_STATIC_PATH "C:/msys64/ucrt64/lib/libwinpthread.a")
        target_link_libraries(${TARGET_NAME} PUBLIC "${PTHREAD_STATIC_PATH}")
        target_compile_options(${TARGET_NAME} PRIVATE -fopt-info-vec)
        if(EXISTS "${PTHREAD_STATIC_PATH}")
            message(STATUS "Found pthread at: ${PTHREAD_STATIC_PATH}")
        else()
            message(WARNING "pthread not found at ${PTHREAD_STATIC_PATH}")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set_target_properties(${TARGET_NAME} PROPERTIES
                LINK_FLAGS "-Wl,-Bstatic,--whole-archive -lpthread -Wl,--no-whole-archive,-Bdynamic"
        )
        set(PTHREAD_STATIC_PATH "C:/msys64/clang64/lib/libwinpthread.a")
        target_link_libraries(${TARGET_NAME} PUBLIC "${PTHREAD_STATIC_PATH}")
        if(EXISTS "${PTHREAD_STATIC_PATH}")
            message(STATUS "Found pthread at: ${PTHREAD_STATIC_PATH}")
        else()
            message(WARNING "pthread not found at ${PTHREAD_STATIC_PATH}")
        endif()
    endif()

    target_link_libraries(${TARGET_NAME} PUBLIC
            vulkan-1 opengl32 openxr_loader ws2_32 dxgi d3d11 d3d12 synchronization
    )

    set(SHADER_COMPILER "${VULKAN_SDK_PATH}/Bin/glslc.exe")
    set(SHADER_OPTIMIZER "${VULKAN_SDK_PATH}/Bin/spirv-opt.exe")

else() # Linux

    find_package(PkgConfig REQUIRED)
    find_package(Vulkan REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

    # Generate Wayland protocol files
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    set(WAYLAND_PROTOCOLS_DIR /usr/share/wayland-protocols)

    set(GEN_DIR ${CMAKE_BINARY_DIR}/gen)
    file(MAKE_DIRECTORY ${GEN_DIR})
    add_custom_target(GenerateWaylandProtocols)

    pkg_check_modules(LIBDECOR REQUIRED libdecor-0)

    list(APPEND SRC_FILES)
    add_executable(${TARGET_NAME} ${SRC_FILES})
    add_dependencies(${TARGET_NAME} GenerateWaylandProtocols)

    # Find OpenXR
    find_library(OPENXR_LOADER openxr_loader)
    if(NOT OPENXR_LOADER)
        pkg_check_modules(OPENXR openxr)
        set(OPENXR_LOADER ${OPENXR_LIBRARIES})
    endif()

    # Find shader compiler
    find_program(GLSLC_EXECUTABLE glslc REQUIRED)
    set(SHADER_COMPILER ${GLSLC_EXECUTABLE})
    find_program(SPIRV_OPT_EXECUTABLE spirv-opt REQUIRED)
    set(SHADER_OPTIMIZER ${SPIRV_OPT_EXECUTABLE})

    set(OPENXR_SDK_PATH "/home/ryan/Developer/OpenXR-SDK")
    set(VULKAN_SDK_PATH "/home/ryan/Developer/vulkansdk-linux/1.4.341.1/x86_64")

    target_include_directories(${TARGET_NAME} PUBLIC
            src
            third_party
            ${GEN_DIR}
            ${VULKAN_SDK_PATH}/include
            ${WAYLAND_CLIENT_INCLUDE_DIRS}
            ${LIBDECOR_INCLUDE_DIRS}
            ${OPENXR_SDK_PATH}/include
    )

    target_link_libraries(${TARGET_NAME} PUBLIC
            ${Vulkan_LIBRARIES}
            ${WAYLAND_CLIENT_LIBRARIES}
            ${LIBDECOR_LIBRARIES}
            ${OPENXR_LOADER}
            pthread
            rt
            GL
            m
    )

endif()

# Common compile settings
target_compile_definitions(${TARGET_NAME} PUBLIC MOXAIC_COMPOSITOR)
target_compile_options(${TARGET_NAME} PRIVATE
        -std=gnu11
        -g3
        -O0
        ${DEBUG_FLAGS}
        ${WARNING_FLAGS}
        ${DISABLE_WARNINGS}
        -fvisibility=hidden
        -include globals.h
        -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=
        -Wno-gnu-statement-expression
        -fno-strict-aliasing
        -fwrapv
)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG)
endif()

message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "Shader compiler: ${SHADER_COMPILER}")

# Shader compilation
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
file(GLOB SHADER_SOURCE_FILES
        shaders/*.vert shaders/*.frag shaders/*.mesh shaders/*.task
        shaders/*.comp shaders/*.tese shaders/*.tesc
)
file(GLOB SHADER_INCLUDE_FILES shaders/*.glsl)

foreach(SHADER IN LISTS SHADER_SOURCE_FILES)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT     "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.o")
    set(SHADER_OUTPUT_OPT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMENT "Compiling ${SHADER_NAME}"
            COMMAND ${SHADER_COMPILER} ${SHADER} --target-spv=spv1.4 -o ${SHADER_OUTPUT}
            COMMAND ${SHADER_OPTIMIZER} -O ${SHADER_OUTPUT} -o ${SHADER_OUTPUT_OPT}
            DEPENDS ${SHADER} ${SHADER_INCLUDE_FILES}
    )
    list(APPEND SHADER_OUTPUT_FILES ${SHADER_OUTPUT})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SHADER_OUTPUT_FILES})
add_dependencies(${TARGET_NAME} CompileShaders)

# Copy textures
add_custom_target(CopyTextures ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/textures ${CMAKE_BINARY_DIR}/textures
)
add_dependencies(${TARGET_NAME} CopyTextures)
