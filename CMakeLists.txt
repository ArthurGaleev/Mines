cmake_minimum_required(VERSION 3.28)
project(GitHubMessenger)

set(CMAKE_CXX_STANDARD 14)

add_executable(GitHubMessenger messenger.cpp)

include_directories(/usr/lib/git2)
link_directories(/usr/lib/x86_64-linux-gnu)
target_link_libraries(GitHubMessenger git2)
