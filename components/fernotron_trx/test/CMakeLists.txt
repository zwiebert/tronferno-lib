idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "." ../include 
                       PRIV_INCLUDE_DIRS ../include/fernotron ../priv_include
                       PRIV_REQUIRES  utils_debug
                       REQUIRES unity fernotron_trx)
