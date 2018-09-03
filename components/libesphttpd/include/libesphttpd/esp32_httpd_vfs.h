#pragma once

//This is a catch-all cgi function. It takes the url passed to it, looks up the corresponding
//path in the filesystem and if it exists, passes the file through. This simulates what a normal
//webserver would do with static files.
//
// The cgiArg value is the base directory path, if specified.
//
// Usage:
//      ROUTE_CGI("*", cgiEspVfsHook) or
//      ROUTE_CGI_ARG("*", cgiEspVfsHook, "/base/directory/") or
//      ROUTE_CGI_ARG("*", cgiEspVfsHook, ".") to use the current working directory
CgiStatus ICACHE_FLASH_ATTR cgiEspVfsHook(HttpdConnData *connData);
