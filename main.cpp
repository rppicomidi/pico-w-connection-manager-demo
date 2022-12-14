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
    "Hello",
    "counter",
};

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
    size_t printed;
    switch (iIndex) {
        case 0: /* "Hello" */
            printed = snprintf(pcInsert, iInsertLen, "Hello user number %d!", rand());
            break;
        case 1: /* "counter" */
        {
            static int counter;
            counter++;
            printed = snprintf(pcInsert, iInsertLen, "%d", counter);
        }
            break;
        default: /* unknown tag */
            printed = 0;
            break;
    }
    LWIP_ASSERT("sane length", printed <= 0xFFFF);
    return (u16_t)printed;
}

void ssi_init() {
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
    httpd_init();
    ssi_init();
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
