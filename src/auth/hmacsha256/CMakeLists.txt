set(AUTH_HMACSHA256_SRCS
  core/qgsauthhmacsha256method.cpp
)

set(AUTH_HMACSHA256_HDRS
  core/qgsauthhmacsha256method.h
)

set(AUTH_HMACSHA256_UIS_H "")

if (WITH_GUI)
  set(AUTH_HMACSHA256_SRCS ${AUTH_HMACSHA256_SRCS}
    gui/qgsauthhmacsha256edit.cpp
  )
  set(AUTH_HMACSHA256_HDRS ${AUTH_HMACSHA256_HDRS}
    gui/qgsauthhmacsha256edit.h
  )
  set(AUTH_HMACSHA256_UIS gui/qgsauthhmacsha256edit.ui)
  if (WITH_QT6)
    QT6_WRAP_UI(AUTH_HMACSHA256_UIS_H ${AUTH_HMACSHA256_UIS})
  else()
    QT5_WRAP_UI(AUTH_HMACSHA256_UIS_H ${AUTH_HMACSHA256_UIS})
  endif()
endif()


# static library
add_library(authmethod_hmacsha256_a STATIC ${AUTH_HMACSHA256_SRCS} ${AUTH_HMACSHA256_HDRS} ${AUTH_HMACSHA256_UIS_H})

target_include_directories(authmethod_hmacsha256_a PUBLIC ${CMAKE_SOURCE_DIR}/src/auth/hmacsha256/core)

# require c++17
target_compile_features(authmethod_hmacsha256_a PRIVATE cxx_std_17)

target_link_libraries(authmethod_hmacsha256_a qgis_core)

if (WITH_GUI)
  target_include_directories(authmethod_hmacsha256_a PRIVATE
    ${CMAKE_SOURCE_DIR}/src/auth/hmacsha256/gui
    ${CMAKE_BINARY_DIR}/src/auth/hmacsha256
  )
  target_link_libraries (authmethod_hmacsha256_a qgis_gui)
endif()

target_compile_definitions(authmethod_hmacsha256_a PRIVATE "-DQT_NO_FOREACH")

if (FORCE_STATIC_LIBS)
  # for (external) mobile apps to be able to pick up provider for linking
  install (TARGETS authmethod_hmacsha256_a ARCHIVE DESTINATION ${QGIS_PLUGIN_DIR})
else()
  # dynamically loaded module
  add_library(authmethod_hmacsha256 MODULE ${AUTH_HMACSHA256_SRCS} ${AUTH_HMACSHA256_HDRS} ${AUTH_HMACSHA256_UIS_H})

  # require c++17
  target_compile_features(authmethod_hmacsha256 PRIVATE cxx_std_17)

  target_link_libraries(authmethod_hmacsha256 qgis_core)

  if (WITH_GUI)
    target_include_directories(authmethod_hmacsha256 PRIVATE
      ${CMAKE_SOURCE_DIR}/src/auth/hmacsha256/gui
      ${CMAKE_BINARY_DIR}/src/auth/hmacsha256
    )
    target_link_libraries (authmethod_hmacsha256 qgis_gui)
    add_dependencies(authmethod_hmacsha256 ui)
  endif()

  target_compile_definitions(authmethod_hmacsha256 PRIVATE "-DQT_NO_FOREACH")

  install (TARGETS authmethod_hmacsha256
    RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
    LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
  )
endif()
