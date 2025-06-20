#ifndef MAIN_PULSA_INALAM_H_
#define MAIN_PULSA_INALAM_H_
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

nvs_handle handle_config;
uint8_t chipid[6];
void ota_task(void *pvParameter);

EventGroupHandle_t wifi_event_group;
static const int PROVISION_ON = BIT3;








#ifdef __cplusplus
}
#endif


#endif /* MAIN_PULSA_INALAM_H_ */
