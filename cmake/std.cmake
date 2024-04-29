function(modgen_enable_std)
    cmake_parse_arguments(
            ARG
            ""
            "CXX_STANDARD;EXCLUDE"
            "DISABLE"
            ${ARGN}
    )

    set(STANDARD_INCLUDES "concepts;coroutine;any;bitset;chrono;compare;csetjmp;csignal;cstdarg;cstddef;cstdlib;ctime;debugging;expected;functional;initializer_list;optional;source_location;tuple;type_traits;typeindex;typeinfo;utility;variant;version;memory;memory_resource;new;scoped_allocator;cfloat;cinttypes;climits;cstdint;limits;stdfloat;cassert;cerrno;exception;stacktrace;stdexcept;system_error;cctype;charconv;cstring;cuchar;cwchar;cwctype;format;string;string_view;array;deque;flat_map;flat_set;forward_list;list;map;mdspan;queue;set;span;stack;unordered_map;unordered_set;vector;iterator;generator;ranges;algorithm;execution;bit;cfenv;cmath;complex;linalg;numbers;numeric;random;ratio;valarray;clocale;locale;text_encoding;cstdio;fstream;iomanip;ios;iosfwd;iostream;istream;ostream;print;spanstream;sstream;streambuf;syncstream;filesystem;regex;atomic;barrier;condition_variable;future;hazard_pointer;latch;mutex;rcu;semaphore;shared_mutex;stop_token;thread;assert.h;ctype.h;errno.h;fenv.h;float.h;inttypes.h;limits.h;locale.h;math.h;setjmp.h;signal.h;stdarg.h;stddef.h;stdint.h;stdio.h;stdlib.h;string.h;time.h;uchar.h;wchar.h;wctype.h;stdatomic.h;complex.h;tgmath.h;stdalign.h;stdbool.h")
    set(INCLUDES "")
    foreach(inc ${STANDARD_INCLUDES})
        if (inc IN_LIST ARG_DISABLE)
            continue()
        endif()
        list(APPEND INCLUDES "${inc}")
    endforeach()

    modgen_define_module(
            NAME std
            INCLUDES ${INCLUDES}
            NAMESPACES std
            EXCLUDE "${ARG_EXCLUDE}"
            IGNORE_NONEXISTENT_INCLUDES
    )
    if (NOT DEFINED ARG_CXX_STANDARD)
        if (DEFINED CMAKE_CXX_STANDARD)
            set(ARG_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
        else ()
            set(ARG_CXX_STANDARD 23)
        endif ()
    endif ()
    set_target_properties(std PROPERTIES CXX_STANDARD "${ARG_CXX_STANDARD}")
    target_compile_options(std PRIVATE -Wno-reserved-module-identifier)
endfunction()
