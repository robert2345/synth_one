cmake_minimum_required(VERSION 3.16)
project(SynthOne)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This assumes the SDL source is available in ./SDL
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# Create your game executable target as usual
add_executable(synth_one synth_one.c low_pass_filter.c square_controller.c square_controller.c text.c delay.c distortion.c envelope.c)

# Link to the actual SDL3 library.
target_link_libraries(synth_one PRIVATE SDL3::SDL3)
# Link math lib
target_link_libraries(synth_one PRIVATE m)

