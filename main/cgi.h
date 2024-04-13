#ifndef CGI_H
#define CGI_H
#ifdef __cplusplus
extern "C" {
#endif


//#include "libesphttpd/httpd.h"

CgiStatus cgiLed(HttpdConnData *connData);
CgiStatus tplLed(HttpdConnData *connData, char *token, void **arg);
CgiStatus cgiConfig(HttpdConnData *connData);
CgiStatus tplConfig(HttpdConnData *connData, char *token, void **arg);
CgiStatus cgiFlash(HttpdConnData *connData);
CgiStatus tplFlash(HttpdConnData *connData, char *token, void **arg);

CgiStatus tplCounter(HttpdConnData *connData, char *token, void **arg);


#ifdef __cplusplus
}
#endif

#endif
