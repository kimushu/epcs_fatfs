#
# epcs_fatfs - Copyright (C) 2016 @kimu_shu
# FatFs - (C)ChaN, 2015
#

create_sw_package epcs_fatfs

set_sw_property version 1.0

set_sw_property auto_initialize true
set_sw_property bsp_subdirectory drivers

# Source files
add_sw_property c_source fatfs/src/ff.c
add_sw_property c_source src/epcs_fatfs.c
add_sw_property c_source src/epcs_fatfs_diskio.c
add_sw_property include_source fatfs/src/diskio.h
add_sw_property include_source fatfs/src/ff.h
add_sw_property include_source fatfs/src/integer.h
add_sw_property include_source inc/epcs_fatfs.h
add_sw_property include_source inc/ffconf.h
add_sw_property include_directory fatfs/src

# # Supported BSP types
add_sw_property supported_bsp_type HAL
add_sw_property supported_bsp_type UCOSII
add_sw_property supported_bsp_type TINYTH

# Settings

add_sw_setting quoted_string system_h_define mount_point EPCS_FATFS_MOUNT_POINT /mnt/epcs "Mount point"
add_sw_setting unquoted_string system_h_define epcs.base_address EPCS_FATFS_EPCS_BASE 0 "Base address of EPCS control port"
add_sw_setting boolean_define_only system_h_define epcs.use_swi EPCS_FATFS_EPCS_USE_SWI 0 "Use SWI (v1.1) interface"
add_sw_setting boolean_define_only system_h_define epcs.use_spi EPCS_FATFS_EPCS_USE_SPI 0 "Use SPI interface"
add_sw_setting decimal_number system_h_define epcs.spi_slavenumber EPCS_FATFS_EPCS_SPI_SLAVE 0 "Slave number for SPI interface"
add_sw_setting decimal_number system_h_define flash.sector_size EPCS_FATFS_FLASH_SECTOR 4096 "Sector size in bytes"
add_sw_setting hex_number system_h_define flash.start_address EPCS_FATFS_FLASH_START 0x0 "Start address of flash"
add_sw_setting hex_number system_h_define flash.end_address EPCS_FATFS_FLASH_END 0x0 "End address of flash (0=auto detect)"
add_sw_setting hex_number system_h_define flash.sector_erase_cmd EPCS_FATFS_FLASH_CMD_ERASE 0x20 "Command for sector erase"
add_sw_setting boolean_define_only system_h_define flash.enable_verify EPCS_FATFS_FLASH_VERIFY 0 "Verify after writing sector"
# add_sw_setting boolean_define_only system_h_define epcs.omit_erase EPCS_FATFS_FLASH_OMIT_ERASE 0 "Omit erase if erase is not required"

# End of file
