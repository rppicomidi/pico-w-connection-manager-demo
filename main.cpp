/**
 * MIT License
 *
 * Copyright (c) 2022 rppicomidi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico_w_connection_manager.h"
#include "pico_w_connection_manager_cli.h"
#include "pico_lfs_cli.h"
#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"

#include "main_lwipopts.h"

static void onCommand(const char* name, char *tokens)
{
    printf("Received command: %s\r\n",name);

    for (int i = 0; i < embeddedCliGetTokenCount(tokens); ++i) {
        printf("Arg %d : %s\r\n", i, embeddedCliGetToken(tokens, i + 1));
    }
}

static void onCommandFn(EmbeddedCli *embeddedCli, CliCommand *command)
{
    (void)embeddedCli;
    embeddedCliTokenizeArgs(command->args);
    onCommand(command->name == NULL ? "" : command->name, command->args);
}

static void writeCharFn(EmbeddedCli *embeddedCli, char c)
{
    (void)embeddedCli;
    putchar(c);
}

// max length of the tags defaults to be 8 chars
// LWIP_HTTPD_MAX_TAG_NAME_LEN
const char * ssi_example_tags[] = {
    "led",
};

enum {
    SSI_LED_STATE
};

static u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen)
{
    switch (iIndex) {

        case SSI_LED_STATE:
            snprintf(pcInsert, iInsertLen, cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN)? "On" : "Off");
            break;
        default:
            snprintf(pcInsert, iInsertLen, "N/A");
            break;
    }

    /* Tell the server how many characters to insert */
    return (u16_t)strlen(pcInsert);
}


static const char *led_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    LWIP_UNUSED_ARG(iIndex);
    LWIP_UNUSED_ARG(pcValue);
    for (int idx = 0; idx < iNumParams; idx++) {
        if (strcmp(pcParam[idx], "on") == 0) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        } else if (strcmp(pcParam[idx], "off") == 0) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        } else if (strcmp(pcParam[idx], "toggle") == 0) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN) ? 0:1);
        }
    }
    return "/index.shtml";
}

static const char *about_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    LWIP_UNUSED_ARG(iIndex);
    LWIP_UNUSED_ARG(iNumParams);
    LWIP_UNUSED_ARG(pcParam);
    LWIP_UNUSED_ARG(pcValue);
    return "/about.html";
}


static tCGI pCGIs[] = {
    {"/led", (tCGIHandler) led_cgi_handler},
    {"/about", (tCGIHandler) about_cgi_handler},
};

static void cgi_init() {
    http_set_cgi_handlers(pCGIs, sizeof (pCGIs) / sizeof (pCGIs[0]));
}

static void ssi_init() {
    size_t idx;
    for (idx = 0; idx < LWIP_ARRAYSIZE(ssi_example_tags); idx++) {
        LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN",
        strlen(ssi_example_tags[idx]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
    }

    http_set_ssi_handler(ssi_handler, ssi_example_tags, LWIP_ARRAYSIZE(ssi_example_tags));
}


int main()
{
    // Initialize the console
    stdio_init_all();
    printf("pico-w-wifi-setup demo\r\n");
    while(getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {
        // flush out the console input buffer
    }

    // Initialize the CLI
    EmbeddedCliConfig cfg = {
        .rxBufferSize = 64,
        .cmdBufferSize = 64,
        .historyBufferSize = 128,
        .maxBindingCount = static_cast<uint16_t>(rppicomidi::Pico_lfs_cli::get_num_commands() + 
            rppicomidi::Pico_w_connection_manager_cli::get_num_commands()),
        .cliBuffer = NULL,
        .cliBufferSize = 0,
        .enableAutoComplete = true,
    };
    EmbeddedCli *cli = embeddedCliNew(&cfg);
    cli->onCommand = onCommandFn;
    cli->writeChar = writeCharFn;
    rppicomidi::Pico_w_connection_manager wifi;
    rppicomidi::Pico_w_connection_manager_cli wifi_cli(cli);
    rppicomidi::Pico_lfs_cli lfs_cli(cli);
    wifi_cli.setup_cli(&wifi);
    printf("Cli is running.\r\n");
    printf("Type \"help\" for a list of commands\r\n");
    printf("Use backspace and tab to remove chars and autocomplete\r\n");
    printf("Use up and down arrows to recall previous commands\r\n");

    embeddedCliProcess(cli);
    // Get connected
    bool connected = false;
    // If successfully loaded settings, attempt to autoconnect now.
    if (wifi.load_settings()) {
        wifi.autoconnect();
    }
    while (!connected) {
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            embeddedCliReceiveChar(cli, c);
            embeddedCliProcess(cli);
        }
        wifi.task();
        connected = wifi.get_state() == wifi.CONNECTED;
    }
    // Initialize the webserver
    ssi_init();
    cgi_init();
    httpd_init();
    // Continue processing commands and updating the wifi connection status
    while (1) {
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            embeddedCliReceiveChar(cli, c);
            embeddedCliProcess(cli);
        }
        wifi.task();
    }
    return 0;
}
