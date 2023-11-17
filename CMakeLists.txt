cmake_minimum_required(VERSION 3.7)

project(plotjuggler_elroy_plugins)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-------  Include Qt dependencies -------
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Concurrent
    Xml
    Svg
    OpenGL)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}")
find_package(pcapplusplus REQUIRED)
#find_package(sqlite3 REQUIRED)
#find_package(cachegrind REQUIRED)

include_directories(
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5Widgets_INCLUDE_DIRS}
    ${Qt5Xml_INCLUDE_DIRS}
    ${Qt5Svg_INCLUDE_DIRS} )

set(QT_LIBRARIES
    Qt5::Core
    Qt5::Widgets
    Qt5::Xml
    Qt5::Svg )

add_subdirectory(extern/elroy_common_msg)

add_definitions( ${QT_DEFINITIONS} -DQT_PLUGIN )
set( PJ_LIBRARIES ${QT_LIBRARIES} elroy_common_msg )

#--------------------------------------------------------
#-------------- Build with CATKIN (ROS1) ----------------
if( CATKIN_DEVEL_PREFIX OR catkin_FOUND OR CATKIN_BUILD_BINARY_PACKAGE)

    set(COMPILING_WITH_CATKIN 1)
    message(STATUS "COMPILING_WITH_CATKIN")
    add_definitions(-DCOMPILED_WITH_CATKIN)

    find_package(catkin REQUIRED COMPONENTS plotjuggler )
    catkin_package(
        CATKIN_DEPENDS plotjuggler
        )
    include_directories(${catkin_INCLUDE_DIRS})
    list(APPEND ${PJ_LIBRARIES} ${catkin_LIBRARIES} )
    set(PJ_PLUGIN_INSTALL_DIRECTORY ${CATKIN_PACKAGE_BIN_DESTINATION} )

    #--------------------------------------------------------
    #-------------- Build with AMENT (ROS2) -----------------
elseif( DEFINED ENV{AMENT_PREFIX_PATH})

    set(COMPILING_WITH_AMENT 1)
    message(STATUS "COMPILING_WITH_CATKIN")
    add_definitions(-DCOMPILED_WITH_AMENT)

    find_package(ament_cmake REQUIRED)
    find_package(plotjuggler REQUIRED)
    set(PJ_PLUGIN_INSTALL_DIRECTORY lib/${PROJECT_NAME} )

    #--------------------------------------------------------
    #------------- Build without any ROS support ------------
else()

    find_package(plotjuggler REQUIRED)
    message(STATUS "PlotJuggler FOUND")
    message(STATUS "plotjuggler_INCLUDE_DIR: ${plotjuggler_INCLUDE_DIR}")
    message(STATUS "plotjuggler_LIBRARIES: ${plotjuggler_LIBRARIES}")

    include_directories(${plotjuggler_INCLUDE_DIR})
    list(APPEND ${PJ_LIBRARIES} ${plotjuggler_LIBRARIES} )
    set(PJ_PLUGIN_INSTALL_DIRECTORY bin )

endif()

#------- Create the libraries -------

set(ECM_INCLUDES ${CMAKE_SOURCE_DIR}/extern/elroy_common_msg/generated/cpp/include
                 ${CMAKE_SOURCE_DIR}/extern/elroy_common_msg/private
)

add_library(PcapLoader SHARED
    PcapLoader/pcap_loader.h 
    PcapLoader/pcap_loader.cpp )
target_include_directories(
  PcapLoader PRIVATE ${PROJECT_SOURCE_DIR}/include ${ECM_INCLUDES}
)
target_link_libraries(PcapLoader
    ${PJ_LIBRARIES}
    pcapplusplus::pcapplusplus
)

add_executable(PcapLoaderExec PcapLoader/pcap_loader.cpp)
target_include_directories(
  PcapLoaderExec PRIVATE ${PROJECT_SOURCE_DIR}/include ${ECM_INCLUDES}
)
target_link_libraries(PcapLoaderExec 
  PcapLoader    
  ${PJ_LIBRARIES}
  pcapplusplus::pcapplusplus
) 

add_library(ElroyLogLoader SHARED
    ElroyLogLoader/elroy_log_loader.h 
    ElroyLogLoader/elroy_log_loader.cpp )
target_include_directories(
  ElroyLogLoader PRIVATE ${PROJECT_SOURCE_DIR}/include ${ECM_INCLUDES}
)
target_link_libraries(ElroyLogLoader
    PcapLoader
    sqlite3
    ${PJ_LIBRARIES}
    pcapplusplus::pcapplusplus
)
add_executable(ElroyLogLoaderExec ElroyLogLoader/elroy_log_loader.cpp)
target_include_directories(
  ElroyLogLoaderExec PRIVATE ${PROJECT_SOURCE_DIR}/include ${ECM_INCLUDES}
)
target_link_libraries(ElroyLogLoaderExec 
  ElroyLogLoader
  ${PJ_LIBRARIES}
  pcapplusplus::pcapplusplus
) 

#target_include_directories(
#  PcapLoader
#  PRIVATE pcapplusplus::pcapplusplus ${PROJECT_SOURCE_DIR}/include)

#target_link_libraries(PlotjugglerEl2Loader
#    ${PJ_LIBRARIES})

if (COMPILING_WITH_AMENT)
    #ament_target_dependencies(ElroyParser plotjuggler)
    ament_target_dependencies(PcapLoader plotjuggler)
    ament_target_dependencies(ElroyLogLoader plotjuggler)
    #ament_target_dependencies(PlotjugglerEl2Loader plotjuggler)
endif()
#------- Install the libraries -------

install(
    TARGETS
        #ElroyParser
        PcapLoader
        PcapLoaderExec
        ElroyLogLoader
        ElroyLogLoaderExec
        # PlotjugglerEl2Loader
    DESTINATION
        ${PJ_PLUGIN_INSTALL_DIRECTORY}  )
