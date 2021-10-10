// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <atomic>
#include <string>
#include <time.h>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>

class source;

typedef struct {
	std::string adapter;
	int port;
	int listen_queue_size;
	bool dgram;
} listen_adapter_t;

void set_no_delay(int fd, bool use_no_delay);
int start_listen(const listen_adapter_t & la);
std::string get_endpoint_name(int fd);
ssize_t READ(int fd, char *whereto, size_t len);
ssize_t WRITE(int fd, const char *whereto, size_t len);
uint64_t get_us();
std::string myformat(const char *const fmt, ...);
unsigned char *memstr(unsigned char *haystack, unsigned int haystack_len, unsigned char *needle, unsigned int needle_len);
void set_thread_name(const std::string & name);
std::string get_thread_name();
void mysleep(uint64_t slp, std::atomic_bool *const stop_flag, source *const s);
void *find_symbol(void *library, const char *const symbol, const char *const what, const char *const library_name);
std::string un_url_escape(const std::string & in);
std::string url_escape(const std::string & in);
std::vector<std::string> * split(std::string in, std::string splitter);
std::string myctime(const time_t t);
std::string search_replace(const std::string & in, const std::string & search, const std::string & replace);
long int hash(const std::string & what);
bool compare_equal_wo_case(const std::string & a, const std::string & b);

typedef struct
{
	std::string name;
	time_t last_change;
	off_t size;
} file_t;
std::vector<file_t> * load_filelist(const std::string & dir, const bool with_subdirs, const std::string & prefix);

int connect_to(std::string hostname, int port, std::atomic_bool *abort);

void * allocate_0x00(const size_t size);
void * duplicate(const void *const org, const size_t size);
void * duplicate(const void *const org, const size_t size, const int line_len /* bytes */, const int padding); // padding = stride - xres * Bpp

void mynanosleep(struct timespec req);
void myusleep(long int us);

void create_path(const std::string & filename);

std::string str_tolower(std::string s);
std::string base64_decode(const std::string & in);

void dump_stacktrace();

void rgb_to_hls(const int R, const int G, const int B, double *const h, double *const s, double *const l);
void hls_to_rgb(const double H, const double L, const double S, double *const r, double *const g, double *const b);
void hex_str_to_rgb(const std::string & in, uint8_t *const r, uint8_t *const g, uint8_t *const b);

std::string get_file_size(const off_t size);
std::string get_file_size(const std::string & filename);
std::pair<std::string, std::string> get_file_age(const time_t t);
std::pair<std::string, std::string> get_file_age(const std::string & filename);

std::string bin_to_hex(const uint8_t *const in, const size_t n);

uint8_t *gen_random(size_t n);

void handle_fps(std::atomic_bool *stop_flag, const double fps, const double frame_start);
