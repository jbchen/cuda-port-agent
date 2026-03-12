cmake_minimum_required(VERSION 3.21)

project(smokeParticles LANGUAGES C CXX HIP)

# Find HIP
find_package(hip REQUIRED)

# Set HIP architecture for MI250x
set(CMAKE_HIP_ARCHITECTURES gfx90a)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)

# Common/ directory contains helper headers and GL headers
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Common)
include_directories(${COMMON_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Use GL headers from Common/GL/ and system libGL
# GLUT/freeglut headers are in Common/GL/, system may or may not have the library
find_package(OpenGL QUIET)
find_package(GLUT QUIET)

# If system find_package didn't work, set paths manually from Common/
if(NOT OPENGL_FOUND)
    # Use system libGL if available
    find_library(OPENGL_gl_LIBRARY GL)
    find_library(OPENGL_glu_LIBRARY GLU)
    if(OPENGL_gl_LIBRARY)
        set(OPENGL_FOUND TRUE)
        set(OPENGL_LIBRARIES ${OPENGL_gl_LIBRARY})
        if(OPENGL_glu_LIBRARY)
            list(APPEND OPENGL_LIBRARIES ${OPENGL_glu_LIBRARY})
        endif()
    endif()
endif()

if(NOT GLUT_FOUND)
    # Use headers from Common/ and try to find system libglut
    find_library(GLUT_glut_LIBRARY NAMES glut freeglut)
    if(GLUT_glut_LIBRARY)
        set(GLUT_FOUND TRUE)
        set(GLUT_LIBRARIES ${GLUT_glut_LIBRARY})
    endif()
endif()

# Source files
set(SOURCES
    GLSLProgram.cpp
    ParticleSystem.cpp
    ParticleSystem_cuda.hip
    SmokeRenderer.cpp
    SmokeShaders.cpp
    framebufferObject.cpp
    particleDemo.cpp
    renderbuffer.cpp
)

# Set HIP language for .hip files
set_source_files_properties(ParticleSystem_cuda.hip PROPERTIES LANGUAGE HIP)

add_executable(smokeParticles ${SOURCES})

target_compile_features(smokeParticles PRIVATE cxx_std_17)

# HIP architecture is set via CMAKE_HIP_ARCHITECTURES (passed at cmake configure time)
# No need to manually set --offload-arch

target_link_libraries(smokeParticles
    hip::host
)

if(OPENGL_FOUND)
    target_link_libraries(smokeParticles ${OPENGL_LIBRARIES})
endif()

if(GLUT_FOUND)
    target_link_libraries(smokeParticles ${GLUT_LIBRARIES})
endif()

# Copy data files to build directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
    add_custom_command(TARGET smokeParticles POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        ${CMAKE_CURRENT_BINARY_DIR}/data
    )
endif()
