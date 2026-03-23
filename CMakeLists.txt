cmake_minimum_required(VERSION 3.16)
project(cliphist VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(CTest)
include(cmake/Warnings.cmake)

set(CLIPHIST_ENABLE_X11_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CLIPHIST_ENABLE_X11_DEFAULT ON)
endif()
option(CLIPHIST_ENABLE_X11 "Enable X11 clipboard/tray integration" ${CLIPHIST_ENABLE_X11_DEFAULT})

if(WIN32)
  foreach(_cliphist_prefix_hint
          "$ENV{Qt5_DIR}"
          "$ENV{QTDIR}"
          "$ENV{MSYSTEM_PREFIX}"
          "$ENV{MINGW_PREFIX}")
    if(_cliphist_prefix_hint)
      list(APPEND CMAKE_PREFIX_PATH "${_cliphist_prefix_hint}")
    endif()
  endforeach()
endif()

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
if(CLIPHIST_ENABLE_X11)
  find_package(X11 REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(XFIXES REQUIRED xfixes)
  pkg_check_modules(XFT REQUIRED xft)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif()

configure_file(packaging/cliphist.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/cliphist.desktop
  @ONLY
)

set(CLIPHIST_APP_SOURCES
  src/app/Application.cpp
  src/app/QtClipboardRuntime.cpp
  src/db/Database.cpp
  src/db/HistoryRepository.cpp
  src/core/Deduplicator.cpp
  src/core/RetentionPolicy.cpp
  src/cli/CommandParser.cpp
  src/cli/PrintFormatter.cpp
  src/ui/AppIcon.cpp
  src/ui/SimpleUi.cpp
  src/util/ArchiveStore.cpp
  src/util/Hash.cpp
  src/util/Logger.cpp
  src/util/Path.cpp
  src/util/Time.cpp
)

if(CLIPHIST_ENABLE_X11)
  list(APPEND CLIPHIST_APP_SOURCES
    src/app/DaemonRunner.cpp
    src/app/DesktopSession.cpp
    src/x11/ClipboardWatcher.cpp
    src/x11/StatusTrayIcon.cpp
  )
else()
  list(APPEND CLIPHIST_APP_SOURCES
    src/app/DaemonRunnerStub.cpp
    src/app/DesktopSessionStub.cpp
  )
endif()

add_library(cliphist_lib
  ${CLIPHIST_APP_SOURCES}
)

target_include_directories(cliphist_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CLIPHIST_ENABLE_X11)
  target_include_directories(cliphist_lib
    PUBLIC
      ${X11_INCLUDE_DIR}
      ${XFIXES_INCLUDE_DIRS}
      ${XFT_INCLUDE_DIRS}
      ${GTK3_INCLUDE_DIRS}
  )
endif()

target_link_libraries(cliphist_lib
  PUBLIC
    SQLite::SQLite3
    Threads::Threads
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

if(CLIPHIST_ENABLE_X11)
  target_link_libraries(cliphist_lib
    PUBLIC
      ${X11_LIBRARIES}
      ${XFIXES_LIBRARIES}
      ${XFT_LIBRARIES}
      ${GTK3_LIBRARIES}
  )
  target_compile_definitions(cliphist_lib PUBLIC CLIPHIST_HAS_X11=1)
else()
  target_compile_definitions(cliphist_lib PUBLIC CLIPHIST_HAS_X11=0)
endif()

target_compile_options(cliphist_lib PRIVATE ${PROJECT_WARNINGS})

add_executable(cliphist src/main.cpp)
target_sources(cliphist PRIVATE src/app/ProgramEntry.cpp)
if(WIN32)
  target_sources(cliphist PRIVATE src/app/cliphist.rc)
endif()
target_link_libraries(cliphist PRIVATE cliphist_lib)
target_compile_options(cliphist PRIVATE ${PROJECT_WARNINGS})

if(WIN32)
  add_executable(cliphist-gui WIN32 src/gui_main.cpp)
  target_sources(cliphist-gui PRIVATE src/app/ProgramEntry.cpp src/app/cliphist.rc)
  target_link_libraries(cliphist-gui PRIVATE cliphist_lib)
  target_compile_options(cliphist-gui PRIVATE ${PROJECT_WARNINGS})
endif()

install(TARGETS cliphist RUNTIME DESTINATION bin)
if(WIN32)
  install(TARGETS cliphist-gui RUNTIME DESTINATION bin)
endif()
if(WIN32)
  install(FILES assets/cliphist.ico DESTINATION .)
endif()

if(CLIPHIST_ENABLE_X11)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cliphist.desktop
          DESTINATION share/applications)
  install(FILES assets/cliphist.svg
          DESTINATION share/icons/hicolor/scalable/apps
          RENAME cliphist.svg)
endif()

if(WIN32)
  function(cliphist_get_imported_location out_var target_name)
    set(_cliphist_location "")
    foreach(_cliphist_config RELEASE RELWITHDEBINFO MINSIZEREL DEBUG)
      if(NOT _cliphist_location)
        get_target_property(_cliphist_location ${target_name}
                            IMPORTED_LOCATION_${_cliphist_config})
      endif()
    endforeach()
    if(NOT _cliphist_location)
      get_target_property(_cliphist_location ${target_name} IMPORTED_LOCATION)
    endif()
    set(${out_var} "${_cliphist_location}" PARENT_SCOPE)
  endfunction()

  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  include(InstallRequiredSystemLibraries)

  set(CLIPHIST_WINDOWS_RUNTIME_HINT_DIRS "")
  if(TARGET Qt5::qmake)
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    if(QT_QMAKE_EXECUTABLE)
      get_filename_component(_cliphist_qt_qmake_dir "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
      list(APPEND CLIPHIST_WINDOWS_RUNTIME_HINT_DIRS "${_cliphist_qt_qmake_dir}")
      execute_process(
        COMMAND "${QT_QMAKE_EXECUTABLE}" -query QT_INSTALL_PLUGINS
        OUTPUT_VARIABLE QT_PLUGINS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    endif()
  endif()

  foreach(_cliphist_runtime_target Qt5::Core Qt5::Gui Qt5::Widgets SQLite::SQLite3)
    if(TARGET ${_cliphist_runtime_target})
      cliphist_get_imported_location(_cliphist_runtime_dll "${_cliphist_runtime_target}")
      if(_cliphist_runtime_dll AND EXISTS "${_cliphist_runtime_dll}")
        get_filename_component(_cliphist_runtime_dir "${_cliphist_runtime_dll}" DIRECTORY)
        list(APPEND CLIPHIST_WINDOWS_RUNTIME_HINT_DIRS "${_cliphist_runtime_dir}")
      endif()
    endif()
  endforeach()

  get_filename_component(_cliphist_compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  list(APPEND CLIPHIST_WINDOWS_RUNTIME_HINT_DIRS "${_cliphist_compiler_bin_dir}")
  list(REMOVE_DUPLICATES CLIPHIST_WINDOWS_RUNTIME_HINT_DIRS)

  find_program(CLIPHIST_WINDEPLOYQT_EXECUTABLE
               NAMES windeployqt windeployqt.exe
               HINTS ${CLIPHIST_WINDOWS_RUNTIME_HINT_DIRS})

  set(CLIPHIST_WINDOWS_PLATFORM_PLUGIN "")
  if(QT_PLUGINS_DIR AND EXISTS "${QT_PLUGINS_DIR}/platforms/qwindows.dll")
    set(CLIPHIST_WINDOWS_PLATFORM_PLUGIN "${QT_PLUGINS_DIR}/platforms/qwindows.dll")
  else()
    find_file(CLIPHIST_WINDOWS_PLATFORM_PLUGIN
              NAMES qwindows.dll
              HINTS ${CLIPHIST_WINDOWS_RUNTIME_HINT_DIRS}
              PATH_SUFFIXES
                ../plugins/platforms
                ../share/qt5/plugins/platforms
                plugins/platforms
                platforms)
  endif()

  configure_file(
    cmake/InstallWindowsRuntimeDeps.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/InstallWindowsRuntimeDeps.cmake
    @ONLY
  )
  install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/InstallWindowsRuntimeDeps.cmake)
endif()

if(BUILD_TESTING)
  add_executable(test_dedup tests/unit/test_dedup.cpp)
  target_link_libraries(test_dedup PRIVATE cliphist_lib)

  add_executable(test_retention tests/unit/test_retention.cpp)
  target_link_libraries(test_retention PRIVATE cliphist_lib)

  add_executable(test_repository tests/integration/test_repository.cpp)
  target_link_libraries(test_repository PRIVATE cliphist_lib)

  add_executable(test_cli_list tests/integration/test_cli_list.cpp)
  target_link_libraries(test_cli_list PRIVATE cliphist_lib)

  add_executable(test_command_parser tests/unit/test_command_parser.cpp)
  target_link_libraries(test_command_parser PRIVATE cliphist_lib)

  add_executable(test_app_icon tests/unit/test_app_icon.cpp)
  target_link_libraries(test_app_icon PRIVATE cliphist_lib)

  add_test(NAME test_dedup COMMAND test_dedup)
  add_test(NAME test_retention COMMAND test_retention)
  add_test(NAME test_repository COMMAND test_repository)
  add_test(NAME test_cli_list COMMAND test_cli_list)
  add_test(NAME test_command_parser COMMAND test_command_parser)
  add_test(NAME test_app_icon COMMAND test_app_icon)
endif()

set(CPACK_PACKAGE_NAME "cliphist")
set(CPACK_PACKAGE_VENDOR "cliphist")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Chinese desktop clipboard history manager")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "cliphist")

if(WIN32)
  set(CPACK_GENERATOR "ZIP;NSIS")
  set(CPACK_NSIS_DISPLAY_NAME "cliphist")
  set(CPACK_NSIS_PACKAGE_NAME "cliphist")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_MENU_LINKS "bin\\\\cliphist-gui.exe" "cliphist")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\cliphist-gui.exe")
  set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/cliphist.ico")
  set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
      "ExecWait '$SYSDIR\\\\taskkill.exe /IM cliphist-gui.exe /F /T'\n"
      "ExecWait '$SYSDIR\\\\taskkill.exe /IM cliphist.exe /F /T'")
  set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
      "ExecWait '$SYSDIR\\\\taskkill.exe /IM cliphist-gui.exe /F /T'\n"
      "ExecWait '$SYSDIR\\\\taskkill.exe /IM cliphist.exe /F /T'")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR "TGZ;DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "cliphist")
  if(CLIPHIST_ENABLE_X11)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsqlite3-0, libx11-6, libxfixes3, libxft2, libqt5widgets5, libqt5gui5, libqt5core5a")
  else()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsqlite3-0, libqt5widgets5, libqt5gui5, libqt5core5a")
  endif()
else()
  set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
