# pico-w-connection-manager-demo
Pico W C++ class and demo CLI program to scan for SSID, handle
connect and disconnect, read RSSI, and store SSID info in flash.

The source code is designed so you can use the `Pico_w_connection_manager` Class
with your own user interface. Use the `Pico_w_connection_manager_cli` Class
as a guide. The code assumes a "super-loop" and does not use an RTOS.

Note that all this code does is manage the connection. It does not attempt to launch a Web server, fetch info from the Internet, etc.

# Demo program
The demo program uses a serial port console to accept user input and
print output. Type `help` at the command prompt for a list of commands.
All commands for managing the Wi-Fi connection are of the form `wifi-`.
The other commands are for managing the LittleFs flash file system.

The demo program uses a UART serial port console. You will need to modify
`CMakeLists.txt` if you wish to use the Pico-W's microUSB port as the
serial port console. See the comments in `CMakeLists.txt`.

# Dependencies
Aside from the dependencies on the Pico C/C++ SDK, the Pico_w_connection_manager class
uses the following external code:

- the `parson` JSON library to serialize and deserialize settings to JSON format
- the `LittleFs` file system to store Wi-Fi settings in JSON format to
a small reserved amount of Pico board program flash.
- the `LwIP` library for a TCP/IP stack.

The demo program uses the following external code:
- the `EmbeddedCli` library for the main CLI implementation
- a `getsn()` implementation for user input of numbers and strings in response to prompts

See the source code `ext_lib` and `lib` directories for more details

# Getting the Source And Building the demo program
Make sure you have installed the `pico-sdk` and that it works.
Execute the following commands. The commands below assume that
the `pico-sdk` is stored in `${PICO_SDK_PATH}` and you want
the `pico-w-connection-manager-demo` directory at the same
directory level as the `pico-sdk`.

```
export PICO_BOARD=pico_w
cd ${PICO_SDK_PATH}/..
git clone https://github.com/rppicomidi/pico-w-connection-manager-demo.git
cd pico-w-connection-manager-demo
git submodule update --recursive --init
mkdir build
cd build
cmake ..
make
```

Load the built image into your Pico-W. If if you run into issues during testing, apply any patches described in the [Known Issues](#known-issues) section and rebuild.

# Known Issues
For all known issues, check the date. By the time you build this, they
may be fixed.
## On 6-dec-2022
If you call `initialize()` after you call `deinitialize()` then the software will hang up.
This is an issue the `pico-sdk`. To work around this issue, use the
`pico-sdk` `development` branch and patch per the discussion in [sdk issue #980](https://github.com/raspberrypi/pico-sdk/issues/980):

```
cd ${PICO_SDK_PATH}
git fetch origin
git checkout -b develop origin/develop
git submodule update lib/cyw43-driver/
```

Edit the file 'pico-sdk/src/rp2_common/pico_cyw43_arch/cyw43_arch_threadsafe_background.c`. 
Replace code near line 194

```
#if CYW43_LWIP
    lwip_init();
#endif
```

with

```
#if CYW43_LWIP
    static bool done_lwip_init;
    if (!done_lwip_init) {
        lwip_init();
        done_lwip_init = true;
    }
#endif
```

# Demo program features
The demo prgram is designed to exercise features of the
`pico-w-connection-manager` class, which is called "the class" below.
## Hardware initialization
1. The User chooses Wi-Fi radio bands based on a list of supported countries that the class can supply to a user interface
2. The class initializes the Wi-Fi system with the user-selected
country code.

Commands: `wifi-country` and `wifi-initialize`.

## Hardware shutdown
1. The User requests Wi-Fi shutdown
2. The class de-initializes the Wi-Fi system.

Commands: `wifi-deinitialize`

## User-directed scan and connect workflow
1. The user recalls the last connected AP SSID info from flash and initializes Wi-Fi if required.
2. User starts a scan for available Wi-Fi access points
3. User chooses the SSID to connect from the scan list and supplies a
passphrase if required
4. The class stores the selected SSID, user-entered passphrase, and
scan-obtained security type to flash in JSON format
5. The class attempts to connect to the access point using the SSID info stored.
6. If connection succeeds, the class stores the SSID info to a list of known APs

Commands `wifi-scan-connect`

## User-directed hidden SSID connect workflow
1. The user recalls the last connected AP SSID info from flash and initializes Wi-Fi if required.
2. User enters SSID, passphrase and security type (Open, WPA-PSK, WPA2-PSK) for the AP.
3. The class stores the SSID, passphrase, and security type to flash in JSON format
4. The class attempts to connect to the access point using the SSID info stored.
5. If connection succeeds, the class stores the SSID info to a list of known APs

Commands: `wifi-connect`
## Automatic connection
1. The class recalls the last connected AP SSID info from flash and initializes Wi-Fi if required.
2. The class attempts to connect to the access point using the SSID
info recalled.
3. If the connection fails, the class will cycle through the list of
known APs until connection succeeds or the user interrupts the process.

Commands: `wifi-autoconnect`

## Link loss reconnection
1. The Wi-Fi was connected but the link went down due to AP shutdown, AP out of range, etc.
2. The class and Wi-Fi hardware automatically reconnects when the AP is back online or in range again.

Commands: none; the Pico-W will do this automatically. To cancel this
behavior, you have to explicityly run `wifi-deinitialize` after link loss.
