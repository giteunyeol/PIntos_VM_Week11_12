cmake_minimum_required(VERSION 3.16)

project(pintos_lab_index C ASM)

# Indexing-only CMake model for CLion.
# Build Pintos with the existing Makefile/Docker targets, not with CMake.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_filename_component(PINTOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../pintos" ABSOLUTE)
get_filename_component(LAB_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

file(GLOB_RECURSE PINTOS_KERNEL_SOURCES CONFIGURE_DEPENDS
    "${PINTOS_ROOT}/threads/*.c"
    "${PINTOS_ROOT}/devices/*.c"
    "${PINTOS_ROOT}/filesys/*.c"
    "${PINTOS_ROOT}/userprog/*.c"
    "${PINTOS_ROOT}/vm/*.c"
)

file(GLOB PINTOS_LIB_SOURCES CONFIGURE_DEPENDS
    "${PINTOS_ROOT}/lib/*.c"
)

file(GLOB PINTOS_KERNEL_LIB_SOURCES CONFIGURE_DEPENDS
    "${PINTOS_ROOT}/lib/kernel/*.c"
)

file(GLOB_RECURSE PINTOS_USER_TEST_SOURCES CONFIGURE_DEPENDS
    "${PINTOS_ROOT}/lib/user/*.c"
    "${PINTOS_ROOT}/tests/*.c"
)

file(GLOB_RECURSE PINTOS_HEADERS CONFIGURE_DEPENDS
    "${PINTOS_ROOT}/include/*.h"
    "${PINTOS_ROOT}/tests/*.h"
)

file(GLOB_RECURSE PINTOS_ASM_SOURCES CONFIGURE_DEPENDS
    "${PINTOS_ROOT}/threads/*.S"
    "${PINTOS_ROOT}/userprog/*.S"
)

add_library(pintos_kernel_index OBJECT EXCLUDE_FROM_ALL
    ${PINTOS_KERNEL_SOURCES}
    ${PINTOS_LIB_SOURCES}
    ${PINTOS_KERNEL_LIB_SOURCES}
    ${PINTOS_HEADERS}
)

target_compile_definitions(pintos_kernel_index PRIVATE
    USERPROG
    FILESYS
    VM
    EFILESYS
)

target_include_directories(pintos_kernel_index BEFORE PRIVATE
    "${PINTOS_ROOT}"
    "${PINTOS_ROOT}/include/lib"
    "${PINTOS_ROOT}/include"
    "${PINTOS_ROOT}/include/lib/kernel"
)

target_compile_options(pintos_kernel_index PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
)

add_library(pintos_user_tests_index OBJECT EXCLUDE_FROM_ALL
    ${PINTOS_USER_TEST_SOURCES}
    ${PINTOS_HEADERS}
)

target_compile_definitions(pintos_user_tests_index PRIVATE
    USERPROG
    FILESYS
    VM
)

target_include_directories(pintos_user_tests_index BEFORE PRIVATE
    "${PINTOS_ROOT}"
    "${PINTOS_ROOT}/include/lib"
    "${PINTOS_ROOT}/include"
    "${PINTOS_ROOT}/include/lib/user"
    "${PINTOS_ROOT}/include/lib/kernel"
)

target_compile_options(pintos_user_tests_index PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
)

add_custom_target(pintos_asm_index SOURCES
    ${PINTOS_ASM_SOURCES}
)

file(GLOB_RECURSE PINTOS_LAB_DOCS CONFIGURE_DEPENDS
    "${LAB_ROOT}/docs/*.md"
    "${LAB_ROOT}/README.md"
    "${CMAKE_CURRENT_LIST_DIR}/README.md"
    "${CMAKE_CURRENT_LIST_DIR}/TESTING.md"
    "${PINTOS_ROOT}/README.md"
)

if(PINTOS_LAB_DOCS)
    add_custom_target(pintos_lab_docs SOURCES
        ${PINTOS_LAB_DOCS}
    )
endif()

source_group(TREE "${PINTOS_ROOT}" PREFIX "pintos" FILES
    ${PINTOS_KERNEL_SOURCES}
    ${PINTOS_LIB_SOURCES}
    ${PINTOS_KERNEL_LIB_SOURCES}
    ${PINTOS_USER_TEST_SOURCES}
    ${PINTOS_HEADERS}
    ${PINTOS_ASM_SOURCES}
)

source_group(TREE "${LAB_ROOT}" PREFIX "docs" FILES
    ${PINTOS_LAB_DOCS}
)
