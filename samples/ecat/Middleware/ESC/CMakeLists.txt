target_sources(app PRIVATE hpm_ecat_e2p_emulation.c hpm_ecat_hw.c hpm5e31_phy.c)

# The SDK component's standalone CMakeLists uses sdk_src/sdk_inc and adds a
# bare-metal NOR port. Use its core with our Zephyr flash callbacks instead.
target_sources(app PRIVATE
    ${HPM_SDK_DIR}/components/eeprom_emulation/eeprom_emulation.c
    ${HPM_SDK_DIR}/utils/hpm_crc32.c
)
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HPM_SDK_DIR}/components/eeprom_emulation
    ${HPM_SDK_DIR}/components/eeprom_emulation/port
    ${HPM_SDK_DIR}/utils
)
