cmake_minimum_required(VERSION 3.16)
project(SynthOne)
set(APP_NAME "synth_one")

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This assumes the SDL source is available in ./SDL
add_subdirectory(SDL EXCLUDE_FROM_ALL)

add_executable(${APP_NAME} synth_one.c low_pass_filter.c square_controller.c square_controller.c text.c delay.c distortion.c envelope.c slide_controller.c midi.c sequencer.c ball.c)

# Link to the SDL3 library.
target_link_libraries(${APP_NAME} PRIVATE SDL3::SDL3)

# Link Alsalib
find_package(ALSA REQUIRED)                                    
if (ALSA_FOUND)                                                
    include_directories(${ALSA_INCLUDE_DIRS})                  
    target_link_libraries (${APP_NAME} PRIVATE ${ALSA_LIBRARIES})      
    # TODO make the code work without ALSA on windows for instance.
endif(ALSA_FOUND)  

# Link math lib
target_link_libraries(${APP_NAME} PRIVATE m)

