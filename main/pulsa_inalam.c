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
#include "rf_driver.h"
#include "driver/pwm.h"
#include "driver/uart.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

#define TEXT_BUFFSIZE 1024
// GPIO0 >> D3, GPIO1 >> TX, GPIO2 >> D4, GPIO3 >> RX,
// GPIO4 >> D2, GPIO5 >> D1
// Normal GPIO0 y GPIO2 a HIGH.   Flash GPIO0 low y GPIO2 a HIGH
// RX=CONFIG  TX=PULSADOR GPIO2=LED_SWITCH

// #define CONFIGURA GPIO_NUM_4
#define PULSADOR GPIO_NUM_4

#define AP_WIFI_PASS "11111111"
#define MAX_STA_CONN 2

#define SENSOR_TIPE 2

// ESP-01
#define CONFIGURA GPIO_NUM_13
// #define PULSADOR   GPIO_NUM_1
#define LED_SWITCH GPIO_NUM_5
#define RF_TX_IO_NUM GPIO_NUM_14
// /api/v1/movieventos/evento

/* The event group allows multiple bits for each event,
	 but we only care about one event - are we connected
	 to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int SCANDONE_BIT = BIT2;
#define PUL_ON 1
#define PUL_OFF 0

char SERVER_NAME[255] = {0};
char SERVER_PORT[10] = {0};
char SERVER_PATH[255] = {0};
uint8_t radio_always_on;
uint32_t sendcode = 0;

static const char *TAG = "PUL";
static bool sendactive = false;

static int8_t pulsador;
int8_t pulsador_old = 255;
int8_t bat_baja = 0, bat_baja_old = 255;
uint32_t sleep_time = 0, min_bat = 0;

typedef struct
{
	uint8_t butt_status;
	uint8_t low_bat;
	uint32_t batt_vcc;
	uint32_t sleep_time_sec;
	uint8_t sensor_type;
	char data[50];
} estado_t;

xQueueHandle qhNotif;

void set_mode_config(){
			ESP_LOGI(TAG, "Modo configuracion");
		//	gpio_set_level(LED_SWITCH, 0);
			esp_wifi_stop();
			esp_wifi_deinit();
			init_local_http();
			wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
			ESP_ERROR_CHECK(esp_wifi_init(&cfg));
			wifi_config_t wifi_config = {
					.ap = {
							//	            .ssid = EXAMPLE_ESP_WIFI_SSID,
							//	            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
							.password = AP_WIFI_PASS,
							.max_connection = MAX_STA_CONN,
							.authmode = WIFI_AUTH_WPA_WPA2_PSK},
			};

			sprintf((char *)wifi_config.ap.ssid, "PUL_%X%X%X%X%X%X", chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);
			// int len=strlen((uint8_t*)wifi_config.ap.ssid);
			int len = 16;
			wifi_config.ap.ssid_len = len;

			if (strlen(AP_WIFI_PASS) == 0)
			{
				wifi_config.ap.authmode = WIFI_AUTH_OPEN;
		}

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
		ESP_ERROR_CHECK(esp_wifi_start());
		
		//		xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);

}



void ledsw(bool send, bool pul)
{
	static bool inited = false;
	float phase[1] = {0};
	uint32_t duties[1] = {0};
	uint32_t pin_num[1] = {LED_SWITCH};

	if (!inited)
	{
		pwm_init(250000, duties, 1, pin_num);
		inited = true;
	}

	if (pul)
		duties[0] = 50000;
	if (send)
		duties[0] = 200000;

	if (duties[0] > 0)
	{
		//		gpio_set_level(LED_SWITCH, 1);
		pwm_set_phases(phase);
		pwm_set_duties(duties);
		pwm_start();
	}
	else
	{
		pwm_stop(0);
		pwm_deinit();
		gpio_set_level(LED_SWITCH, 0);
	}
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch (event->event_id)
	{
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

#define EX_UART_NUM UART_NUM_0

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;

static void uart_event_task(void *pvParameters)
{
	uart_event_t event;
	uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
	uint8_t butt_status;
	int m;
	int sig;
	estado_t est_sensor;

	for (;;)
	{

		// Waiting for UART event.
		if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY))
		{
			bzero(dtmp, RD_BUF_SIZE);
			ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);

			switch (event.type)
			{
			// Event of UART receving data
			// We'd better handler data event fast, there would be much more data events than
			// other types of events. If we take too much time on data event, the queue might be full.
			case UART_DATA:
				ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
				uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
				dtmp[event.size] = 0;
				char *strres = strtok((char *)dtmp, "\n");
				while (strres != 0)
				{
					sig = 255;
					m = 255;
					butt_status = 0;
					ESP_LOGI(TAG, "[UART DATA PROC]: %s", strres);

					sscanf(strres, "m=%d, sig=%d", &m, &sig);
					// strcpy(est_sensor.data, strres);

					if (sig != 255 || m != 255)
					{
						ESP_LOGI(TAG, "Recibo: m=%d, sig=%d", m, sig);
						switch (sig)
						{
						case 3: // TEST
						case 7:
							/* code */
							butt_status = 2;
							break;
						case 18: // ALARMA
							/* code */
							butt_status = 1;
							break;
						case 19: // LINK
							/* iniciar en modo ap */
							set_mode_config();
							break;

						default:
							butt_status = 0;
							break;
						}

						if (butt_status != 0)
						{
							est_sensor.batt_vcc = esp_wifi_get_vdd33();
							est_sensor.butt_status = butt_status;
							est_sensor.low_bat = bat_baja;
							est_sensor.sleep_time_sec = sleep_time;
							est_sensor.sensor_type = SENSOR_TIPE;
							est_sensor.data[0] = 0;

							xQueueSend(qhNotif, (void *)&est_sensor, 0);
							sendactive = true;
						}
					}
					strres = strtok(NULL, "\n");
				}

				ESP_LOGI(TAG, "[DATA EVT]:");
				// uart_write_bytes(EX_UART_NUM, (const char *) dtmp, event.size);
				break;

			// Event of HW FIFO overflow detected
			case UART_FIFO_OVF:
				ESP_LOGI(TAG, "hw fifo overflow");
				// If fifo overflow happened, you should consider adding flow control for your application.
				// The ISR has already reset the rx FIFO,
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(EX_UART_NUM);
				xQueueReset(uart0_queue);
				break;

			// Event of UART ring buffer full
			case UART_BUFFER_FULL:
				ESP_LOGI(TAG, "ring buffer full");
				// If buffer full happened, you should consider encreasing your buffer size
				// As an example, we directly flush the rx buffer here in order to read more data.
				uart_flush_input(EX_UART_NUM);
				xQueueReset(uart0_queue);
				break;

			case UART_PARITY_ERR:
				ESP_LOGI(TAG, "uart parity error");
				break;

			// Event of UART frame error
			case UART_FRAME_ERR:
				ESP_LOGI(TAG, "uart frame error");
				break;

			// Others
			default:
				ESP_LOGI(TAG, "uart event type: %d", event.type);
				break;
			}
		}
	}

	free(dtmp);
	dtmp = NULL;
	vTaskDelete(NULL);
}

