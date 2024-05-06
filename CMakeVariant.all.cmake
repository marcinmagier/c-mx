
#
#   Normal build
#

include(build_utils)
include(build_flags_sections)


set(PRJ_LIB_NAME        cmx_lib)
set(PRJ_LIB_OUT_NAME    mx)

set(PRJ_LIB_VER_MAJOR   0)
set(PRJ_LIB_VER_MINOR   1)
set(PRJ_LIB_VER_PATCH   0)


# find external libs


# add subdirectories
add_subdirectory("include")
add_subdirectory("source")




# ------------------------------------------------------------------------------

# Build library

# retrieve includes
get_includes(PRJ_LIB_INCLUDES       "${PRJ_LIB_NAME}")
# retrieve defines
get_defines(PRJ_LIB_DEFINES         "${PRJ_LIB_NAME}")
# retrieve cflags
get_cflags(PRJ_LIB_CFLAGS           "${PRJ_LIB_NAME}")
# retrieve sources
get_sources(PRJ_LIB_SOURCES         "${PRJ_LIB_NAME}")
# retrieve headers
get_headers(PRJ_LIB_HEADERS         "${PRJ_LIB_NAME}")


# objects
add_library(${PRJ_LIB_NAME}_objs OBJECT ${PRJ_LIB_SOURCES})
set_target_properties(${PRJ_LIB_NAME}_objs PROPERTIES
    COMPILE_FLAGS           "${PRJ_LIB_CFLAGS}"
    COMPILE_DEFINITIONS     "${PRJ_LIB_DEFINES}"
)
target_include_directories(${PRJ_LIB_NAME}_objs PRIVATE ${PRJ_LIB_INCLUDES})

# static lib
add_library(${PRJ_LIB_NAME}_static STATIC $<TARGET_OBJECTS:${PRJ_LIB_NAME}_objs>)
set_target_properties(${PRJ_LIB_NAME}_static PROPERTIES
    OUTPUT_NAME             "${PRJ_LIB_OUT_NAME}"
    CLEAN_DIRECT_OUTPUT     1
)

# shared lib
add_library(${PRJ_LIB_NAME}_shared SHARED $<TARGET_OBJECTS:${PRJ_LIB_NAME}_objs>)
set_target_properties(${PRJ_LIB_NAME}_shared PROPERTIES
    OUTPUT_NAME             "${PRJ_LIB_OUT_NAME}"
    CLEAN_DIRECT_OUTPUT     1
)
set_target_properties(${PRJ_LIB_NAME}_shared PROPERTIES
    VERSION                 ${PRJ_LIB_VER_MAJOR}.${PRJ_LIB_VER_MINOR}.${PRJ_LIB_VER_PATCH}
    SOVERSION               ${PRJ_LIB_VER_MAJOR}
)

# private defines
set_private_defines(${PRJ_LIB_NAME})

# link libraries

# install
install(TARGETS ${PRJ_LIB_NAME}_static  DESTINATION "lib")
install(TARGETS ${PRJ_LIB_NAME}_shared  DESTINATION "lib")
install(FILES   ${PRJ_LIB_HEADERS}      DESTINATION "include/mx")

