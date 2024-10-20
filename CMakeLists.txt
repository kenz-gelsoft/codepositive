cmake_minimum_required(VERSION 3.30)
project(CodePositive)

SET(CodePositive_SOURCES
	LauncherApp.cpp
	LauncherWindow.cpp
	AuthenticationPanel.cpp
)

SET(CodePositive_LIBRARIES
    JavaScriptCore
#    WebCore
    shared
    WebKitLegacy
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${ICU_I18N_LIBRARIES}
    ${ICU_LIBRARIES}
    be bsd network stdc++ translation tracker
)

INCLUDE_DIRECTORIES(
    "${WEBKITLEGACY_DIR}/haiku/API"
    /system/develop/headers/private/netservices/
)

add_definitions(-D_DEFAULT_SOURCE)
ADD_EXECUTABLE(CodePositive ${CodePositive_SOURCES})
TARGET_LINK_LIBRARIES(CodePositive ${CodePositive_LIBRARIES})
SET_TARGET_PROPERTIES(CodePositive PROPERTIES FOLDER "Tools")
SET_TARGET_PROPERTIES(CodePositive PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