static void initialise_serial(void)
{
	// Configure parameters of an UART driver,
	// communication pins and install the driver
	uart_config_t uart_config = {
			.baud_rate = 9600,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
	uart_param_config(EX_UART_NUM, &uart_config);

	// Install UART driver, and get the queue.
	uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 100, &uart0_queue, 0);

	// Create a task to handler UART event from ISR
	xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
}

static void initialise_wifi(void)
{
	static bool inited = false;
	if (!inited)
	{
		tcpip_adapter_init();
		wifi_event_group = xEventGroupCreate();
		ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
		ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
		//		ESP_ERROR_CHECK( esp_wifi_start() );
		inited = true;
	}
}

estado_t est_alarma;

inline uint32_t IRAM_ATTR micros()
{
	uint32_t ccount;
	asm volatile("rsr %0, ccount" : "=a"(ccount));
	return ccount;
}

void IRAM_ATTR local_delay_us(uint32_t us)
{
	// os_delay_us(us);  18 = 820 & 17 = 700

	for (size_t i = 0; i < us * 18; i++)
	{
		__asm__ __volatile__("nop");
	}

	/*
		if (us)
		{
			uint32_t m = micros();
			while ((micros() - m) < us)
			{
				__asm__ __volatile__ ("nop");
			}
		}
		*/
}

