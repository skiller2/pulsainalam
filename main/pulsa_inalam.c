/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"


#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "local_http_lib.h"

#include <esp_sleep.h>


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

#define SLEEP_SECS 20
//#define SLEEP_SECS 60*60
#define SERVER_NAME "pepamon.local"
#define SERVER_PORT "85"
#define SERVER_PATH "/api/v1/movieventos/evento"
#define TEXT_BUFFSIZE 1024
// GPIO0 >> D3, GPIO1 >> TX, GPIO2 >> D4, GPIO3 >> RX,
// GPIO4 >> D2, GPIO5 >> D1
// Normal GPIO0 y GPIO2 a HIGH.   Flash GPIO0 low y GPIO2 a HIGH
// RX=CONFIG  TX=PULSADOR GPIO2=LED


#define RESET_CONF GPIO_NUM_4
#define PULSADOR   GPIO_NUM_5
#define LED   GPIO_NUM_0
#define LED_WORKING GPIO_NUM_0

#define EXAMPLE_ESP_WIFI_MODE_AP   CONFIG_ESP_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define EXAMPLE_ESP_WIFI_SSID      "ESP_"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_MAX_STA_CONN       2
// ESP-01
//#define RESET_CONF GPIO_NUM_3
//#define PULSADOR   GPIO_NUM_1
//#define LED   GPIO_NUM_2
//#define LED_WORKING   GPIO_NUM_0



/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "PUL";
static bool sendactive = false;

typedef struct
{
  uint8_t gpio_nro;     // data buffer
  uint32_t value;
  char gpio_label[20];
  char value_label[20];
} estado_t;


xQueueHandle qhNotif;

void smartconfig_example_task(void * parm);

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
    	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START WIFI");
        break;
    case SYSTEM_EVENT_SCAN_DONE:
    	ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE");
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    	ESP_LOGI(TAG, "LISTO");

    	init_local_http();
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

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Pp: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            esp_restart();
            vTaskDelete(NULL);
        }
    }
}

uint16_t system_get_vdd33(void);
uint16_t readvdd33(void);
estado_t est_alarma;

static void send_task(void *pvParameters)
{
	estado_t *alarma=(estado_t*)pvParameters;
	ESP_LOGI(TAG, "Enviando GPIO: %d Valor:%d", alarma->gpio_nro, alarma->value);

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
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

    sendactive=true;
	uint8_t chipid[6];
	esp_efuse_mac_get_default(chipid);

    int get_len_post_data = asprintf(&post_data, "gpio_name=GPIO%d&gpio_label=\"%s\"&value=%d&value_label=\"%s\"&stm_event=&cod_equipo=\"%X\"",alarma->gpio_nro,alarma->gpio_label,alarma->value,alarma->value_label,(unsigned int)chipid);
    int get_len = asprintf(&http_request, POST_FORMAT, SERVER_PATH, SERVER_NAME, SERVER_PORT,get_len_post_data,post_data);

    ESP_LOGI(TAG, "Server name: %s ", "http://" SERVER_NAME ":" SERVER_PORT SERVER_PATH);
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    ESP_ERROR_CHECK( esp_wifi_connect() );

    while(retry--) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Reintento: %d",retry);

        int err = getaddrinfo(SERVER_NAME, SERVER_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
//            vTaskDelay(1000 / portTICK_PERIOD_MS);
//            continue;
//            int err = getaddrinfo("192.168.8.106", SERVER_PORT, &hints, &res);
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
		ESP_LOGI(TAG, "... allocated socket");

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

			for(int i = 0; i < r; i++) {
				putchar(recv_buf[i]);
			}
		} while(r > 0);
		ESP_LOGI(TAG, "Connection closed, all packets received");
		close(s);
		break;
	}

    free(http_request);
    free(post_data);

//    esp_wifi_set_mode(WIFI_MODE_NULL);
//    esp_wifi_stop();
//    sendactive=false;
}

uint16_t phy_get_vdd33(void);

