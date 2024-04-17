set(CMAKE_CXX_STANDARD 23)

modgen_generate(
    std
    STD_INCLUDES
    NAMESPACES std
)

modgen_generate(
    glm
    INCLUDE
        glm/glm.hpp
        glm/ext.hpp
    NAMESPACES glm
)
