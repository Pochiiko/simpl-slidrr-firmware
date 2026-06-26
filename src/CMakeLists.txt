pico_sdk_init()

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Werror -Wfatal-errors -O3)
link_libraries(pico_stdlib hardware_i2c hardware_adc
                hardware_flash hardware_watchdog
                tinyusb_device tinyusb_board)

function(make_firmware board board_def)
        add_executable(${board}
        main.c slider.c air.c button.c save.c config.c commands.c
        cli.c mpr121.c usb_descriptors.c sega_serial_reader.c sega_slider.c
        protocol.c)
    target_compile_definitions(${board} PUBLIC ${board_def})
    pico_enable_stdio_usb(${board} 1)
    
    target_link_libraries(${board} PRIVATE aic)

    pico_add_extra_outputs(${board})

    add_custom_command(TARGET ${board} PRE_BUILD
    COMMAND touch ${CMAKE_CURRENT_SOURCE_DIR}/cli.c)

    add_custom_command(TARGET ${board} POST_BUILD
                       COMMAND cp ${board}.uf2 ${CMAKE_CURRENT_LIST_DIR}/..)
endfunction()

make_firmware(simpl-slidrr-firmware BOARD_SIMPL_SLIDRR)
