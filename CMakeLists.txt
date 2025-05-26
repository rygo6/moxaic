cmake_minimum_required(VERSION 3.28)
project(moxaic C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(TARGET_NAME moxaic)

file(GLOB SRC_FILES
        src/*.c
        src/*.h
        third_party/*.c
        third_party/*.h
)

add_executable(${TARGET_NAME}
        ${SRC_FILES}
)


set(VULKAN_SDK_PATH "C:/VulkanSDK/1.3.290.0")
set(OPENXR_SDK_INCLUDE_PATH "C:/OpenXRSDK/openxr_loader_windows-1.1.42/include")
set(OPENXR_SDK_LINK_PATH "C:/OpenXRSDK/openxr_loader_windows-1.1.42/x64/bin")

set(SANITIZER_FLAGS -fsanitize=address -fsanitize=undefined -fsanitize=leak)
set(DEBUG_FLAGS -fno-omit-frame-pointer)
set(WARNING_FLAGS -Wall -Wextra -Werror -pedantic -Wformat -Wformat-security)
set(SECURITY_FLAGS -fstack-protector-all -D_FORTIFY_SOURCE=2 -fPIE -pie)
set(DISABLE_WARNINGS -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-parameter -Wno-error=unused-const-variable -Wno-error=pedantic)

# make switch to not compile oxr into main comp app

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

# Force static linking of pthread... this might be reason not to use pthread?
set_target_properties(${TARGET_NAME} PROPERTIES
        # Force static linking of runtime libraries
        LINK_FLAGS "-Wl,-Bstatic,--whole-archive -lpthread -Wl,--no-whole-archive,-Bdynamic"
)
set(PTHREAD_STATIC_PATH "C:/msys64/ucrt64/lib/libwinpthread.a")
if(EXISTS "${PTHREAD_STATIC_PATH}")
    message(STATUS "Found UCRT64 pthread at: ${PTHREAD_STATIC_PATH}")
else()
    message(WARNING "UCRT64 pthread not found at ${PTHREAD_STATIC_PATH}")
endif()


target_link_libraries(${TARGET_NAME} PUBLIC
        vulkan-1
        opengl32
        openxr_loader
        ws2_32
        dxgi
        d3d11
        d3d12
        synchronization
        "${PTHREAD_STATIC_PATH}"
)
target_link_options(${TARGET_NAME} PRIVATE
        #no santizers on GCC windows
#        -fsanitize=address
#        -fsanitize=undefined
#        -fsanitize=leak
#        -fstack-protector-all
#        -fPIE
#        -pie
)


# COMPILE
target_compile_definitions(${TARGET_NAME} PUBLIC
        MOXAIC_COMPOSITOR
)
target_compile_options(${TARGET_NAME} PRIVATE
#        ${SANITIZER_FLAGS}
        ${DEBUG_FLAGS}
        ${WARNING_FLAGS}
#        ${SECURITY_FLAGS}
        ${DISABLE_WARNINGS}

        -include globals.h
        -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/= # make __FILE__ path relative to project
#        -fopt-info-vec # vector optimization
        -std=c23
        -std=gnu23
        -Wno-gnu-statement-expression

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
            COMMENT "Compiling Optimizing ${SHADER_NAME}"
            COMMAND ${SHADER_COMPILER} ${SHADER} --target-spv=spv1.4 -o ${SHADER_OUTPUT}
            COMMAND ${SHADER_OPTIMIZER} -O ${SHADER_OUTPUT} -o ${SHADER_OUTPUT_OPT}
            DEPENDS ${SHADER} ${SHADER_INCLUDE_FILES}
    )
    list(APPEND SHADER_OUTPUT_FILES ${SHADER_OUTPUT})
endforeach ()
add_custom_target(CompileShaders ALL DEPENDS ${SHADER_OUTPUT_FILES})
add_dependencies(${TARGET_NAME} CompileShaders)