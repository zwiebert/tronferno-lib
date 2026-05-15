set(srcs   fer_uo_publish.cc so_write_msg.cc
    )
set(platform_srcs "") 
include("${CMAKE_CURRENT_LIST_DIR}/../../source_filter.cmake" OPTIONAL)

idf_component_register(
    SRCS ${srcs} 
    INCLUDE_DIRS "./include" 
    REQUIRES utils_misc
    PRIV_REQUIRES  utils_debug  uout fernotron_trx fernotron_app
     )

 
target_compile_options(${COMPONENT_LIB} PRIVATE "SHELL:-include sdkconfig.h")
