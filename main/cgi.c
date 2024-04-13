/*
Some random cgi routines. Used in the LED example and the page that returns the entire
flash as a binary. Also handles the hit counter on the main page.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <libesphttpd/httpd.h>
#include "cgi.h"
#include "io.h"
#include <../../main/pulsa_inalam.h>
#include "esp_log.h"
#include "esp_system.h"

//cause I can't be bothered to write an ioGetLed()
static char currLedState=0;

//Cgi that turns the LED on or off according to the 'led' param in the POST data
CgiStatus ICACHE_FLASH_ATTR cgiLed(HttpdConnData *connData) {
	int len;
	char buff[1024];

	if (connData->isConnectionClosed) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->post.buff, "led", buff, sizeof(buff));
	if (len!=0) {
		currLedState=atoi(buff);
		ioLed(currLedState);
	}

	httpdRedirect(connData, "led.tpl");
	return HTTPD_CGI_DONE;
}


CgiStatus ICACHE_FLASH_ATTR cgiFlash(HttpdConnData *connData) {
//	uint32_t int_val;
	int8_t len;
	char buff[512];

	if (connData->isConnectionClosed) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->post.buff, "boton", buff, sizeof(buff));
	if (len>0) {
		ESP_LOGI("HTTPD","Encontré boton len: %d, data: %s",len,buff);
	
		printf("RES: %s - Len:%d\n",buff, len);

		if (memcmp(buff,"ota",len-1)==0) {
//			xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
		} else
		if (memcmp(buff,"reset",len-1)==0) {}
			//esp_restartBOOL=true; // para ejecutar
			esp_restart();

	}

	len=httpdFindArg(connData->post.buff, "ota_uri", buff, sizeof(buff));
	if (len>0) {
	    nvs_set_str(handle_config, "ota_uri", buff);
	}

	httpdRedirect(connData, "flash.tpl");
	return HTTPD_CGI_DONE;
}

CgiStatus ICACHE_FLASH_ATTR tplFlash(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	//uint32_t sleep_time,min_bat;
	//uint8_t radio_always_on;
	size_t str_len;

	if (token==NULL) return HTTPD_CGI_DONE;

	strcpy(buff, "Unknown");
	if (strcmp(token, "serial_number")==0) {
		sprintf(buff, "%X%X%X%X%X%X",chipid[0],chipid[1],chipid[2],chipid[3],chipid[4],chipid[5]);
	}

	if (strcmp(token, "ota_uri")==0) {
		nvs_get_str(handle_config,"ota_uri",NULL,&str_len);
		if (str_len<128 && str_len>0){
			nvs_get_str(handle_config,"ota_uri",buff,&str_len);
//			ESP_LOGI("debug", "server_name len %d Valor:%s", server_name_length, buff);
		} else {
			strcpy(buff,"firmware.local");
		}
	}

  for (size_t i = 0; i < 80; i++)
	{
		/* code */
		httpdSend(connData, buff, -1);

	}
	
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}



CgiStatus ICACHE_FLASH_ATTR cgiConfig(HttpdConnData *connData) {
	uint32_t int_val;
	int len;
	char buff[1024];

	if (connData->isConnectionClosed) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->post.buff, "sleep_time", buff, sizeof(buff));
	if (len>0) {
		int_val=atol(buff);
	    nvs_set_u32(handle_config, "sleep_time", int_val);
	}

	len=httpdFindArg(connData->post.buff, "min_bat", buff, sizeof(buff));
	if (len>0) {
		int_val=atol(buff);
	    nvs_set_u32(handle_config, "min_bat", int_val);
	}

	len=httpdFindArg(connData->post.buff, "radio_always_on", buff, sizeof(buff));
	if (len>0) {
		int_val=atol(buff);
	    nvs_set_u8(handle_config, "radio_always_on", int_val);
	}

	len=httpdFindArg(connData->post.buff, "server_port", buff, sizeof(buff));
	if (len>0) {
	    nvs_set_str(handle_config, "server_port", buff);
	}

	len=httpdFindArg(connData->post.buff, "server_name", buff, sizeof(buff));
	if (len>0) {
	    nvs_set_str(handle_config, "server_name", buff);
	}

	len=httpdFindArg(connData->post.buff, "server_path", buff, sizeof(buff));
	if (len>0) {
	    nvs_set_str(handle_config, "server_path", buff);
	}

	httpdRedirect(connData, "config.tpl");
	return HTTPD_CGI_DONE;
}


//Template code for the led page.
CgiStatus ICACHE_FLASH_ATTR tplConfig(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	uint32_t sleep_time,min_bat;
	uint8_t radio_always_on;
	size_t str_len;

	if (token==NULL) return HTTPD_CGI_DONE;

	strcpy(buff, "Unknown");
	if (strcmp(token, "sleep_time")==0) {
		nvs_get_u32(handle_config, "sleep_time", &sleep_time);
		sprintf(buff, "%d", sleep_time);
	}

	if (strcmp(token, "min_bat")==0) {
		nvs_get_u32(handle_config, "min_bat", &min_bat);
		sprintf(buff, "%u", min_bat);
	}

	if (strcmp(token, "radio_always_on")==0) {
		nvs_get_u8(handle_config, "radio_always_on", &radio_always_on);
		sprintf(buff, "%d", radio_always_on);
	}

	if (strcmp(token, "serial_number")==0) {
		sprintf(buff, "%X%X%X%X%X%X",chipid[0],chipid[1],chipid[2],chipid[3],chipid[4],chipid[5]);
	}

	if (strcmp(token, "server_port")==0) {
		nvs_get_str(handle_config,"server_port",NULL,&str_len);
		if (str_len<128 && str_len>0){
			nvs_get_str(handle_config,"server_port",buff,&str_len);
//			ESP_LOGI("debug", "server_name len %d Valor:%s", server_name_length, buff);
		} else {
			strcpy(buff,"80");
		}
	}



	if (strcmp(token, "server_name")==0) {
		nvs_get_str(handle_config,"server_name",NULL,&str_len);
		if (str_len<128 && str_len>0){
			nvs_get_str(handle_config,"server_name",buff,&str_len);
//			ESP_LOGI("debug", "server_name len %d Valor:%s", server_name_length, buff);
		} else {
			strcpy(buff,"pepamon.local");
		}
	}

	if (strcmp(token, "server_path")==0) {
		nvs_get_str(handle_config,"server_path",NULL,&str_len);
		if (str_len<128 && str_len>0){
			nvs_get_str(handle_config,"server_path",buff,&str_len);
//			ESP_LOGI("debug", "server_name len %d Valor:%s", server_name_length, buff);
		} else {
			strcpy(buff,"/api/v1/movieventos/evento");
		}
	}

	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

//Template code for the led page.
CgiStatus ICACHE_FLASH_ATTR tplLed(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	strcpy(buff, "Unknown");
	if (strcmp(token, "ledstate")==0) {
		if (currLedState) {
			strcpy(buff, "on");
		} else {
			strcpy(buff, "off");
		}
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

static int hitCounter=0;

//Template code for the counter on the index page.
CgiStatus ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	if (strcmp(token, "counter")==0) {
		hitCounter++;
		sprintf(buff, "%d", hitCounter);
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}
