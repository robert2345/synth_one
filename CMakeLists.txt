cmake_minimum_required(VERSION 3.16)
project(AudioSDL3)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This assumes the SDL source is available in ./SDL
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# Create your game executable target as usual
add_executable(audio_sdl3 audio_sdl3.c)

# Link to the actual SDL3 library.
target_link_libraries(audio_sdl3 PRIVATE SDL3::SDL3)
target_link_libraries(audio_sdl3 PRIVATE m)

