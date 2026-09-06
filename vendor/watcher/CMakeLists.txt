project(
  wtr.watcher
  VERSION 0.14.5 # hook: tool/release
  DESCRIPTION "watcher: a filesystem watcher"
  HOMEPAGE_URL "github.com/e-dant/watcher"
  LANGUAGES
    CXX
    C
)

add_library(wtr INTERFACE)
target_include_directories(wtr INTERFACE
    ./include/
)

install(DIRECTORY include/ DESTINATION include)
