if(NOT OPENSCENEGRAPH_FOUND OR NOT OPENGL_FOUND)
    return()
endif()

add_module(OSGRenderer OSGRenderer.cpp OSGRenderer_vistle.cpp)

if(WIN32)
else(WIN32)
set_source_files_properties(OSGRenderer.cpp PROPERTIES COMPILE_FLAGS "-Wno-overloaded-virtual")
set_source_files_properties(OSGRenderer_vistle.cpp PROPERTIES COMPILE_FLAGS "-Wno-overloaded-virtual")
endif(WIN32)

include_directories(
        ${Boost_INCLUDE_DIRS}
        ${MPI_C_INCLUDE_PATH}
        ${OPENSCENEGRAPH_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIRS}
        ${ICET_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(OSGRenderer
        ${Boost_LIBRARIES}
        ${MPI_C_LIBRARIES}
        ${OPENSCENEGRAPH_LIBRARIES}
        ${OPENGL_LIBRARIES}
        vistle_module
        ${ICET_CORE_LIBS}
        ${ICET_MPI_LIBS}
)