inline void send1() // 910+410=1320
{
	gpio_set_level(RF_TX_IO_NUM, 1);
	local_delay_us(1030);
	gpio_set_level(RF_TX_IO_NUM, 0);
	local_delay_us(310);
}
inline void send0() // 400+920
{
	gpio_set_level(RF_TX_IO_NUM, 1);
	local_delay_us(310);
	gpio_set_level(RF_TX_IO_NUM, 0);
	local_delay_us(1030);
}

inline void preamble()
{
	local_delay_us(2000);
	gpio_set_level(RF_TX_IO_NUM, 1);
	local_delay_us(400);
	gpio_set_level(RF_TX_IO_NUM, 0);
	local_delay_us(9000);
}

uint32_t reverseBits(uint32_t num)
{
	uint32_t NO_OF_BITS = sizeof(num) * 8;
	uint32_t reverse_num = 0;
	int i;
	for (i = 0; i < NO_OF_BITS; i++)
	{
		if ((num & (1 << i)))
			reverse_num |= 1 << ((NO_OF_BITS - 1) - i);
	}
	return reverse_num;
}

void tx_rf_433_task(void *arg)
{
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = 1 << RF_TX_IO_NUM;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);
	gpio_set_level(RF_TX_IO_NUM, 0);
	uint32_t sendcode = (uint32_t)arg;

	ESP_LOGI(TAG, "Codigo recibido %d", sendcode);
	// while (1)
	//{
	//  OFF 1001101100000100011000010
	//  ON  1001101100000100011000100
	//  pulso 1ms + silencio 9ms
	for (size_t i = 0; i < 2; i++)
	{
		preamble();
		send1();
		send0();
		send0();
		send1();
		send1();
		send0();
		send1();
		send1();
		send0();
		send0();
		send0();
		send0();
		send0();
		send1();
		send0();
		send0();
		send0();
		send1();
		send1();
		send0();
		send0();
		send0();
		if (sendcode == 2)
		{
			send0();
			send1();
		}
		else
		{
			send1();
			send0();
		}

		send0();

		vTaskDelay(9 / portTICK_RATE_MS);
	}

	//	vTaskDelay(2000 / portTICK_RATE_MS);
	//}

	vTaskDelete(NULL);
}

