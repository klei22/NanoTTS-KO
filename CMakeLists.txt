cmake_minimum_required(VERSION 3.16)
project(NanoTTS VERSION 2.0.0 LANGUAGES C)

option(NANOTTS_BUILD_CLI "Build the Korean command-line tool" ON)
option(NANOTTS_BUILD_TESTS "Build tests" ON)
option(NANOTTS_BUILD_EXAMPLES "Build MCU integration examples" ON)
option(NANOTTS_BUILD_PWM "Build PCM-to-PWM adapter" ON)
option(NANOTTS_BUILD_PCM "Build raw PCM16LE adapter" ON)
option(NANOTTS_ENABLE_STRICT_WARNINGS "Enable strict warnings" ON)
option(NANOTTS_ENABLE_BUILTIN_LEXICON "Build compact Korean exception lexicon" ON)
option(NANOTTS_ENABLE_MORPHOLOGY "Build the compact Korean morphology analyzer" ON)
option(NANOTTS_ENABLE_NORMALIZATION "Build semantic text normalization" ON)
option(NANOTTS_USE_LIBM "Use system libm" ON)

set(NANOTTS_MAX_EVENTS "640" CACHE STRING "Maximum buffered acoustic events")
set(NANOTTS_MAX_SYLLABLES "192" CACHE STRING "Maximum buffered Korean syllables")
set(NANOTTS_MAX_TOKENS "96" CACHE STRING "Maximum buffered text tokens")
set(NANOTTS_MAX_MORPHEMES "192" CACHE STRING "Maximum buffered Korean morphemes")
set(NANOTTS_CONTEXT_BYTES "16384" CACHE STRING "Opaque public context bytes")
set(NANOTTS_AUDIO_BLOCK "128" CACHE STRING "PCM samples per callback block")
set(NANOTTS_CONTROL_MS "1" CACHE STRING "Acoustic control-frame duration in milliseconds")
set(NANOTTS_COEFF_STRIDE "2" CACHE STRING "Samples between smoothed filter-coefficient updates")
foreach(v NANOTTS_MAX_EVENTS NANOTTS_MAX_SYLLABLES NANOTTS_MAX_TOKENS NANOTTS_MAX_MORPHEMES NANOTTS_CONTEXT_BYTES NANOTTS_AUDIO_BLOCK NANOTTS_CONTROL_MS NANOTTS_COEFF_STRIDE)
  if(NOT ${v} MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "${v} must be a positive integer")
  endif()
endforeach()
if(NANOTTS_CONTROL_MS GREATER 4)
  message(FATAL_ERROR "NANOTTS_CONTROL_MS must be between 1 and 4")
endif()
if(NANOTTS_COEFF_STRIDE GREATER 4)
  message(FATAL_ERROR "NANOTTS_COEFF_STRIDE must be between 1 and 4")
endif()

set(NANOTTS_SOURCES
  src/nanotts.c
  src/nanotts_ko.c
  src/nanotts_ko_unicode.c
  src/nanotts_ko_normalize.c
  src/nanotts_ko_lexicon.c
  src/nanotts_ko_morph.c
  src/nanotts_ko_phonology.c
  src/nanotts_ko_prosody.c
  src/nanotts_voice.c
  src/nanotts_synth.c)

