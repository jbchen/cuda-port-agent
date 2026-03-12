cmake_minimum_required(VERSION 3.21)

# Set HIP architecture for MI250x before project() enables HIP
set(CMAKE_HIP_ARCHITECTURES "gfx90a")

project(nbody LANGUAGES C CXX HIP)

# Find HIP
find_package(hip REQUIRED)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find OpenGL and GLUT (optional — headless benchmark mode works without them)
find_package(OpenGL QUIET)
find_package(GLUT QUIET)

# Build the HIP kernel as a separate object library to isolate HIP compile flags
add_library(nbody_hip OBJECT bodysystemcuda.hip)
set_source_files_properties(bodysystemcuda.hip PROPERTIES LANGUAGE HIP)
target_link_libraries(nbody_hip PRIVATE hip::device)
target_include_directories(nbody_hip PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Build the main executable with CXX sources
add_executable(nbody
    render_particles.cpp
    nbody.cpp
    $<TARGET_OBJECTS:nbody_hip>
)

target_compile_features(nbody PRIVATE cxx_std_17)

target_include_directories(nbody PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link HIP runtime for the CXX files (hip::host does not add --offload-arch)
target_link_libraries(nbody hip::host)

if(OpenGL_FOUND AND GLUT_FOUND)
    target_include_directories(nbody PRIVATE
        ${OPENGL_INCLUDE_DIR}
        ${GLUT_INCLUDE_DIRS}
    )
    target_link_libraries(nbody
        ${OPENGL_LIBRARIES}
        ${GLUT_LIBRARIES}
    )
else()
    message(STATUS "OpenGL/GLUT not found - building in headless (benchmark/compare) mode only")
    target_compile_definitions(nbody PRIVATE HEADLESS_BUILD)
    target_compile_definitions(nbody_hip PRIVATE HEADLESS_BUILD)
endif()