void rf_tx_task(void *arg)
{
	rf_tx_config_t ir_tx_config = {
			.io_num = RF_TX_IO_NUM,
			.freq = 0,
			.timer = RF_TX_HW_TIMER // WDEV timer will be more accurate, but PWM will not work
	};

	rf_tx_init(&ir_tx_config);

	rf_tx_nec_data_t ir_data[5];
	/*
			The standard NEC ir code is:
			addr + ~addr + cmd + ~cmd
	*/
	ir_data[0].addr1 = 0x55;
	ir_data[0].addr2 = ~0x55;
	ir_data[0].cmd1 = 0x00;
	ir_data[0].cmd2 = ~0x00;

	while (1)
	{
		for (int x = 1; x < 5; x++)
		{ // repeat 4 times
			ir_data[x] = ir_data[0];
		}

		rf_tx_send_data(ir_data, 5, portMAX_DELAY);
		ESP_LOGI(TAG, "ir tx nec: addr:%02xh;cmd:%02xh;repeat:%d", ir_data[0].addr1, ir_data[0].cmd1, 4);
		ir_data[0].cmd1++;
		ir_data[0].cmd2 = ~ir_data[0].cmd1;
		vTaskDelay(2000 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}

bool wifi_connected = false;
void wifi_sta_start()
{
	if (wifi_connected == false)
	{
		ESP_LOGI(TAG, "Start WIFI");
		esp_wifi_stop();
		esp_wifi_set_mode(WIFI_MODE_STA);
		esp_wifi_start();
		ESP_ERROR_CHECK(esp_wifi_connect());
		wifi_connected = true;
	}
}

void wifi_sta_stop()
{
	if (radio_always_on == 0)
	{
		ESP_LOGI(TAG, "Stop WIFI");
		esp_wifi_set_mode(WIFI_MODE_NULL);
		esp_wifi_stop();
		sendactive = false;
		wifi_connected = false;
	}
}

static void send_task(estado_t *alarma, uint8_t radio_always_on)
{
	//	estado_t *alarma = (estado_t *)pvParameters;

	const struct addrinfo hints = {
			.ai_family = AF_INET,
			.ai_socktype = SOCK_STREAM,
	};
	const TickType_t xTicksToWait = 15000 / portTICK_PERIOD_MS;
	struct addrinfo *res;
	struct in_addr *addr;
	int s, r;
	char recv_buf[TEXT_BUFFSIZE + 1];
	int retry = 2;
	char *http_request = NULL;
	char *post_data = NULL;
	char type[10] = "";
	const char *POST_FORMAT =
			"POST %s HTTP/1.1\r\n"
			"Host: %s:%s\r\n"
			"User-Agent: esp-idf/1.0 esp8266\r\n"
			"Connection: close\r\n"
			"Content-Type: application/json;\r\n"
			"Content-Length: %d\r\n"
			"\r\n"
			"%s";

	//	ESP_LOGI(TAG, "Enviando PULSADOR: %d,  BATERIA BAJA:%d, VCC:%d, DESCANSO: %d", alarma.butt_status, alarma.low_bat,alarma.batt_vcc,alarma.sleep_time_sec);

	sendactive = true;
	switch (alarma->sensor_type)
	{
	case 1:
		strcpy(type, "button");
		break;
	case 2:
		strcpy(type, "smoke");
		break;

	default:
		break;
	}
	int get_len_post_data = asprintf(&post_data, "{\"origin\":\"%X%X%X%X%X%X\",\"button\":\"%d\",\"low_bat\":\"%d\",\"type\":\"%s\",\"sleep_time_sec\":\"%d\",\"bat_vcc_mv\":\"%d\",\"data\":\"%s\"}", chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5], alarma->butt_status, alarma->low_bat, type, alarma->sleep_time_sec, alarma->batt_vcc, alarma->data);

	// int get_len_post_data = asprintf(&post_data, "io_name=IO%02d&io_label=%s&value=%d&value_label=%s&id_disp_origen=%X%X%X%X%X%X&valor_analogico=%u&des_unidad_medida=%s", alarma->gpio_nro, alarma->gpio_label, alarma->value, alarma->value_label, chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5], alarma->value_anal, alarma->value_anal_label);
	// int get_len_post_data = asprintf(&post_data, "io_name=IO%02d&io_label=%s&value=%d&value_label=%s&id_disp_origen=%X%X%X%X%X%X&valor_analogico=%u&des_unidad_medida=%s", alarma->gpio_nro, alarma->gpio_label, alarma->value, alarma->value_label, chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5], alarma->value_anal, alarma->value_anal_label);

	int get_len = asprintf(&http_request, POST_FORMAT, SERVER_PATH, SERVER_NAME, SERVER_PORT, get_len_post_data, post_data);

	ESP_LOGI(TAG, "Server name: http://%s:%s%s", SERVER_NAME, SERVER_PORT, SERVER_PATH);

	wifi_sta_start();

	while (retry--)
	{
		/* Wait for the callback to set the CONNECTED_BIT in the
			 event group.
		*/
		EventBits_t uxBits;
		uint8_t recv_buf_len = 0;
		uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
																 false, true, xTicksToWait);
		if ((uxBits & CONNECTED_BIT) == 0)
		{
			ESP_LOGE(TAG, "Cannot connect to AP");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}

		ESP_LOGI(TAG, "Reintento: %d", retry);

		int err = getaddrinfo(SERVER_NAME, SERVER_PORT, &hints, &res);

		if (err != 0 || res == NULL)
		{
			ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}

		addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
		ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

		s = socket(res->ai_family, res->ai_socktype, 0);
		if (s < 0)
		{
			ESP_LOGE(TAG, "... Failed to allocate socket.");
			freeaddrinfo(res);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}

		if (connect(s, res->ai_addr, res->ai_addrlen) != 0)
		{
			ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
			close(s);
			freeaddrinfo(res);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}

		ESP_LOGI(TAG, "... connected");
		freeaddrinfo(res);

		if (write(s, http_request, get_len) < 0)
		{
			ESP_LOGE(TAG, "... socket send failed");
			close(s);
			vTaskDelay(2000 / portTICK_PERIOD_MS);
			continue;
		}
		else
		{
			ESP_LOGI(TAG, "Send request to server succeeded");
		}

		struct timeval receiving_timeout;
		receiving_timeout.tv_sec = 2;
		receiving_timeout.tv_usec = 0;
		if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
									 sizeof(receiving_timeout)) < 0)
		{
			ESP_LOGE(TAG, "... failed to set socket receiving timeout");
			close(s);
			vTaskDelay(2000 / portTICK_PERIOD_MS);
			continue;
		}
		recv_buf_len = 0;
		/* Read HTTP response */
		do
		{
			bzero(recv_buf, sizeof(recv_buf));
			r = read(s, recv_buf, sizeof(recv_buf) - 1);
			if (r > 0)
			{
				recv_buf[r] = 0;
				recv_buf_len += r;
			}
		} while (r > 0);
		close(s);
		if (recv_buf_len > 0)
		{
			recv_buf[recv_buf_len] = 0;
			ESP_LOGI(TAG, "Connection closed, all packets received bytes: %d, data:%s", recv_buf_len, recv_buf);
			break;
		}
	}

	free(http_request);
	free(post_data);
	ESP_LOGI(TAG, "Envidado todo");

	wifi_sta_stop();
}

