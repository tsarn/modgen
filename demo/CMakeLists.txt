find_package(Boost REQUIRED COMPONENTS headers program_options)
find_package(glfw3 REQUIRED)

modgen_define_module(
  # Name of the generated module + target
  NAME boost.program_options

  # Include file(s) that contain all the necessary names
  INCLUDES boost/program_options.hpp

  # This module will only include names from these namespaces
  NAMESPACES boost::program_options

  # This module depends on this target to provide it with include directories and compiler flags
  DEPENDS Boost::program_options
)

modgen_define_module(
  NAME boost.asio
  TARGET boost_asio_module # You can also specify a different target

  INCLUDES
    boost/asio.hpp
    boost/some_very_cool_asio_extensions.hpp # Doesn't exist

  NAMESPACES boost::asio

  DEPENDS Boost::headers # header-only

  NO_BUILD # Disable actually building the module, only generate the module file
  IGNORE_NONEXISTENT_INCLUDES # If an include is not actually there, ignore it
)

modgen_define_module(
  NAME glfw
  TARGET glfw_module

  INCLUDES
    vulkan/vulkan.h # GLFW conditionally includes vulkan-related functions
    GLFW/glfw3.h
  # Filter must match the fully-qualified name in full (not just a substring)
  # Fully-qualified names look like this: std::this_thread::sleep_for
  # Note that they don't include the leading ::
  FILTER "(glfw|GLFW).*"
  # Note: GLFW uses #defines to declare its constants
  # Modules cannot export them, and the module will not include them
)

# Instead of using DEPENDS in modgen_define_module, you can also
# link publically your dependencies to the target
target_link_libraries(glfw_module PUBLIC glfw)

# This is a shortcut to generate a module named "std", containing
# everything from the standard C++ library
modgen_enable_std(
  CXX_STANDARD 23
  DISABLE stacktrace # Disable <stacktrace> header
)