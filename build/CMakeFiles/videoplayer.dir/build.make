# CMAKE generated file: DO NOT EDIT!
# Generated by "MinGW Makefiles" Generator, CMake Version 3.30

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

SHELL = cmd.exe

# The CMake executable.
CMAKE_COMMAND = "C:\Program Files\CMake\bin\cmake.exe"

# The command to remove a file.
RM = "C:\Program Files\CMake\bin\cmake.exe" -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\build

# Include any dependencies generated for this target.
include CMakeFiles/videoplayer.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/videoplayer.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/videoplayer.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/videoplayer.dir/flags.make

CMakeFiles/videoplayer.dir/test.c.obj: CMakeFiles/videoplayer.dir/flags.make
CMakeFiles/videoplayer.dir/test.c.obj: CMakeFiles/videoplayer.dir/includes_C.rsp
CMakeFiles/videoplayer.dir/test.c.obj: C:/Users/David/Documents/DevProjects/msys64/home/David/Workspace/VideoPlayer/test.c
CMakeFiles/videoplayer.dir/test.c.obj: CMakeFiles/videoplayer.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/videoplayer.dir/test.c.obj"
	C:\Users\David\Documents\DevProjects\msys64\mingw64\bin\gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/videoplayer.dir/test.c.obj -MF CMakeFiles\videoplayer.dir\test.c.obj.d -o CMakeFiles\videoplayer.dir\test.c.obj -c C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\test.c

CMakeFiles/videoplayer.dir/test.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing C source to CMakeFiles/videoplayer.dir/test.c.i"
	C:\Users\David\Documents\DevProjects\msys64\mingw64\bin\gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\test.c > CMakeFiles\videoplayer.dir\test.c.i

CMakeFiles/videoplayer.dir/test.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling C source to assembly CMakeFiles/videoplayer.dir/test.c.s"
	C:\Users\David\Documents\DevProjects\msys64\mingw64\bin\gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\test.c -o CMakeFiles\videoplayer.dir\test.c.s

# Object files for target videoplayer
videoplayer_OBJECTS = \
"CMakeFiles/videoplayer.dir/test.c.obj"

# External object files for target videoplayer
videoplayer_EXTERNAL_OBJECTS =

videoplayer.exe: CMakeFiles/videoplayer.dir/test.c.obj
videoplayer.exe: CMakeFiles/videoplayer.dir/build.make
videoplayer.exe: C:/Users/David/Documents/DevProjects/msys64/mingw64/lib/libavcodec.dll.a
videoplayer.exe: C:/Users/David/Documents/DevProjects/msys64/mingw64/lib/libavformat.dll.a
videoplayer.exe: C:/Users/David/Documents/DevProjects/msys64/mingw64/lib/libavutil.dll.a
videoplayer.exe: C:/Users/David/Documents/DevProjects/msys64/mingw64/lib/libavdevice.dll.a
videoplayer.exe: C:/Users/David/Documents/DevProjects/msys64/mingw64/lib/libswscale.dll.a
videoplayer.exe: C:/Users/David/Documents/DevProjects/msys64/mingw64/lib/libportaudio.dll.a
videoplayer.exe: CMakeFiles/videoplayer.dir/linkLibs.rsp
videoplayer.exe: CMakeFiles/videoplayer.dir/objects1.rsp
videoplayer.exe: CMakeFiles/videoplayer.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable videoplayer.exe"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles\videoplayer.dir\link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/videoplayer.dir/build: videoplayer.exe
.PHONY : CMakeFiles/videoplayer.dir/build

CMakeFiles/videoplayer.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles\videoplayer.dir\cmake_clean.cmake
.PHONY : CMakeFiles/videoplayer.dir/clean

CMakeFiles/videoplayer.dir/depend:
	$(CMAKE_COMMAND) -E cmake_depends "MinGW Makefiles" C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\build C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\build C:\Users\David\Documents\DevProjects\msys64\home\David\Workspace\VideoPlayer\build\CMakeFiles\videoplayer.dir\DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/videoplayer.dir/depend

