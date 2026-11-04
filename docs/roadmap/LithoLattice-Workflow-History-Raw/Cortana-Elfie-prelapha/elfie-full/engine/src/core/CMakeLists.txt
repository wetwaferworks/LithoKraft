add_library(litholattice_core STATIC json.cpp log.cpp timer_resolution.cpp process_memory.cpp)
target_include_directories(litholattice_core PUBLIC include)
find_package(Threads REQUIRED)
target_link_libraries(litholattice_core PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(litholattice_core PRIVATE winmm psapi)  # timeBeginPeriod, GetProcessMemoryInfo
  target_compile_definitions(litholattice_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
litholattice_module_options(litholattice_core)
