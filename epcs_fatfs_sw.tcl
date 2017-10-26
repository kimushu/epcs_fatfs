#
# epcs_fatfs - Copyright (C) 2016 @kimu_shu
# FatFs - (C)ChaN, 2015
#

create_sw_package epcs_fatfs

set_sw_property version 1.2

set_sw_property auto_initialize true
set_sw_property bsp_subdirectory drivers

# Source files
add_sw_property c_source fatfs/src/ff.c
add_sw_property c_source fatfs/src/option/unicode.c
add_sw_property c_source src/epcs_fatfs.c
add_sw_property c_source src/epcs_fatfs_diskio.c
add_sw_property include_source fatfs/src/diskio.h
add_sw_property include_source fatfs/src/ff.h
add_sw_property include_source fatfs/src/integer.h
add_sw_property include_source fatfs/src/option/cc932.h
add_sw_property include_source fatfs/src/option/cc936.h
add_sw_property include_source fatfs/src/option/cc949.h
add_sw_property include_source fatfs/src/option/cc950.h
add_sw_property include_source fatfs/src/option/ccsbcs.h
add_sw_property include_source inc/epcs_fatfs.h
add_sw_property include_source inc/ffconf.h
add_sw_property include_directory fatfs/src

# # Supported BSP types
add_sw_property supported_bsp_type HAL
add_sw_property supported_bsp_type UCOSII
add_sw_property supported_bsp_type TINYTH

# Settings

add_sw_setting quoted_string system_h_define mount_point EPCS_FATFS_MOUNT_POINT /mnt/epcs "Mount point"
add_sw_setting boolean_define_only system_h_define interface.use_peridot_spi_flash EPCS_FATFS_IF_PERIDOT_SPI_FLASH 0 "Use SPI flash interface (peridot_spi_master_driver)"
add_sw_setting boolean_define_only system_h_define interface.use_altera_spi EPCS_FATFS_IF_ALTERA_SPI 0 "Use SPI interface (altera_avalon_spi_driver)"
add_sw_setting boolean_define_only system_h_define interface.use_altera_epcs EPCS_FATFS_IF_ALTERA_EPCS 0 "Use EPCS interface (altera_avalon_epcs_flash_controller_driver)"
add_sw_setting unquoted_string system_h_define interface.base_address EPCS_FATFS_IF_INST_NAME 0 "Instance name (SPI and EPCS interface only)"
add_sw_setting decimal_number system_h_define interface.spi_slavenumber EPCS_FATFS_IF_SPI_SLAVE 0 "Slave number (SPI interface only)"
add_sw_setting decimal_number system_h_define flash.sector_size EPCS_FATFS_FLASH_SECTOR 4096 "Sector size in bytes"
add_sw_setting hex_number system_h_define flash.start_address EPCS_FATFS_FLASH_START 0x0 "Start address of flash"
add_sw_setting hex_number system_h_define flash.end_address EPCS_FATFS_FLASH_END 0x0 "End address of flash (0=auto detect)"
add_sw_setting hex_number system_h_define flash.sector_erase_cmd EPCS_FATFS_FLASH_CMD_ERASE 0x20 "Command for sector erase"
add_sw_setting boolean_define_only system_h_define flash.enable_verify EPCS_FATFS_FLASH_VERIFY 0 "Verify after writing sector"
add_sw_setting boolean system_h_define fatfs.use_lfn EPCS_FATFS_USE_LFN 0 "Use LFN feature"
add_sw_setting decimal_number system_h_define fatfs.code_page EPCS_FATFS_CODE_PAGE 1 "Code page (1:ASCII, 437:US, 932:Japanese, etc)"
# add_sw_setting boolean_define_only system_h_define epcs.omit_erase EPCS_FATFS_FLASH_OMIT_ERASE 0 "Omit erase if erase is not required"

# End of file