add_library(nanotts ${NANOTTS_SOURCES})
add_library(NanoTTS::NanoTTS ALIAS nanotts)
set_target_properties(nanotts PROPERTIES
  OUTPUT_NAME nanotts EXPORT_NAME NanoTTS
  VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_compile_features(nanotts PUBLIC c_std_99)
target_include_directories(nanotts PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(nanotts PUBLIC
  NANOTTS_MAX_EVENTS=${NANOTTS_MAX_EVENTS}
  NANOTTS_MAX_SYLLABLES=${NANOTTS_MAX_SYLLABLES}
  NANOTTS_MAX_TOKENS=${NANOTTS_MAX_TOKENS}
  NANOTTS_MAX_MORPHEMES=${NANOTTS_MAX_MORPHEMES}
  NANOTTS_CONTEXT_BYTES=${NANOTTS_CONTEXT_BYTES}
  NANOTTS_CONTROL_MS=${NANOTTS_CONTROL_MS}
  NANOTTS_COEFF_STRIDE=${NANOTTS_COEFF_STRIDE}
  PRIVATE
  NANOTTS_AUDIO_BLOCK=${NANOTTS_AUDIO_BLOCK}
  NANOTTS_USE_LIBM=$<BOOL:${NANOTTS_USE_LIBM}>
  NANOTTS_ENABLE_BUILTIN_LEXICON=$<BOOL:${NANOTTS_ENABLE_BUILTIN_LEXICON}>
  NANOTTS_ENABLE_MORPHOLOGY=$<BOOL:${NANOTTS_ENABLE_MORPHOLOGY}>
  NANOTTS_ENABLE_NORMALIZATION=$<BOOL:${NANOTTS_ENABLE_NORMALIZATION}>)

function(nanotts_warnings target)
  if(NANOTTS_ENABLE_STRICT_WARNINGS)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
  endif()
endfunction()
nanotts_warnings(nanotts)
if(NANOTTS_USE_LIBM AND NOT MSVC)
  target_link_libraries(nanotts PUBLIC m)
endif()

set(NANOTTS_INSTALL_TARGETS nanotts)
if(NANOTTS_BUILD_PWM)
  add_library(nanotts_pwm src/output/nanotts_pwm.c)
  add_library(NanoTTS::PWM ALIAS nanotts_pwm)
  set_target_properties(nanotts_pwm PROPERTIES OUTPUT_NAME nanotts_pwm EXPORT_NAME PWM)
  target_compile_features(nanotts_pwm PUBLIC c_std_99)
  target_link_libraries(nanotts_pwm PUBLIC NanoTTS::NanoTTS)
  nanotts_warnings(nanotts_pwm)
  list(APPEND NANOTTS_INSTALL_TARGETS nanotts_pwm)
endif()
if(NANOTTS_BUILD_PCM)
  add_library(nanotts_pcm src/output/nanotts_pcm.c)
  add_library(NanoTTS::PCM ALIAS nanotts_pcm)
  set_target_properties(nanotts_pcm PROPERTIES OUTPUT_NAME nanotts_pcm EXPORT_NAME PCM)
  target_compile_features(nanotts_pcm PUBLIC c_std_99)
  target_link_libraries(nanotts_pcm PUBLIC NanoTTS::NanoTTS)
  nanotts_warnings(nanotts_pcm)
  list(APPEND NANOTTS_INSTALL_TARGETS nanotts_pcm)
endif()

if(NANOTTS_BUILD_CLI)
  add_executable(nanotts_cli tools/nanotts_cli.c)
  set_target_properties(nanotts_cli PROPERTIES OUTPUT_NAME nanotts)
  target_link_libraries(nanotts_cli PRIVATE NanoTTS::NanoTTS)
  target_compile_definitions(nanotts_cli PRIVATE
    NANOTTS_CLI_HAVE_PWM=$<BOOL:${NANOTTS_BUILD_PWM}>
    NANOTTS_CLI_HAVE_PCM=$<BOOL:${NANOTTS_BUILD_PCM}>)
  if(NANOTTS_BUILD_PWM)
    target_link_libraries(nanotts_cli PRIVATE NanoTTS::PWM)
  endif()
  if(NANOTTS_BUILD_PCM)
    target_link_libraries(nanotts_cli PRIVATE NanoTTS::PCM)
  endif()
  nanotts_warnings(nanotts_cli)
endif()

if(NANOTTS_BUILD_EXAMPLES)
  add_executable(nanotts_mcu_example examples/nanotts_mcu_callback.c)
  target_link_libraries(nanotts_mcu_example PRIVATE NanoTTS::NanoTTS)
  nanotts_warnings(nanotts_mcu_example)
  add_executable(nanotts_lexicon_example examples/nanotts_lexicon.c)
  target_link_libraries(nanotts_lexicon_example PRIVATE NanoTTS::NanoTTS)
  nanotts_warnings(nanotts_lexicon_example)
  add_executable(nanotts_analysis_example examples/nanotts_analysis.c)
  target_link_libraries(nanotts_analysis_example PRIVATE NanoTTS::NanoTTS)
  nanotts_warnings(nanotts_analysis_example)
  if(NANOTTS_BUILD_PWM)
    add_executable(nanotts_pwm_example examples/nanotts_mcu_pwm.c)
    target_link_libraries(nanotts_pwm_example PRIVATE NanoTTS::PWM)
    nanotts_warnings(nanotts_pwm_example)
  endif()
endif()

if(NANOTTS_BUILD_TESTS)
  enable_testing()
  foreach(name api korean morphology normalization styles prosody synth declick transitions fuzz)
    add_executable(test_${name} tests/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE NanoTTS::NanoTTS)
    target_compile_definitions(test_${name} PRIVATE
      NANOTTS_ENABLE_BUILTIN_LEXICON=$<BOOL:${NANOTTS_ENABLE_BUILTIN_LEXICON}>
      NANOTTS_ENABLE_MORPHOLOGY=$<BOOL:${NANOTTS_ENABLE_MORPHOLOGY}>
      NANOTTS_ENABLE_NORMALIZATION=$<BOOL:${NANOTTS_ENABLE_NORMALIZATION}>)
    if(NOT MSVC)
      target_compile_options(test_${name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
  endforeach()
  add_executable(test_pronunciations tests/test_pronunciations.c)
  target_link_libraries(test_pronunciations PRIVATE NanoTTS::NanoTTS)
  target_compile_definitions(test_pronunciations PRIVATE
    NANOTTS_ENABLE_BUILTIN_LEXICON=$<BOOL:${NANOTTS_ENABLE_BUILTIN_LEXICON}>
    NANOTTS_ENABLE_MORPHOLOGY=$<BOOL:${NANOTTS_ENABLE_MORPHOLOGY}>
    NANOTTS_TEST_PRONUNCIATIONS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ko_pronunciations.tsv")
  if(NOT MSVC)
    target_compile_options(test_pronunciations PRIVATE -UNDEBUG)
  endif()
  add_test(NAME pronunciations COMMAND test_pronunciations)
  if(NANOTTS_BUILD_PWM)
    add_executable(test_pwm tests/test_pwm.c)
    target_link_libraries(test_pwm PRIVATE NanoTTS::PWM)
    if(NOT MSVC)
      target_compile_options(test_pwm PRIVATE -UNDEBUG)
    endif()
    add_test(NAME pwm COMMAND test_pwm)
  endif()
  if(NANOTTS_BUILD_PCM)
    add_executable(test_pcm tests/test_pcm.c)
    target_link_libraries(test_pcm PRIVATE NanoTTS::PCM)
    if(NOT MSVC)
      target_compile_options(test_pcm PRIVATE -UNDEBUG)
    endif()
    add_test(NAME pcm COMMAND test_pcm)
  endif()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS ${NANOTTS_INSTALL_TARGETS}
  EXPORT NanoTTSTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/nanotts DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
install(EXPORT NanoTTSTargets NAMESPACE NanoTTS::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoTTS)
configure_package_config_file(cmake/nanottsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoTTS)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/NanoTTSConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NanoTTS)

message(STATUS "NanoTTS-KO 2: Korean-only; lexicon=${NANOTTS_ENABLE_BUILTIN_LEXICON}; morphology=${NANOTTS_ENABLE_MORPHOLOGY}; normalization=${NANOTTS_ENABLE_NORMALIZATION}; libm=${NANOTTS_USE_LIBM}")
