cmake_minimum_required(VERSION 3.21)

# Build nativo de Ben 10: Protector of Earth (SLUS_216.61, NTSC-U v1.01).
#
# Usa o runtime do PS2Recomp (external/PS2Recomp) como biblioteca e compila o
# código gerado (generated/) em targets separados, para iterar rápido:
#   ben10poe_generated  funções recompiladas (unity build + PCH, -O2)
#   ben10poe            executável: tabela de funções (-O0), overrides e main

project(Ben10POERecomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PS2RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/PS2Recomp" CACHE PATH "Clone do PS2Recomp")
set(BEN10_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated" CACHE PATH "Saída do ps2_recomp")
set(BEN10_UNITY_BATCH 32 CACHE STRING "Funções por TU no unity build do código gerado")

if(NOT EXISTS "${PS2RECOMP_DIR}/ps2xRuntime/CMakeLists.txt")
    message(FATAL_ERROR "PS2Recomp não encontrado em ${PS2RECOMP_DIR}")
endif()
if(NOT EXISTS "${BEN10_GENERATED_DIR}/register_functions.cpp")
    message(FATAL_ERROR "Código gerado não encontrado em ${BEN10_GENERATED_DIR}: rode o ps2_recomp antes")
endif()

# GCC/Clang no x86-64 precisam habilitar SSE4.1/AVX2 explicitamente (PS2Recomp#261).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64" AND NOT MSVC)
    add_compile_options(-march=x86-64-v3)
endif()

option(PS2X_ENABLE_AGRESSIVE_LOGS "Enable very verbose/agressive PS2 runtime logs" OFF)
set(PS2X_IS_ARM_TARGET OFF)
add_subdirectory("${PS2RECOMP_DIR}/ps2xIOP" ps2xIOP EXCLUDE_FROM_ALL)
add_subdirectory("${PS2RECOMP_DIR}/ps2xRuntime" ps2xRuntime EXCLUDE_FROM_ALL)

set(RT_DIR "${PS2RECOMP_DIR}/ps2xRuntime")

# ---------------------------------------------------------------------------
# Código recompilado (sem register_functions.cpp)
# ---------------------------------------------------------------------------
file(GLOB BEN10_GENERATED_SOURCES "${BEN10_GENERATED_DIR}/*.cpp")
list(REMOVE_ITEM BEN10_GENERATED_SOURCES
    "${BEN10_GENERATED_DIR}/register_functions.cpp"
    "${BEN10_GENERATED_DIR}/vu1_recompiled.cpp"
)
list(SORT BEN10_GENERATED_SOURCES)

add_library(ben10poe_generated STATIC ${BEN10_GENERATED_SOURCES})
target_include_directories(ben10poe_generated PUBLIC "${BEN10_GENERATED_DIR}")
target_link_libraries(ben10poe_generated PUBLIC ps2_runtime)
set_target_properties(ben10poe_generated PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE "${BEN10_UNITY_BATCH}"
)
target_precompile_headers(ben10poe_generated PRIVATE
    "${RT_DIR}/include/ps2_runtime_macros.h"
    "${RT_DIR}/include/ps2_runtime.h"
    "${BEN10_GENERATED_DIR}/ps2_recompiled_functions.h"
    "${BEN10_GENERATED_DIR}/ps2_recompiled_stubs.h"
    "${RT_DIR}/include/ps2_syscalls.h"
    "${RT_DIR}/include/ps2_stubs.h"
)

# ---------------------------------------------------------------------------
# Executável
# ---------------------------------------------------------------------------
file(GLOB BEN10_OVERRIDE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/overrides/*.cpp")

add_executable(ben10poe
    "${RT_DIR}/src/main.cpp"
    "${BEN10_GENERATED_DIR}/register_functions.cpp"
    ${BEN10_OVERRIDE_SOURCES}
)
target_link_libraries(ben10poe PRIVATE ps2_runtime ben10poe_generated)
target_include_directories(ben10poe PRIVATE "${RT_DIR}/src/lib")

# A tabela de funções é um construtor grande; -O0 compila rápido e roda só no boot.
set_source_files_properties("${BEN10_GENERATED_DIR}/register_functions.cpp"
    PROPERTIES COMPILE_OPTIONS "-O0")

# Sink GPU paraLLEl-GS (opcional, ativado em runtime com BEN10_GS_BACKEND=gpu-present).
option(BEN10_ENABLE_PARALLEL_GS "Espelhar/apresentar GIF no paraLLEl-GS (Vulkan)" ON)
if(BEN10_ENABLE_PARALLEL_GS)
    set(BEN10_PARALLEL_GS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/.cache/parallel-gs-source"
        CACHE PATH "Checkout pinado do paraLLEl-GS")
    if(NOT EXISTS "${BEN10_PARALLEL_GS_SOURCE}/gs/gs_interface.hpp")
        message(FATAL_ERROR "paraLLEl-GS não encontrado em ${BEN10_PARALLEL_GS_SOURCE}")
    endif()
    set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
    set(GRANITE_AUDIO OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BEN10_PARALLEL_GS_SOURCE}" parallel-gs EXCLUDE_FROM_ALL)
    target_sources(ben10poe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime/parallel_gs/ben10_parallel_gs.cpp")
    target_include_directories(ben10poe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime/parallel_gs")
    target_link_libraries(ben10poe PRIVATE parallel-gs)
    target_compile_definitions(ben10poe PRIVATE BEN10_HAS_PARALLEL_GS=1)
endif()

if(PS2X_ENABLE_DEBUG_UI)
    target_sources(ben10poe PRIVATE "${RT_DIR}/src/lib/ps2_debug_panel.cpp")
    target_link_libraries(ben10poe PRIVATE rlImGui imgui)
    target_compile_definitions(ben10poe PRIVATE PS2X_ENABLE_DEBUG_UI=1)
endif()

# Microprogramas VU1 transpilados AOT (opcional, gerado por tools/vu1_transpile.py).
if(EXISTS "${BEN10_GENERATED_DIR}/vu1_recompiled.cpp")
    set_source_files_properties("${BEN10_GENERATED_DIR}/vu1_recompiled.cpp"
        PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
    target_sources(ben10poe PRIVATE "${BEN10_GENERATED_DIR}/vu1_recompiled.cpp")
endif()
