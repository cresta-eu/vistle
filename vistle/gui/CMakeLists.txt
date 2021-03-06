find_package(Boost 1.40 REQUIRED COMPONENTS python)

set(SAVED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
if (APPLE)
   set(CMAKE_PREFIX_PATH /usr/local/opt/qt5 ${CMAKE_PREFIX_PATH})
endif()
set(CMAKE_PREFIX_PATH $ENV{EXTERNLIBS}/qt5 ${CMAKE_PREFIX_PATH})

find_package(Qt5Core)
# don't bother to continue if not even QtCore was found
if (NOT Qt5Core_FOUND)
    message("Not building GUI: Qt5 Core library not found")
    return()
endif()
find_package(Qt5Widgets)
find_package(Qt5Gui)

set(CMAKE_PREFIX_PATH ${SAVED_CMAKE_PREFIX_PATH})


add_subdirectory(propertybrowser)

set(CMAKE_AUTOMOC ON)

include_directories(
  ${Boost_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_PATH}
  ..
  ${CMAKE_CURRENT_BINARY_DIR}
  .
  ../../3rdparty/qt-solutions/qtpropertybrowser/src
)

set(gui_SOURCES
  main.cpp
  mainwindow.cpp
  connection.cpp
  module.cpp
  dataflownetwork.cpp
  port.cpp
  vistleobserver.cpp
  parameters.cpp
  modulebrowser.cpp
  dataflowview.cpp
  vistleconsole.cpp
  modifieddialog.cpp
  uicontroller.cpp

  qconsole/qconsole.cpp
)

set(gui_HEADERS
  mainwindow.h
  connection.h
  module.h
  dataflownetwork.h
  port.h
  vistleobserver.h
  parameters.h
  modulebrowser.h
  dataflowview.h
  vistleconsole.h
  modifieddialog.h
  uicontroller.h

  qconsole/qconsole.h
)

set(gui_FORMS
  mainwindow.ui
  modulebrowser.ui
  modifieddialog.ui
)

set(gui_name vistle_gui)
if (APPLE)
  add_custom_target(gui_wrapper ALL
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/gui_wrapper.sh ${EXECUTABLE_OUTPUT_PATH}/${gui_name}
          DEPENDS gui_wrapper.sh
          SOURCES gui_wrapper.sh
  )
  set(MACOSX_BUNDLE_ICON_FILE vistle.icns)
  set(gui_SOURCES ${gui_SOURCES} ${MACOSX_BUNDLE_ICON_FILE})
  set_source_files_properties(${MACOSX_BUNDLE_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set(gui_name Vistle)
endif(APPLE)


qt5_wrap_ui(GUI_UI_SRCS ${gui_FORMS})
add_definitions(${QT_DEFINITIONS})
add_executable(${gui_name}
  MACOSX_BUNDLE
  ${gui_SOURCES} 
  ${GUI_UI_SRCS})

qt5_use_modules(${gui_name} Widgets)

qt5_add_resources(${gui_name} gui.qrc)

target_link_libraries(${gui_name}
  ${QT_LIBRARIES}
  ${Boost_LIBRARIES}
  ${PYTHON_LIBRARIES}
  vistle_core
  vistle_userinterface
  vistle_propertybrowser
)
