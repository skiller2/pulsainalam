#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "pulsa_inalam.h"

static const char *TAG = "PROV";
wifi_config_t wifi_config;

void promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type)
{
  char databuf[100];
  uint8_t len = 0;
  databuf[0] = 0;
  const char delim[2] = ":";
  char *token;

  if (type != WIFI_PKT_MGMT)
    return; // Solo queremos paquetes de gestión

  const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *payload = pkt->payload;




/*
  if (payload[0] != 0x40 || payload[10] != 0xAA || payload[11] != 0xBB || payload[12] != 0xCC || payload[13] != 0xFE || payload[14] != 0x00 || payload[15] != 0x06)
    return;
  len = payload[39];

  if (len > 100)
    return;

  memcpy(databuf, &payload[40], len);
*/



  strcpy(databuf, "SKLRDVC:1q2w3e4r:192.168.1.1:80:/test");
  len = 37;

  databuf[len] = 0;


  ESP_LOGI(TAG, "Leo len:%d, %s  ", len, databuf);


  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

  token = strtok(databuf, delim); // SSID
  strncpy((char *)wifi_config.sta.ssid, token, sizeof(wifi_config.sta.ssid));

  token = strtok(NULL, delim); // PASS
  strncpy((char *)wifi_config.sta.password, token, sizeof(wifi_config.sta.password));


  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

  token = strtok(NULL, delim);
  nvs_set_str(handle_config, "server_name", token);

  token = strtok(NULL, delim);
  nvs_set_str(handle_config, "server_port", token);

  token = strtok(NULL, delim);
  nvs_set_str(handle_config, "server_path", token);

  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(0));
}

void prov_task(void *pvParameter)
{
  ESP_LOGI(TAG, "Inicio Aprovisionamiento");
  xEventGroupSetBits(wifi_event_group, PROVISION_ON);


  wifi_config_t wifi_cfg_empty, wifi_cfg_old;
  memset(&wifi_cfg_empty, 0, sizeof(wifi_config_t));
  esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg_old);
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg_empty);
  esp_wifi_disconnect();

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA))
  //ESP_ERROR_CHECK(esp_wifi_start());
  
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(promiscuous_callback));


  wifi_promiscuous_filter_t filter = {
      .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT // | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL | WIFI_PROMIS_FILTER_MASK_MISC,
  };

  ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(1));

  int canal = 1;
  bool prom_status = false;
  while (true)
  {
    if (canal > 13)
      canal = 1;
    vTaskDelay(300 / portTICK_PERIOD_MS);
    //		canal = 0;
    ESP_ERROR_CHECK(esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE));
    //		ESP_LOGI(TAG, "Canal: %d", canal);

    canal++;

    esp_wifi_get_promiscuous(&prom_status);
    if (!prom_status)
      break;
    /* code */
  }


  ESP_ERROR_CHECK(esp_wifi_start())
  ESP_ERROR_CHECK(esp_wifi_connect());

  ESP_LOGI(TAG, "Fin Aprovisionamiento");

  xEventGroupClearBits(wifi_event_group, PROVISION_ON);


  vTaskDelete(NULL);
}
