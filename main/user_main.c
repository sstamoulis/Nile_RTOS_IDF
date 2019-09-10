/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <time.h>

#include <esp_system.h>
#include <lwip/apps/sntp.h>
#include <lwip/ip_addr.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "wifi_connect.h"

static const char *TAG = "AppMain";

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "SDK version: %s\n", esp_get_idf_version());
    wifi_init();
}
