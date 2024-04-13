set (libesphttpd_SOURCES "core/auth.c"
                         "core/httpd-freertos.c"
                         "core/httpd.c"
                         "core/sha1.c"
                         "core/base64.c"
                         "util/captdns.c"
                         "util/cgiflash.c"
                         "util/cgiredirect.c"
                         "util/cgiwebsocket.c"
                         "util/cgiwifi.c"
                         "util/cgiredirect.c"
                         "util/esp32_flash.c"
                         "util/esp32_httpd_vfs.c"
                         "espfs/espfs.c"
                         "espfs/heatshrink_decoder.c"
                         )

set (libesphttpd_PRIV_INCLUDE_DIRS "core"
                                   "util" "espfs")

if (CONFIG_ESPHTTPD_USE_ESPFS)
    list (APPEND libesphttpd_SOURCES "core/httpd-espfs.c" "${CMAKE_CURRENT_BINARY_DIR}/src/espfs_image.c")
    list (APPEND libesphttpd_PRIV_INCLUDE_DIRS "../espfs/include")
endif (CONFIG_ESPHTTPD_USE_ESPFS)

idf_component_register(
    SRCS "${libesphttpd_SOURCES}"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${libesphttpd_PRIV_INCLUDE_DIRS}"
    REQUIRES "app_update"
             "json"
             "spi_flash"
             "wpa_supplicant"
)

target_compile_definitions (${COMPONENT_TARGET} PUBLIC -DFREERTOS)

idf_build_get_property(python PYTHON)

add_custom_command(
    OUTPUT requirements.stamp
    COMMAND "${python}" -m pip install -r "${COMPONENT_DIR}/requirements.txt"
    COMMAND ${CMAKE_COMMAND} -E touch requirements.stamp
)

set(CONFIG_ESPFS_IMAGEROOTDIR "espfs")
set(espfs_image_bin_DEPS "")
if (EXISTS "${PROJECT_DIR}/espfs.yaml")
    list(APPEND "${PROJECT_DIR}/espfs.yaml")
endif()

add_custom_command(
    OUTPUT espfs_image.bin
    COMMAND ${CMAKE_COMMAND} -E env PROJECT_DIR=${PROJECT_DIR} "${python}" "${COMPONENT_DIR}/tools/mkespfsimage.py" "${PROJECT_DIR}/${CONFIG_ESPFS_IMAGEROOTDIR}" espfs_image.bin
    DEPENDS "${PROJECT_DIR}/${CONFIG_ESPFS_IMAGEROOTDIR}" requirements.stamp ${espfs_image_bin_DEPS}
    COMMENT "Building espfs image"
    USES_TERMINAL
    VERBATIM
)

add_custom_command(
    OUTPUT src/espfs_image.c
    COMMAND "${python}" "${COMPONENT_DIR}/tools/bin2c.py" espfs_image.bin src/espfs_image.c
    DEPENDS espfs_image.bin
    COMMENT "Generating espfs image C source"
    VERBATIM
)
