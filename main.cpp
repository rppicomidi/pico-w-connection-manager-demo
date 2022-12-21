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
#include "lwip/apps/fs.h"
#include "main_lwipopts.h"
//#include "parson.h"

#if 0
// Use this function to discover what functions are called in ISR context
static void print_icsr_active()
{
    printf("active irq=%lu\r\n", 0x1F & *reinterpret_cast<volatile uint32_t *>(0xE000ED04));
}
#endif

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
    for (int idx = 0; idx < iNumParams; idx++) {
        if (strcmp(pcParam[idx], "state") == 0) {
            if (pcValue[idx][0] == '1') {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            }
            else if (pcValue[idx][0] == '0') {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            }
            else if (strcmp(pcValue[idx], "toggle") == 0) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN) ? 0:1);
            }
            else {
                printf("unknown state pcValue %s\r\n", pcValue[idx]);
            }
        }
        else {
            printf("unknown pcParam %s", pcParam[idx]);
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

static const char *aindex_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    LWIP_UNUSED_ARG(iIndex);
    LWIP_UNUSED_ARG(iNumParams);
    LWIP_UNUSED_ARG(pcParam);
    LWIP_UNUSED_ARG(pcValue);
    return "/aindex.shtml";
}

static tCGI pCGIs[] = {
    {"/led", (tCGIHandler) led_cgi_handler},
    {"/about", (tCGIHandler) about_cgi_handler},
    {"/aindex", (tCGIHandler) aindex_cgi_handler},
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

static void make_ajax_response_file_data(struct fs_file *file, const char* result, const char* content)
{
#if 1
    static char data[1024];
    size_t content_len = strlen(content) + 1;
    file->len = snprintf(data, sizeof(data), "HTTP/1.0 %s\r\nContent-Type: application/json;charset=UTF-8\r\nContent-Length: %d+\r\n\r\n%s", result, content_len, content);
    if ((size_t)file->len >= sizeof(data)) {
        printf("make_ajax_response_file_data: response truncated\r\n");
    }
#else
    // don't allocate memory in an irq
    std::string content_len = std::to_string(strlen(content) + 1);
    std::string file_str = std::string("HTTP/1.0 ")+std::string(result) +
            std::string("\nContent-Type: application/json\nContent-Length: ") +
                content_len+std::string("\n\n") + std::string(content);
    file->len = file_str.length();
    char* data = new char[file->len];
    strncpy(data, file_str.c_str(), file->len);
    data[file->len] = '\0';
#endif
    file->data = data;
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_CUSTOM;
}

static const char* get_led_state_json_string(bool is_on)
{
    static const char* onstr = "{\"ledState\":\"On\"}";
    static const char* offstr = "{\"ledState\":\"Off\"}";
    return is_on ? onstr : offstr;
}

static const char* get_led_state_json()
{
    return get_led_state_json_string(cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN));
}

// Required by LWIP_HTTPD_CUSTOM_FILES
int fs_open_custom(struct fs_file *file, const char *name)
{
    cyw43_arch_lwip_begin();
    int result = 0;
    const char* OK_200 = "200 OK";
    const char* Created_201 = "201 Created";
    const char* url = "/ledState.json";
    if (strncmp(url, name, strlen(url)) == 0) {
        //printf("got request for ledState\r\n");
        make_ajax_response_file_data(file, OK_200, get_led_state_json());
        result = 1;
    }
    else {
        url = "/ledStatePost.json";
        if (strncmp(url, name, strlen(url)) == 0) {
            //printf("got request for ledStatePost\r\n");
            make_ajax_response_file_data(file, Created_201, get_led_state_json());
            result = 1;
        }
    }
    cyw43_arch_lwip_end();
    return result;
}

void fs_close_custom(struct fs_file *file)
{
    #if 1
    LWIP_UNUSED_ARG(file);
    #else
    cyw43_arch_lwip_begin();
    if (file->data) {
        delete[] file->data;
        file->data = NULL;
    }
    cyw43_arch_lwip_end();
    #endif
}

