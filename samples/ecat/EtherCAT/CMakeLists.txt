target_sources(app PRIVATE
    ssc/ecatslv.c
    ssc/ecatcoe.c
    ssc/mailbox.c
    ssc/sdoserv.c
    ssc/coeappl.c
    ssc/objdef.c
    ssc/ecatappl.c
    bsp/ecat_zephyr_port.c
    app_EtherCAT/app_ethercat.c
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ssc
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
)
