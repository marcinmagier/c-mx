
#
#   Test build
#

include(build_utils)
include(build_flags_sections)


set(PRJ_LIB_NAME        cmx_lib)
set(PRJ_APP_NAME        cmx_test)
set(PRJ_APP_OUT_NAME    cmx_test)


# find external libs
find_library(EXT_LIB_JSON_PATH          "json-c")
find_library(EXT_LIB_SSL_PATH           "ssl")
find_library(EXT_LIB_CRYPTO_PATH        "crypto")
find_library(EXT_LIB_DL_PATH            "dl")
find_library(EXT_LIB_CUNIT_PATH         "cunit")


# add subdirectories
add_subdirectory("include")
add_subdirectory("source")
add_subdirectory("test/cunit")




# ------------------------------------------------------------------------------

# Build executable

# retrieve includes
get_includes(PRJ_APP_INCLUDES   "${PRJ_LIB_NAME}" "${PRJ_APP_NAME}")
# retrieve defines
get_defines(PRJ_APP_DEFINES     "${PRJ_LIB_NAME}" "${PRJ_APP_NAME}")
# retrieve cflags
get_cflags(PRJ_APP_CFLAGS       "${PRJ_LIB_NAME}" "${PRJ_APP_NAME}")
# retrieve sources
get_sources(PRJ_APP_SOURCES     "${PRJ_LIB_NAME}" "${PRJ_APP_NAME}")



# target
add_executable(${PRJ_APP_NAME} ${PRJ_APP_SOURCES})
set_target_properties(${PRJ_APP_NAME} PROPERTIES
    COMPILE_FLAGS           "${PRJ_APP_CFLAGS}"
    COMPILE_DEFINITIONS     "${PRJ_APP_DEFINES}"
    OUTPUT_NAME             "${PRJ_APP_OUT_NAME}"
)
target_include_directories(${PRJ_APP_NAME} PRIVATE ${PRJ_APP_INCLUDES})

# private defines
set_private_defines(${PRJ_LIB_NAME})
set_private_defines(${PRJ_APP_NAME})

# link libraries
target_link_libraries(${PRJ_APP_NAME} ${EXT_LIB_JSON_PATH})
target_link_libraries(${PRJ_APP_NAME} ${EXT_LIB_SSL_PATH})
target_link_libraries(${PRJ_APP_NAME} ${EXT_LIB_CRYPTO_PATH})
target_link_libraries(${PRJ_APP_NAME} ${EXT_LIB_DL_PATH})
target_link_libraries(${PRJ_APP_NAME} ${EXT_LIB_CUNIT_PATH})


# install
install(TARGETS  ${PRJ_APP_NAME}        DESTINATION "bin")

