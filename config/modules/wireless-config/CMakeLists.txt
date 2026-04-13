# Wireless Config Module for minimal-keys

if(CONFIG_WIRELESS_CONFIG)
    zephyr_library_named(wireless_config)

    zephyr_library_include_directories(include)

    zephyr_library_sources(
        src/wireless_config.c
        src/ble_service.c
        src/config_handler.c
    )

    if(CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS)
        zephyr_library_sources(src/settings_storage.c)
    endif()

endif()
