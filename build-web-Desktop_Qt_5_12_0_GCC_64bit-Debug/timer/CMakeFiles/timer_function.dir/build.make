# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/ccpang/web

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug

# Include any dependencies generated for this target.
include timer/CMakeFiles/timer_function.dir/depend.make

# Include the progress variables for this target.
include timer/CMakeFiles/timer_function.dir/progress.make

# Include the compile flags for this target's objects.
include timer/CMakeFiles/timer_function.dir/flags.make

timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o: timer/CMakeFiles/timer_function.dir/flags.make
timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o: ../timer/lst_timer.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o"
	cd /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer && /usr/bin/g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/timer_function.dir/lst_timer.cpp.o -c /home/ccpang/web/timer/lst_timer.cpp

timer/CMakeFiles/timer_function.dir/lst_timer.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/timer_function.dir/lst_timer.cpp.i"
	cd /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ccpang/web/timer/lst_timer.cpp > CMakeFiles/timer_function.dir/lst_timer.cpp.i

timer/CMakeFiles/timer_function.dir/lst_timer.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/timer_function.dir/lst_timer.cpp.s"
	cd /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ccpang/web/timer/lst_timer.cpp -o CMakeFiles/timer_function.dir/lst_timer.cpp.s

timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.requires:

.PHONY : timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.requires

timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.provides: timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.requires
	$(MAKE) -f timer/CMakeFiles/timer_function.dir/build.make timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.provides.build
.PHONY : timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.provides

timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.provides.build: timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o


# Object files for target timer_function
timer_function_OBJECTS = \
"CMakeFiles/timer_function.dir/lst_timer.cpp.o"

# External object files for target timer_function
timer_function_EXTERNAL_OBJECTS =

timer/libtimer_function.a: timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o
timer/libtimer_function.a: timer/CMakeFiles/timer_function.dir/build.make
timer/libtimer_function.a: timer/CMakeFiles/timer_function.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libtimer_function.a"
	cd /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer && $(CMAKE_COMMAND) -P CMakeFiles/timer_function.dir/cmake_clean_target.cmake
	cd /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/timer_function.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
timer/CMakeFiles/timer_function.dir/build: timer/libtimer_function.a

.PHONY : timer/CMakeFiles/timer_function.dir/build

timer/CMakeFiles/timer_function.dir/requires: timer/CMakeFiles/timer_function.dir/lst_timer.cpp.o.requires

.PHONY : timer/CMakeFiles/timer_function.dir/requires

timer/CMakeFiles/timer_function.dir/clean:
	cd /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer && $(CMAKE_COMMAND) -P CMakeFiles/timer_function.dir/cmake_clean.cmake
.PHONY : timer/CMakeFiles/timer_function.dir/clean

timer/CMakeFiles/timer_function.dir/depend:
	cd /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ccpang/web /home/ccpang/web/timer /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer /home/ccpang/web/build-web-Desktop_Qt_5_12_0_GCC_64bit-Debug/timer/CMakeFiles/timer_function.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : timer/CMakeFiles/timer_function.dir/depend