void check_task(void *parm)
{

	while (1)
	{
		pulsador = (SENSOR_TIPE==1)?gpio_get_level(PULSADOR):0;
		bat_baja = (bat_baja == 1 || esp_wifi_get_vdd33() < min_bat) ? 1 : 0;
		if (pulsador != pulsador_old || bat_baja != bat_baja_old)
		{
			ESP_LOGI(TAG, "Pulsador %d, Batería: %d", pulsador, bat_baja);

			est_alarma.batt_vcc = esp_wifi_get_vdd33();
			est_alarma.butt_status = pulsador;
			est_alarma.low_bat = bat_baja;
			est_alarma.sleep_time_sec = sleep_time;
			est_alarma.data[0] = 0;
			est_alarma.sensor_type = SENSOR_TIPE;
			//			gpio_set_level(LED_SWITCH, 1);

			xQueueSend(qhNotif, (void *)&est_alarma, 0);
			sendactive = true;
			if (pulsador == PUL_ON)
			{
				sendcode = 1;
			}
			else
			{
				sendcode = 2;
			}
			xTaskCreate(tx_rf_433_task, "tx_rf_433_task", 2048, (void *)sendcode, 5, NULL);
		}
		pulsador_old = pulsador;
		bat_baja_old = bat_baja;

		ledsw(sendactive, pulsador);

		if ((!sendactive) && (pulsador == PUL_OFF))
		{
			// max 4294967295
			//     3600000000
			//			gpio_set_level(LED_SWITCH, 0);

			if (sleep_time > 0)
			{
				ESP_LOGI(TAG, "Voy a dormir durante %d segundos", sleep_time);
				esp_deep_sleep(sleep_time * 1000000u);
			}
			else
				ESP_LOGI(TAG, "Función sleep desactivada");
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void report_task(void *parm)
{

	estado_t alarma;
	size_t str_len;

	nvs_get_str(handle_config, "server_name", NULL, &str_len);
	nvs_get_str(handle_config, "server_name", SERVER_NAME, &str_len);
	nvs_get_str(handle_config, "server_path", NULL, &str_len);
	nvs_get_str(handle_config, "server_path", SERVER_PATH, &str_len);
	nvs_get_str(handle_config, "server_port", NULL, &str_len);
	nvs_get_str(handle_config, "server_port", SERVER_PORT, &str_len);
	if (strlen(SERVER_PORT) == 0)
		strcpy(SERVER_PORT, "80");

	while (true)
	{
		if (xQueueReceive(qhNotif, &alarma, portMAX_DELAY) != pdFALSE)
		{
			ESP_LOGI(TAG, "Notifico button: %d low_bat:%d", alarma.butt_status, alarma.low_bat);
			send_task(&alarma, radio_always_on);
		}
	}
}


void app_main()
{

	//	esp_log_level_set("*", ESP_LOG_NONE); // Disable
	if (SENSOR_TIPE==2)
		initialise_serial();

	ESP_LOGI(TAG, "SDK version: %s\n", esp_get_idf_version());
	esp_efuse_mac_get_default(chipid);

	ESP_LOGI(TAG, "Dispositivo ID: %X%X%X%X%X%X ", chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);

	//	ESP_LOGI(TAG, "Dispositivo ID: %X",(unsigned int)chipid);

	//	esp_log_level_set("*",ESP_LOG_NONE);

	// Init GPIOS
	gpio_config_t io_in_conf;

	io_in_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_PIN_INTR_POSEDGE;
	io_in_conf.mode = GPIO_MODE_INPUT;
	io_in_conf.pull_up_en = 1;
	io_in_conf.pull_down_en = 0;

	io_in_conf.pin_bit_mask = (1ULL << CONFIGURA);
	gpio_config(&io_in_conf);

	io_in_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_PIN_INTR_POSEDGE;
	io_in_conf.mode = GPIO_MODE_INPUT;
	io_in_conf.pull_up_en = 0;
	io_in_conf.pull_down_en = 0;
	io_in_conf.pin_bit_mask = (1ULL << PULSADOR);
	gpio_config(&io_in_conf);

	io_in_conf.pull_up_en = 0;
	io_in_conf.mode = GPIO_MODE_OUTPUT;
	io_in_conf.pin_bit_mask = (1ULL << LED_SWITCH); //| (1ULL << LED_WORKING);
	gpio_config(&io_in_conf);

	gpio_set_level(LED_SWITCH, 0);
	// Initialize NVS.
	// ESP_ERROR_CHECK(nvs_flash_erase());
	esp_err_t err = nvs_flash_init();

	if (err == ESP_ERR_NVS_NO_FREE_PAGES)
	{
		// OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// If this happens, we erase NVS partition and initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &handle_config));

	nvs_get_u8(handle_config, "radio_always_on", &radio_always_on);
	radio_always_on = (radio_always_on > 0) ? 1 : 0;

	initialise_wifi();

	if (gpio_get_level(CONFIGURA) == 0)
	{ // Entra en modo configuracion
		set_mode_config();
	}
	else
	{
		ESP_LOGI(TAG, "Modo monitoreo");
		qhNotif = xQueueCreate(100, sizeof(estado_t));
		nvs_get_u32(handle_config, "sleep_time", &sleep_time);
		nvs_get_u32(handle_config, "min_bat", &min_bat);

		xTaskCreate(check_task, "check_task", 4096, NULL, 3, NULL);
		xTaskCreate(report_task, "report_task", 4096, NULL, 3, NULL);

		wifi_sta_start();
		if (radio_always_on != 0)
			init_local_http();
	}

	vTaskDelay(10000 / portTICK_PERIOD_MS);

	// Mando a dormir al micro
	const char *data = "\x55\xAA\x00\x02\x00\x01\x04\x06";
	uart_write_bytes(UART_NUM_0, (const char *)data, 8);
}
