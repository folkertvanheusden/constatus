// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <string>
#include <curl/curl.h>

#define LL_FATAL	0
#define LL_ERR		1
#define LL_WARNING	2
#define LL_INFO		3
#define LL_DEBUG	4
#define LL_DEBUG_VERBOSE	5

std::string ll_to_str(const int ll);
int get_default_loglevel();
void setlogfile(const char *const other, const int loglevel);
void log(const int loglevel, const char *const what, ...);
void lognee(const int loglevel, const char *const what, ...);
void log(const std::string & id, const int loglevel, const char *const what, ...);
void log(const int loglevel, const std::string & what, ...);
int curl_log(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);
