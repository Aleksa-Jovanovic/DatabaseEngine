# CMake Tutorial

## What CMake is doing in this project

CMake is not the compiler itself. It is a build system generator.

In this project, CMake:
- reads the build rules from `CMakeLists.txt`
- generates build files for your machine and compiler
- builds object files, libraries, and executables from those rules

The flow is:

1. configure the project
2. generate build files into the `build/` folder
3. build the targets
4. run the executable

## Current commands

From the project root:

```powershell
cmake -S . -B build
cmake --build build
.\build\Debug\DatabaseEngine.exe
```

If the project is already configured and you only changed code, usually this is enough:

```powershell
cmake --build build
.\build\Debug\DatabaseEngine.exe
```

## What the current CMake file means

Current `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)

project(DatabaseEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(db_storage
    src/storage/disk/disk_manager.cpp
    src/storage/heap/heap_file.cpp
)

target_include_directories(db_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(DatabaseEngine
    src/main.cpp
)

target_link_libraries(DatabaseEngine
    PRIVATE
        db_storage
)
```

## Line-by-line explanation

### `cmake_minimum_required(VERSION 3.16)`

This says the project expects at least CMake 3.16.

### `project(DatabaseEngine LANGUAGES CXX)`

This defines the project name and says the project uses C++.

### `set(CMAKE_CXX_STANDARD 17)`

This tells CMake to compile the project with C++17.

### `set(CMAKE_CXX_STANDARD_REQUIRED ON)`

This makes C++17 required instead of optional.

### `set(CMAKE_CXX_EXTENSIONS OFF)`

This asks for standard C++ mode, not compiler-specific language extensions.

### `add_library(db_storage ...)`

This creates a library target named `db_storage`.

Right now it contains:
- `src/storage/disk/disk_manager.cpp`
- `src/storage/heap/heap_file.cpp`

CMake compiles these source files into object files and then produces a library from them.

### `target_include_directories(db_storage PUBLIC .../include)`

This tells the compiler where the header files are.

`PUBLIC` means:
- `db_storage` itself uses this include path
- anything that links against `db_storage` also gets this include path

That is why `main.cpp` can include:

```cpp
#include "storage/heap/heap_file.h"
```

without manually repeating the include directory on the executable target.

### `add_executable(DatabaseEngine src/main.cpp)`

This creates the runnable program target named `DatabaseEngine`.

Right now its entry point is:
- `src/main.cpp`

### `target_link_libraries(DatabaseEngine PRIVATE db_storage)`

This links the executable with the `db_storage` library.

`PRIVATE` means the executable uses `db_storage` internally, but does not pass that dependency further to anything else.

## Why use a library target

The storage code is placed in `db_storage` instead of compiling everything directly into `main.cpp`.

That is useful because:
- storage code is grouped as one module
- tests can later link to `db_storage`
- `main.cpp` stays small
- the project can grow into multiple libraries later

Possible later targets:
- `db_storage`
- `db_index`
- `db_sql`
- `db_execution`

## What gets produced

With the current Visual Studio generator, the build creates files like:
- `build/Debug/db_storage.lib`
- `build/Debug/DatabaseEngine.exe`

So yes, object files are being built under the hood first, then combined into the library and executable.

## What `cmake -S . -B build` means

- `-S .` means the source directory is the current folder
- `-B build` means generated build files go into the `build/` folder

This keeps generated files separate from your source code.

## What `cmake --build build` means

This tells CMake to build the already-configured project in the `build/` folder.

You do not need to rerun full configuration every time unless:
- `CMakeLists.txt` changed
- the generator changed
- the build directory was deleted

## Running the program

After build, run:

```powershell
.\build\Debug\DatabaseEngine.exe
```

Your current `main.cpp` creates:

```cpp
db::HeapFile heap("test.db");
```

so the database file `test.db` is created in the project root when you run the program.

## When to update `CMakeLists.txt`

Update it when:
- you add a new `.cpp` file that belongs to a target
- you add a new executable
- you add tests
- you split code into more libraries
- you add external dependencies later

If you only change existing `.cpp` or `.h` code, you usually do not need to edit `CMakeLists.txt`.

## Current limitation

The current `CMakeLists.txt` only includes the code that is implemented and buildable right now.

That is intentional.

There are other source files in the repo, but they are still placeholders or unfinished modules. Adding them to the build too early would create compile or link errors.

## Good next steps later

As the project grows, you can improve the build by:
- grouping source files into variables like `STORAGE_SOURCES`
- adding a `tests` executable
- adding separate targets for index, SQL, and execution layers
- adding compiler warnings
- adding CTest for automated tests
