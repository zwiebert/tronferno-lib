set(srcs   astro.cc fer_api.cc fer_msg_basic.cc fer_msg_rx.cc fer_msg_tx.cc fer_msg_tx_queue.cc
     fer_radio_parity.cc fer_rawmsg_buffer.cc fer_rawmsg_build.cc
    fer_trx_api.cc timer_data.cc
    repeater/repeater.cc
    )
set(platform_srcs "") 
include("${CMAKE_CURRENT_LIST_DIR}/../../source_filter.cmake" OPTIONAL)

if(CONFIG_APP_USE_FER_TRANSMITTER)
  set(srcs ${srcs} fer_radio_tx.cc)
endif()

if(CONFIG_APP_USE_FER_RECEIVER)
  set(srcs ${srcs} fer_radio_rx.cc)
endif()
if(CONFIG_TRONFERNO_ENABLE_RF_CAPTURE)
  set(srcs ${srcs} rf_capture.cc)
endif()
idf_component_register(
    SRCS ${srcs} 
    INCLUDE_DIRS "include" 
    PRIV_INCLUDE_DIRS "priv_include" 
    REQUIRES utils_misc utils_debug 
    PRIV_REQUIRES main_loop utils_time utils_misc cc1101_ook 
     )

 
target_compile_options(${COMPONENT_LIB} PRIVATE "SHELL:-include sdkconfig.h" -Wno-error=missing-field-initializers)
