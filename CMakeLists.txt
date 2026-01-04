cmake_minimum_required(VERSION 3.21)
project(stostatus_tray LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(stostatus_tray WIN32
  src/main.cpp
  src/App.cpp
  src/TrayIcon.cpp
  src/HttpClient.cpp
  src/StatusChecker.cpp
  src/resource.rc
 "src/crypto.h" "src/crypto.cpp")

target_include_directories(stostatus_tray PRIVATE src)

target_link_libraries(stostatus_tray PRIVATE
  winhttp
  shell32
  user32
)

# Optional: keep binary small-ish in Release
if (MSVC)
  target_compile_options(stostatus_tray PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
  target_compile_options(stostatus_tray PRIVATE -Wall -Wextra -Wpedantic)
endif()