static void *current_connection = NULL;
static void chardump(const char* buffer, size_t len)
{
    for (size_t idx=0; idx < len; idx++) {
        if (std::isprint(buffer[idx])) {
            printf("%c", buffer[idx]);
        }
        else {
            printf(".");
        }
    }
}
static void hexdump(const char* buffer, size_t len)
{
    printf("Dump %u bytes\r\n", len);
    size_t bytes_printed_on_line = 0;
    size_t idx = 0;
    for (; idx < len; idx++) {
        printf("%02x ", buffer[idx]);
        if (++bytes_printed_on_line == 16) {
            char partial[16];
            memcpy(partial, buffer+idx-15, 16);
            printf("   | ");
            chardump(partial, 16);
            bytes_printed_on_line = 0;
            printf("\r\n");
        }
    }
    if (bytes_printed_on_line != 0) {
        char partial[bytes_printed_on_line];
        memcpy(partial, buffer+idx-bytes_printed_on_line, bytes_printed_on_line);
        for (size_t idx=0; idx < 16-bytes_printed_on_line; idx++) {
            printf("   ");
        }
        printf("   | ");
        chardump(partial, bytes_printed_on_line);
        printf("\r\n");
    }
}
err_t httpd_post_begin(void *connection, const char *uri, const char *http_request, u16_t http_request_len,
                            int content_len, char *response_uri, u16_t response_uri_len, u8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    //printf("Got POST message %u bytes content %d bytes\r\n", http_request_len, content_len);
    //hexdump(http_request, http_request_len);
    if (!memcmp(uri, "/ledStatePost.json", 11)) {
    if (current_connection != NULL) {
        snprintf(response_uri, response_uri_len, "/429.html");
    }
    else  {
        current_connection = connection;
        /* default page is too many requests */
        snprintf(response_uri, response_uri_len, "/429.html");
        /* e.g. for large uploads to slow flash over a fast connection, you should
            manually update the rx window. That way, a sender can only send a full
            tcp window at a time. If this is required, set 'post_aut_wnd' to 0.
            We do not need to throttle upload speed here, so: */
        *post_auto_wnd = 1;
        return ERR_OK;
    }
    }
    return ERR_VAL;
}

err_t httpd_post_receive_data (void *connection, struct pbuf *p)
{
    cyw43_arch_lwip_begin();
    err_t result = ERR_VAL;
    if (connection == current_connection) {
        //char* data = new char[p->tot_len]; don't allocate memory on the heap in ISR
        char data[p->tot_len];
        char* buffer = static_cast<char*>(pbuf_get_contiguous(p, data, p->tot_len, p->tot_len, 0));
        if (buffer != NULL) {
            //printf("received POST data %u of %u bytes\r\n", p->len, p->tot_len);
#if 0 // parson allocates memory
            JSON_Value* value = json_parse_string(buffer);
            if (value != NULL) {
                JSON_Object* object = json_value_get_object(value);
                const char* led_state = json_object_get_string(object, "ledState");
                if (led_state != NULL) {
                    if (strncmp("On", led_state, 2) == 0) {
                        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
                    }
                    else if (strncmp("Off", led_state, 3) == 0) {
                        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
                    }
                    else {
                        printf("Unexpected ledState value %s\r\n", led_state);
                    }
                }
                json_value_free(value);
            }
            else {
                printf("value in POST not parsed as JSON\r\n");
                hexdump(buffer, p->tot_len);
            }
#endif
            if (strncmp(buffer, get_led_state_json_string(true), p->tot_len) == 0) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
            }
            else if (strncmp(buffer, get_led_state_json_string(false), p->tot_len) == 0) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
            }
            else {
                printf("Unexpected ledState value %s\r\n", buffer);
                hexdump(buffer, p->tot_len);
            }
        }
        else {
            printf("failed to get POST in buffer\r\n");
        }
        //delete[] data; don't allocate memory on the heap in ISR
        result =  ERR_OK;
    }
    pbuf_free(p);
    cyw43_arch_lwip_end();
    return result;
}

void httpd_post_finished (void *connection, char *response_uri, u16_t response_uri_len)
{
    if (connection == current_connection) {
        snprintf(response_uri, response_uri_len, "/ledStatePost.json");
        current_connection = NULL;
    }
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
