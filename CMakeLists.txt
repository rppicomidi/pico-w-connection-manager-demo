cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_embedded_cli C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(MAKE_FS_DATA_SCRIPT ${PICO_SDK_PATH}/lib/lwip/src/apps/http/makefsdata/makefsdata)
message("Running makefsdata script")
execute_process(COMMAND
        perl ${MAKE_FS_DATA_SCRIPT}
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        )
file(RENAME fsdata.c my_fsdata.c)
set(target_name pico-w-connection-manager-demo)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pico-w-connection-manager)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/littlefs-lib)
add_executable(${target_name}
    main.cpp
    pico_lfs_cli.cpp
    pico_w_connection_manager_cli.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/parson/parson.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pico-c-utilities/getsn.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/embedded-cli/lib/src/embedded_cli.c
)

# To enable the USB console, set pico_enable_stdio_uart(${target_name} 0)
# & pico_enable_stdio_usb(${target_name} 1)
pico_enable_stdio_uart(${target_name} 1)
pico_enable_stdio_usb(${target_name} 0)

target_link_options(${target_name} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_name} PRIVATE -Wall -Wextra)

target_include_directories(${target_name} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/lib/embedded-cli/lib/include
    ${CMAKE_CURRENT_LIST_DIR}/lib/pico-c-utilities/
)

target_link_libraries(${target_name} pico_stdlib pico_w_connection_manager pico_lwip_http)
pico_add_extra_outputs(${target_name})

