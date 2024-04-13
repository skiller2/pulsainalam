/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "tcpip_adapter.h"


#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "local_http_lib.h"
#include "libesphttpd/cgiwifi.h"


#include <esp_sleep.h>
#include "pulsa_inalam.h"


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

#define TEXT_BUFFSIZE 1024
// GPIO0 >> D3, GPIO1 >> TX, GPIO2 >> D4, GPIO3 >> RX,
// GPIO4 >> D2, GPIO5 >> D1
// Normal GPIO0 y GPIO2 a HIGH.   Flash GPIO0 low y GPIO2 a HIGH
// RX=CONFIG  TX=PULSADOR GPIO2=LED_SWITCH


//#define RESET_CONF GPIO_NUM_4
#define PULSADOR   GPIO_NUM_5
//#define LED_SWITCH   GPIO_NUM_2
//#define LED_WORKING GPIO_NUM_0

#define AP_WIFI_PASS      "11111111"
#define MAX_STA_CONN       2

// ESP-01
#define RESET_CONF GPIO_NUM_3
//#define PULSADOR   GPIO_NUM_1
#define LED_SWITCH   GPIO_NUM_2
#define LED_WORKING   GPIO_NUM_0
// /api/v1/movieventos/evento

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int SCANDONE_BIT = BIT2;



char SERVER_NAME[255]={0};
char SERVER_PORT[10]={0};
char SERVER_PATH[255]={0};
uint8_t radio_always_on;

static const char *TAG = "PUL";
static bool sendactive = false;

typedef struct
{
  uint8_t gpio_nro;     // data buffer
  uint32_t value;
  char gpio_label[20];
  char value_label[20];
  uint32_t value_anal;
  char value_anal_label[20];
} estado_t;


xQueueHandle qhNotif;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
    	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START WIFI");
        break;
    case SYSTEM_EVENT_SCAN_DONE:
    	ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE");
    	xEventGroupSetBits(wifi_event_group, SCANDONE_BIT);
    	wifiScanDoneCb();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
//    	ESP_LOGI(TAG, "LISTO");

//    	init_local_http();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
//        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
	static bool inited=false;
	if (!inited){
		tcpip_adapter_init();
		wifi_event_group = xEventGroupCreate();
		ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

		ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
		ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_FLASH) );
		ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
//		ESP_ERROR_CHECK( esp_wifi_start() );
		inited=true;
	}
}



uint16_t system_get_vdd33(void);
uint16_t readvdd33(void);
estado_t est_alarma;

static void send_task(void *pvParameters,uint8_t radio_always_on)
{
	estado_t *alarma=(estado_t*)pvParameters;

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    const TickType_t xTicksToWait = 15000 / portTICK_PERIOD_MS;
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[TEXT_BUFFSIZE+1];
    int retry=4;
    char *http_request = NULL;
    char *post_data = NULL;
    const char *POST_FORMAT =
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%s\r\n"
            "User-Agent: esp-idf/1.0 esp32\r\n"
    		"Connection: close\r\n"
    		"Content-Type: application/x-www-form-urlencoded;\r\n"
    		"Content-Length: %d\r\n"
    		"\r\n"
    		"%s"
    		;


	ESP_LOGI(TAG, "Enviando GPIO: %d Valor:%d", alarma->gpio_nro, alarma->value);

    sendactive=true;

    int get_len_post_data = asprintf(&post_data, "io_name=IO%02d&io_label=%s&value=%d&value_label=%s&id_disp_origen=%X%X%X%X%X%X&valor_analogico=%u&des_unidad_medida=%s",alarma->gpio_nro,alarma->gpio_label,alarma->value,alarma->value_label,chipid[0],chipid[1],chipid[2],chipid[3],chipid[4],chipid[5],alarma->value_anal,alarma->value_anal_label);

    int get_len = asprintf(&http_request, POST_FORMAT, SERVER_PATH, SERVER_NAME, SERVER_PORT,get_len_post_data,post_data);

    ESP_LOGI(TAG, "Server name: http://%s:%s%s", SERVER_NAME,SERVER_PORT, SERVER_PATH);

    if (radio_always_on==0) {
		esp_wifi_stop();
		esp_wifi_set_mode(WIFI_MODE_STA);
		esp_wifi_start();
		ESP_ERROR_CHECK( esp_wifi_connect() );
    }

    while(retry--) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
    	EventBits_t uxBits;
    	uxBits=xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, xTicksToWait);
    	if( ( uxBits & CONNECTED_BIT ) == 0 ){
 		   ESP_LOGE(TAG, "Cannot connect to AP");
 		   vTaskDelay(1000 / portTICK_PERIOD_MS);
 		   continue;
    	}

        ESP_LOGI(TAG, "Reintento: %d",retry);

        int err = getaddrinfo(SERVER_NAME, SERVER_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

	    s = socket(res->ai_family, res->ai_socktype, 0);
		if(s < 0) {
		   ESP_LOGE(TAG, "... Failed to allocate socket.");
		   freeaddrinfo(res);
		   vTaskDelay(1000 / portTICK_PERIOD_MS);
		   continue;
		}

		if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
		   ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
		   close(s);
		   freeaddrinfo(res);
		   vTaskDelay(1000 / portTICK_PERIOD_MS);
		   continue;
		}

		ESP_LOGI(TAG, "... connected");
		freeaddrinfo(res);

		if (write(s, http_request, get_len) < 0) {
		   ESP_LOGE(TAG, "... socket send failed");
		   close(s);
		   vTaskDelay(4000 / portTICK_PERIOD_MS);
		   continue;
		} else {
			ESP_LOGI(TAG, "Send request to server succeeded");
		}

		struct timeval receiving_timeout;
		receiving_timeout.tv_sec = 10;
		receiving_timeout.tv_usec = 0;
		if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
			 sizeof(receiving_timeout)) < 0) {
			ESP_LOGE(TAG, "... failed to set socket receiving timeout");
			close(s);
			vTaskDelay(4000 / portTICK_PERIOD_MS);
			continue;
		}

		/* Read HTTP response */
		do {
			bzero(recv_buf, sizeof(recv_buf));
			r = read(s, recv_buf, sizeof(recv_buf)-1);

			if (r < 0) { /*receive error*/
				ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
  			    vTaskDelay(1000 / portTICK_PERIOD_MS);
				continue;
			}
//			ets_printf("%s",recv_buf);
//			for(int i = 0; i < r; i++) {
//				putchar(recv_buf[i]);
//			}
		} while(r > 0);
		ESP_LOGI(TAG, "Connection closed, all packets received");
		close(s);
		break;
	}

    free(http_request);
    free(post_data);

    if (radio_always_on==0) {
		esp_wifi_set_mode(WIFI_MODE_NULL);
		esp_wifi_stop();
		sendactive=false;
    }
}

