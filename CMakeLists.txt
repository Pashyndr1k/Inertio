cmake_minimum_required(VERSION 3.16)
project(InertiaCursor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
  # Only the motion tests build elsewhere.
  enable_testing()
  add_executable(spring_test tests/spring_test.cpp)
  target_include_directories(spring_test PRIVATE src)
  add_test(NAME spring COMMAND spring_test)
  return()
endif()

add_executable(InertiaCursor WIN32
  src/main.cpp
  src/overlay.cpp
  src/settings.cpp
  src/system_cursor.cpp
)
target_compile_definitions(InertiaCursor PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
target_link_libraries(InertiaCursor PRIVATE user32 gdi32 gdiplus shell32 shcore dwmapi advapi32 winmm)

if(MSVC)
  target_compile_options(InertiaCursor PRIVATE /W4 /utf-8)
  # One .exe with no Visual C++ runtime to install.
  set_property(TARGET InertiaCursor PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  target_compile_options(InertiaCursor PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
  target_link_options(InertiaCursor PRIVATE -municode -static)
endif()

enable_testing()
add_executable(spring_test tests/spring_test.cpp)
target_include_directories(spring_test PRIVATE src)
add_test(NAME spring COMMAND spring_test)

# Settings template and readme travel with the .exe.
add_custom_command(TARGET InertiaCursor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/inertia-cursor.ini $<TARGET_FILE_DIR:InertiaCursor>)
