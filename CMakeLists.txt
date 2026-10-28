cmake_minimum_required(VERSION 3.16)
project(fsmn_kws VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(FSMN_KWS_BUILD_EXAMPLES "Build Linux WAV and PCM stream examples" ON)
option(FSMN_KWS_BUILD_TESTS "Build tests without downloading models" ON)
option(FSMN_KWS_PORTABLE_MATH "Use pinned FP32 adaptive powers" ON)

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
   NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "This release supports GCC and Clang toolchains")
endif()

set(FFT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/kissfft")
set(AGC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webrtc_agc")
set(MATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/musl_math")

add_library(fsmn_kws STATIC
    src/engine.cpp
    src/frontend/frontend.cpp
    src/frontend/dsp.c
    src/frontend/draec.c
    src/frontend/fft.c
    src/frontend/agc.c
    ${FFT}/kiss_fft.c ${FFT}/kiss_fftr.c
    ${AGC}/webrtc/modules/audio_processing/agc/legacy/digital_agc.c
    ${AGC}/webrtc/common_audio/signal_processing/division_operations.c
    ${AGC}/webrtc/common_audio/signal_processing/resample_by_2.c
    ${AGC}/webrtc/common_audio/signal_processing/spl_sqrt.c)
add_library(fsmn_kws::fsmn_kws ALIAS fsmn_kws)
target_compile_features(fsmn_kws PUBLIC cxx_std_11 PRIVATE cxx_std_17 c_std_11)
set_target_properties(fsmn_kws PROPERTIES
    C_STANDARD_REQUIRED ON CXX_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_include_directories(fsmn_kws
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src src/frontend ${FFT} ${AGC} ${MATH})
target_compile_definitions(fsmn_kws PRIVATE WEBRTC_POSIX WEBRTC_LINUX)
# These flags preserve recursive FP32 state and WebRTC signed wraparound.
target_compile_options(fsmn_kws PRIVATE
    -ffp-contract=off -fno-fast-math -fwrapv -fno-strict-aliasing
    -Wall -Wextra -Werror)
target_link_libraries(fsmn_kws PUBLIC m)

if(FSMN_KWS_PORTABLE_MATH)
    target_sources(fsmn_kws PRIVATE
        ${MATH}/powf.c ${MATH}/powf_data.c ${MATH}/exp2f_data.c)
    target_compile_options(fsmn_kws PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/portable_math.h>")
endif()

if(FSMN_KWS_BUILD_EXAMPLES)
    foreach(example kws_wav kws_stream frontend_wav)
        add_executable(${example} examples/${example}.cpp)
        target_link_libraries(${example} PRIVATE fsmn_kws::fsmn_kws)
        target_compile_options(${example} PRIVATE -Wall -Wextra -Werror)
    endforeach()
endif()

if(FSMN_KWS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS fsmn_kws EXPORT fsmn_kwsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT fsmn_kwsTargets NAMESPACE fsmn_kws::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fsmn_kws)
configure_package_config_file(cmake/fsmn_kwsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/fsmn_kwsConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fsmn_kws)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/fsmn_kwsConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fsmn_kwsConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/fsmn_kwsConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fsmn_kws)
install(FILES README.md README.en.md LICENSE NOTICE
              THIRD_PARTY_NOTICES.md THIRD_PARTY_NOTICES.en.md SOURCE_PROVENANCE.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fsmn_kws)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/fsmn_kws/docs
    FILES_MATCHING PATTERN "*.md" PATTERN "*.json")
install(FILES models/README.md models/README.en.md models/nihaomiya.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fsmn_kws/models)
install(DIRECTORY third_party/ DESTINATION ${CMAKE_INSTALL_DATADIR}/fsmn_kws/third_party
    FILES_MATCHING PATTERN "COPYING" PATTERN "COPYRIGHT" PATTERN "PATENTS"
                   PATTERN "AUTHORS" PATTERN "PROVENANCE.json"
                   PATTERN "BSD-3-Clause")
