# C++20 module generator

This tool allows you to convert your old header-based C++ libraries into modules.

## Prerequisites

 - CMake 3.28+
 - Clang 17+

Clang is used to parse your library's source files.

## Usage with CMake

In your `CMakeLists.txt` file, add this project as a subdirectory
using `add_subdirectory(path to modgen repo)`.

You will need to use Clang as your C++ compiler in CMake.
To do this, pass `-DCMAKE_CXX_COMPILER=clang++` to CMake when generating your build

To define a module, use the following function:

```cmake
modgen_define_module(
  NAME <your module name>
  INCLUDES <include> [include]...
  [TARGET <target>]
  [NAMESPACES <namespace> [namespace]...]
  [DEPENDS <target> [target]...]
  [FILTER <filter>]
  [EXCLUDE <exclude>]
)
```

Please take a look at the [examples](https://github.com/tsarn/modgen/blob/main/demo/CMakeLists.txt).

In most cases, you only need to do something like this:

```cmake
find_package(glm REQUIRED)
modgen_define_module(
  NAME glm
  INCLUDES
    glm/glm.hpp
    glm/ext.hpp
  NAMESPACES glm
  DEPENDS glm::glm
)
```

Then you can use this module as a dependency anywhere you want with `target_link_libraries`.

You will find all your generated modules in `${CMAKE_BINARY_DIR}/modgen_generated_modules`.
They can all be built by using the target `modgen_all_modules`.
This is useful if you don't really care about build system integration, and just want the module
files to tinker on them further.

## Usage without CMake

The program `modgen_ast_parser` parses Clang binary AST and outputs the export declarations,
appropriate to use in a module. It depends on the `libclang` library, which should be
distributed together will Clang.

To produce an AST file, appropriate for this command, pass the `-emit-ast` flag to Clang
when compiling. The AST file should include all definitions that will be exported.
The way to achieve that, is to create a C++ source file, containing all the library's
includes, and then generate AST from this file, passing all necessary compiler flags.

```
Usage: modgen_ast_parser [options] <ast file>

Options:
  -o <path>              Write output to the specified file
  -n <namespaces>        Specify the comma separated list of namespaces to export
  -f <regex>             Only export the names matching this regex
  -e <regex>             Do not export the names matching this regex
  -p                     Instead of generating a module, simply output the list of names
```