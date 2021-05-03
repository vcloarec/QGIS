set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR}/sqldrivers)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/${QGIS_PLUGIN_SUBDIR}/sqldrivers)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

# do you need this?
include_directories(SYSTEM
	${Qt5Sql_PRIVATE_INCLUDE_DIRS}
)

set(ODBCPROXYDRIVER_SRC qgssqlodbcproxydriver.cpp qgssqlodbcproxydriver.h)

add_library(qqgsodbcproxy SHARED ${ODBCPROXYDRIVER_SRC})

target_link_libraries(qqgsodbcproxy
    qgis_core
  ${Qt5Core_LIBRARIES}
  ${Qt5Sql_LIBRARIES}
)

if(MSVC)
  target_link_libraries(qqgsodbcproxy wsock32)
endif()

install(TARGETS qqgsodbcproxy
  RUNTIME DESTINATION ${QT_PLUGINS_DIR}/sqldrivers
  LIBRARY DESTINATION ${QT_PLUGINS_DIR}/sqldrivers
)
