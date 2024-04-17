idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "." ../include 
                       PRIV_INCLUDE_DIRS ../include/fernotron
                       PRIV_REQUIRES  utils_debug
                       REQUIRES unity txtio fernotron_trx)