void check_task(void * parm)
{
	static int8_t pulsador;
	static uint16_t vdd33;
	int8_t pulsador_old=1;
	int8_t bat_baja=0,bat_baja_old=1;
	ESP_LOGI(TAG, "Comienzo1");
	//Prueba
	est_alarma.gpio_nro=5;
	strncpy(est_alarma.gpio_label, "Pruebón", 20);
	strncpy(est_alarma.value_label, "OK", 20);
	est_alarma.value=5;
	xQueueSend(qhNotif, (void *)&est_alarma,0);
	sendactive=true;
	gpio_set_level(LED_WORKING, 1);
	ESP_LOGI(TAG, "Comienzo2");
	while(1) {
		pulsador=gpio_get_level(PULSADOR);
		if (pulsador!=pulsador_old){
			ESP_LOGI(TAG, "Pulsador: %d",pulsador);
			est_alarma.gpio_nro=1;
			strncpy(est_alarma.gpio_label, "Pulsador", 20);
			est_alarma.value=pulsador;
			if (pulsador == 1) strncpy(est_alarma.value_label, "Normal", 20); else strncpy(est_alarma.value_label, "Alarma", 20);
			xQueueSend(qhNotif, (void *)&est_alarma,0);
			sendactive=true;
			if (pulsador==1) {
				gpio_set_level(LED, 0);
			}
		}
		if (pulsador==0){
			gpio_set_level(LED, ! gpio_get_level(LED) );
			ESP_LOGI(TAG, "EN estado alarma %d",gpio_get_level(LED));
		}

		pulsador_old=pulsador;

		if (!sendactive) {
			system_get_vdd33();
			vdd33=readvdd33();
			if (vdd33<1900){
				bat_baja=0;
			} else
				bat_baja=1;

			if (bat_baja!=bat_baja_old){
				ESP_LOGI(TAG, "Batería: %d tensión: %d",bat_baja,vdd33);
				est_alarma.gpio_nro=2;
				strncpy(est_alarma.gpio_label, "Batería", 20);
				est_alarma.value=vdd33;
				if (bat_baja == 1) strncpy(est_alarma.value_label, "Normal", 20); else strncpy(est_alarma.value_label, "Alarma", 20);
				xQueueSend(qhNotif, (void *)&est_alarma,0);
				sendactive=true;
			}
			bat_baja_old=bat_baja;
		}

		ESP_LOGI(TAG, "Alive sendactive: %d, pulsador: %d, tensión: %d ",sendactive,pulsador,vdd33);
		if ((!sendactive) && (pulsador==1)){
			ESP_LOGI(TAG, "Voy a dormir,  %d segundos",SLEEP_SECS);
			//max 4294967295
			//    3600000000
			gpio_set_level(LED_WORKING, 0);
			esp_deep_sleep(SLEEP_SECS*1000000u);

		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
    vTaskDelete(NULL);
}

void report_task(void * parm)
{
	estado_t alarma;
	while(true){
		if (xQueueReceive(qhNotif,&alarma,1000)!=pdFALSE){
			ESP_LOGI(TAG, "Notifico gpio: %d Valor:%d", alarma.gpio_nro, alarma.value);
			send_task(&alarma);
		}
	}
}

void app_main()
{
	ESP_LOGI(TAG, "SDK version:%s\n", esp_get_idf_version());


	//Init GPIOS
	gpio_config_t io_in_conf;
	io_in_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_PIN_INTR_POSEDGE;
	io_in_conf.mode     = GPIO_MODE_INPUT;
	io_in_conf.pull_up_en   = 1;
	io_in_conf.pull_down_en = 0;

	io_in_conf.pin_bit_mask = (1ULL<<RESET_CONF) | (1ULL<<PULSADOR);
	gpio_config(&io_in_conf);

//	io_in_conf.pin_bit_mask = (1ULL<<PULSADOR) ;
//	gpio_config(&io_in_conf);

	io_in_conf.pull_up_en   = 0;
	io_in_conf.mode     = GPIO_MODE_OUTPUT;
	io_in_conf.pin_bit_mask = (1ULL<<LED) | (1ULL<<LED_WORKING);
	gpio_config(&io_in_conf);

   // Initialize NVS.
	esp_err_t err = nvs_flash_init();

	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// If this happens, we erase NVS partition and initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );


	initialise_wifi();

	if (gpio_get_level(RESET_CONF)==0) {  //Entra en modo configuracion
		ESP_LOGI(TAG, "Modo configuracion");
/*
	    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	    wifi_config_t wifi_config = {
	        .ap = {
	            .ssid = EXAMPLE_ESP_WIFI_SSID,
	            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
	            .password = EXAMPLE_ESP_WIFI_PASS,
	            .max_connection = EXAMPLE_MAX_STA_CONN,
	            .authmode = WIFI_AUTH_WPA_WPA2_PSK
	        },
	    };
	    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
	        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	    }

	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	    ESP_ERROR_CHECK(esp_wifi_start());
*/
	    esp_wifi_start();
		xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);



	} else {
		ESP_LOGI(TAG, "Modo monitoreo");
		qhNotif=xQueueCreate( 100, sizeof( estado_t ) );
	    xTaskCreate(check_task , "check_task"  , 4096, NULL, 3, NULL);
	    xTaskCreate(report_task, "report_task" , 4096, NULL, 3, NULL);
	}

}
