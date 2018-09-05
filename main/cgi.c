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


#include <libesphttpd/esp.h>
#include "cgi.h"
#include "io.h"
#include <../../main/pulsa_inalam.h>

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


CgiStatus ICACHE_FLASH_ATTR cgiConfig(HttpdConnData *connData) {
	uint32_t sleep_time;
	int len;
	char buff[1024];

	if (connData->isConnectionClosed) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->post.buff, "sleep_time", buff, sizeof(buff));
	if (len!=0) {
		sleep_time=atol(buff);
	    nvs_set_u32(handle_config, "sleep_time", sleep_time);
	}



	httpdRedirect(connData, "config.tpl");
	return HTTPD_CGI_DONE;
}


//Template code for the led page.
CgiStatus ICACHE_FLASH_ATTR tplConfig(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	uint32_t sleep_time;
	if (token==NULL) return HTTPD_CGI_DONE;

	strcpy(buff, "Unknown");
	if (strcmp(token, "sleep_time")==0) {
		nvs_get_u32(handle_config, "sleep_time", &sleep_time);
		sprintf(buff, "%d", sleep_time);
	}

	if (strcmp(token, "serial_number")==0) {
		sprintf(buff, "%X", (unsigned int)chipid);
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
