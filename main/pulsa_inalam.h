#ifndef MAIN_PULSA_INALAM_H_
#define MAIN_PULSA_INALAM_H_
#include "nvs.h"
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

nvs_handle handle_config;
uint8_t chipid[6];
void ota_task(void *pvParameter);

#ifdef __cplusplus
}
#endif


#endif /* MAIN_PULSA_INALAM_H_ */