uint16_t phy_get_vdd33(void);

void check_task(void * parm)
{
	static int8_t pulsador;
	static uint16_t vdd33;
	int8_t pulsador_old=0;
	int8_t bat_baja=0,bat_baja_old=1;
	uint32_t sleep_time=0,min_bat=0;
	est_alarma.gpio_nro=17;
	strncpy(est_alarma.gpio_label, "Inicio", 20);
	strncpy(est_alarma.value_label, "Normal", 20);
	est_alarma.value=1;
	xQueueSend(qhNotif, (void *)&est_alarma,0);
	sendactive=true;

	nvs_get_u32(handle_config, "sleep_time", &sleep_time);
	nvs_get_u32(handle_config, "min_bat",    &min_bat);
	ESP_LOGI(TAG, "Valores: sleep_time %d min_bat %d",sleep_time,min_bat);
	gpio_set_level(LED_WORKING, 0);
	while(1) {
		pulsador=gpio_get_level(PULSADOR);
		if (pulsador!=pulsador_old){
			ESP_LOGI(TAG, "Pulsador: %d",pulsador);
			est_alarma.gpio_nro=PULSADOR;
			strncpy(est_alarma.gpio_label, "Pulsador", 20);
			strncpy(est_alarma.value_anal_label, "", 20);
			est_alarma.value=pulsador;
			est_alarma.value_anal=0;
			if (pulsador == 1) strncpy(est_alarma.value_label, "Alarma", 20); else strncpy(est_alarma.value_label, "Normal", 20);
			xQueueSend(qhNotif, (void *)&est_alarma,0);
			sendactive=true;
			if (pulsador==1) {
				gpio_set_level(LED_SWITCH, 1);
			}
		}
		if (pulsador==1){
			gpio_set_level(LED_SWITCH, ! gpio_get_level(LED_SWITCH) );
			ESP_LOGI(TAG, "EN estado alarma %d",gpio_get_level(LED_SWITCH));
		}

		pulsador_old=pulsador;

		if (!sendactive) {
			system_get_vdd33();
			vdd33=readvdd33();
			if (vdd33<min_bat){
				bat_baja=0;
			} else
				bat_baja=1;

			if (bat_baja!=bat_baja_old){
				ESP_LOGI(TAG, "Batería: %d tensión: %d",bat_baja,vdd33);
				est_alarma.gpio_nro=18;
				strncpy(est_alarma.gpio_label, "Batería", 20);
				strncpy(est_alarma.value_anal_label, "mV", 20);
				est_alarma.value=0;
				est_alarma.value_anal=vdd33;
				if (bat_baja == 1) strncpy(est_alarma.value_label, "Normal", 20); else strncpy(est_alarma.value_label, "Alarma", 20);
				xQueueSend(qhNotif, (void *)&est_alarma,0);
				sendactive=true;
			}
			bat_baja_old=bat_baja;
		}

		ESP_LOGI(TAG, "Alive sendactive: %d, pulsador: %d, tensión: %d ",sendactive,pulsador,vdd33);
		if ((!sendactive) && (pulsador==0)){

			//max 4294967295
			//    3600000000
			gpio_set_level(LED_WORKING, 1);
			if (sleep_time>0){
				ESP_LOGI(TAG, "Voy a dormir,  %d segundos",sleep_time);
				esp_deep_sleep(sleep_time*1000000u);
			} else
				ESP_LOGI(TAG, "Función sleep desactivada");
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
    vTaskDelete(NULL);
}

void report_task(void * parm)
{

	estado_t alarma;
	size_t str_len;

	nvs_get_str(handle_config,"server_name",NULL,&str_len);
	nvs_get_str(handle_config,"server_name",SERVER_NAME,&str_len);
	nvs_get_str(handle_config,"server_path",NULL,&str_len);
	nvs_get_str(handle_config,"server_path",SERVER_PATH,&str_len);
	nvs_get_str(handle_config,"server_port",NULL,&str_len);
	nvs_get_str(handle_config,"server_port",SERVER_PORT,&str_len);
	if (strlen(SERVER_PORT)==0)
		strcpy(SERVER_PORT,"80");

	while(true){
		if (xQueueReceive(qhNotif,&alarma,portMAX_DELAY)!=pdFALSE){
			ESP_LOGI(TAG, "Notifico gpio: %d Valor:%d", alarma.gpio_nro, alarma.value);
			send_task(&alarma,radio_always_on);
		}
	}
}

void app_main()
{
	ESP_LOGI(TAG, "SDK version: %s\n", esp_get_idf_version());
	esp_efuse_mac_get_default(chipid);

	ESP_LOGI(TAG, "Dispositivo ID: %X%X%X%X%X%X ",chipid[0],chipid[1],chipid[2],chipid[3],chipid[4],chipid[5]);

//	ESP_LOGI(TAG, "Dispositivo ID: %X",(unsigned int)chipid);


//	esp_log_level_set("*",ESP_LOG_NONE);

	//Init GPIOS
	gpio_config_t io_in_conf;


	io_in_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_PIN_INTR_POSEDGE;
	io_in_conf.mode     = GPIO_MODE_INPUT;
	io_in_conf.pull_up_en   = 1;
	io_in_conf.pull_down_en = 0;

	io_in_conf.pin_bit_mask = (1ULL<<RESET_CONF);
	gpio_config(&io_in_conf);

	io_in_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_PIN_INTR_POSEDGE;
	io_in_conf.mode     = GPIO_MODE_INPUT;
	io_in_conf.pull_up_en   = 0;
	io_in_conf.pull_down_en = 1;
	io_in_conf.pin_bit_mask = (1ULL<<PULSADOR);
	gpio_config(&io_in_conf);



	io_in_conf.pull_up_en   = 0;
	io_in_conf.mode     = GPIO_MODE_OUTPUT;
	io_in_conf.pin_bit_mask = (1ULL<<LED_SWITCH) | (1ULL<<LED_WORKING);
	gpio_config(&io_in_conf);

	gpio_set_level(LED_SWITCH, 1);
	gpio_set_level(LED_WORKING, 1);
   // Initialize NVS.
	//ESP_ERROR_CHECK(nvs_flash_erase());
	esp_err_t err = nvs_flash_init();

	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// If this happens, we erase NVS partition and initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &handle_config));

	nvs_get_u8(handle_config, "radio_always_on", &radio_always_on);

	initialise_wifi();
	if (gpio_get_level(RESET_CONF)==0 ) {  //Entra en modo configuracion

//	if (gpio_get_level(RESET_CONF)==1 ) {  //Entra en modo configuracion
		ESP_LOGI(TAG, "Modo configuracion");
		gpio_set_level(LED_SWITCH, 0);
		init_local_http();
	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	    wifi_config_t wifi_config = {
	        .ap = {
//	            .ssid = EXAMPLE_ESP_WIFI_SSID,
//	            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
	            .password = AP_WIFI_PASS,
	            .max_connection = MAX_STA_CONN,
	            .authmode = WIFI_AUTH_WPA_WPA2_PSK
	        },
	    };

	    sprintf((char*)wifi_config.ap.ssid, "PUL_%X%X%X%X%X%X", chipid[0],chipid[1],chipid[2],chipid[3],chipid[4],chipid[5]);
	    //int len=strlen((uint8_t*)wifi_config.ap.ssid);
	    int len=16;
	    wifi_config.ap.ssid_len= len;

	    if (strlen(AP_WIFI_PASS) == 0) {
	        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	    }

	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	    ESP_ERROR_CHECK(esp_wifi_start());
//		xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
	} else {
		ESP_LOGI(TAG, "Modo monitoreo");
		qhNotif=xQueueCreate( 100, sizeof( estado_t ) );
//	    xTaskCreate(check_task , "check_task"  , 4096, NULL, 3, NULL);
//	    xTaskCreate(report_task, "report_task" , 4096, NULL, 3, NULL);
		radio_always_on = 1;
		if (radio_always_on != 0)
		{
			init_local_http();
	    	esp_wifi_stop();
//		    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_MODE_STA, &wifi_config));

	    	esp_wifi_set_mode(WIFI_MODE_STA);
	    	esp_wifi_start();
		    ESP_ERROR_CHECK( esp_wifi_connect() );
	    }
	}
}
