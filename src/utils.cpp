// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <malloc.h>
#include <math.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif

#include "error.h"
#include "log.h"
#include "source.h"
#include "utils.h"

void set_no_delay(int fd, bool use_no_delay)
{
        int flag = use_no_delay;

        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
                error_exit(true, "could not set TCP_NODELAY on socket");
}

int start_listen(const listen_adapter_t & la)
{
	struct sockaddr_in6 server6_addr { 0 };
	size_t server6_addr_len = sizeof server6_addr;

	struct sockaddr_in server_addr { 0 };
	size_t server_addr_len = sizeof server_addr;

	bool is_ipv6 = true;

	server6_addr.sin6_port = htons(la.port);

	if (la.adapter == "0.0.0.0" || la.adapter == "::1") {
		server6_addr.sin6_addr = in6addr_any;
		server6_addr.sin6_family = AF_INET6;
	}
	else {
		if (inet_pton(AF_INET6, la.adapter.c_str(), &server6_addr.sin6_addr) == 0) {
			if (inet_pton(AF_INET, la.adapter.c_str(), &server_addr.sin_addr) == 0)
				error_exit(false, "inet_pton(%s) failed", la.adapter.c_str());
			else {
				server_addr.sin_family = AF_INET;
				server_addr.sin_port = htons(la.port);
				is_ipv6 = false;
			}
		}
		else {
			server6_addr.sin6_family = AF_INET6;
		}
	}

	int fd = socket(is_ipv6 ? AF_INET6 : AF_INET, la.dgram ? SOCK_DGRAM : SOCK_STREAM, 0);
	if (fd == -1)
		error_exit(true, "failed creating socket");

	int reuse_addr = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr)) == -1)
		error_exit(true, "setsockopt(SO_REUSEADDR) failed");

#ifdef TCP_FASTOPEN
	if (la.dgram == false) {
		int qlen = 128;
		if (setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof qlen))
			log(LL_WARNING, "Failed to enable TCP FastOpen");
	}
#endif

	if (bind(fd, is_ipv6 ? (struct sockaddr *)&server6_addr : (struct sockaddr *)&server_addr, is_ipv6 ? server6_addr_len : server_addr_len) == -1)
		error_exit(true, "bind(%s.%d) failed", la.adapter.c_str(), la.port);

	if (la.dgram == false && listen(fd, la.listen_queue_size) == -1)
		error_exit(true, "listen() failed");

	return fd;
}

std::string get_endpoint_name(int fd)
{
	char host[256] { "? " };
	char serv[256] { "? " };
	struct sockaddr_in6 addr { 0 };
	socklen_t addr_len = sizeof addr;

	if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1)
		snprintf(host, sizeof host, "[FAILED TO FIND NAME OF %d: %s (1)]", fd, strerror(errno));
	else
		getnameinfo((struct sockaddr *)&addr, addr_len, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);

	return std::string(host) + "." + std::string(serv);
}

ssize_t READ(int fd, char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc = read(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN) {
				log(LL_DEBUG, "EINTR/EAGAIN %d", errno);
				continue;
			}

			return -1;
		}
		else if (rc == 0)
			break;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

ssize_t WRITE(int fd, const char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc = write(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN) {
				log(LL_DEBUG, "EINTR/EAGAIN %d", errno);
				continue;
			}

			return -1;
		}
		else if (rc == 0)
			return -1;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

uint64_t get_us()
{
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

	return uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_nsec / 1000);
}

std::string myformat(const char *const fmt, ...)
{
	char *buffer = nullptr;
        va_list ap;

        va_start(ap, fmt);
        if (vasprintf(&buffer, fmt, ap) == -1) {
		va_end(ap);
		return "(?)";
	}
        va_end(ap);

	std::string result = buffer;
	free(buffer);

	return result;
}

unsigned char *memstr(unsigned char *haystack, unsigned int haystack_len, unsigned char *needle, unsigned int needle_len)
{
	if (haystack_len < needle_len)
		return nullptr;

	unsigned int pos = 0;
	for(;(haystack_len - pos) >= needle_len;)
	{
		unsigned char *p = (unsigned char *)memchr(&haystack[pos], needle[0], haystack_len - pos);
		if (!p)
			return nullptr;

		pos = (unsigned int)(p - haystack);
		if ((haystack_len - pos) < needle_len)
			break;

		if (memcmp(&haystack[pos], needle, needle_len) == 0)
			return &haystack[pos];

		pos++;
	}

	return nullptr;
}

void set_thread_name(const std::string & name)
{
	std::string full_name = "cs:" + name;

	if (full_name.length() > 15)
		full_name = full_name.substr(0, 15);

	pthread_setname_np(pthread_self(), full_name.c_str());
}

std::string get_thread_name()
{
	char buffer[16];

	pthread_getname_np(pthread_self(), buffer, sizeof buffer);

	return buffer;
}

void mynanosleep(struct timespec req)
{
	struct timespec rem { 0, 0 };

	while((req.tv_nsec || req.tv_sec) && nanosleep(&req, &rem) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			log(LL_DEBUG, "error: %d | %lu %lu", errno, req.tv_sec, req.tv_nsec);
			break;
		}

		log(LL_DEBUG, "EINTR/EAGAIN %d | %lu %lu", errno, req.tv_sec, req.tv_nsec);
		req = rem;
	}
}

void myusleep(long int us)
{
	while(us > 0) {
		long int cus = std::min(us, 999999l);
		struct timespec req { 0, cus * 1000 };

		mynanosleep(req);

		us -= cus;
	}
}

void mysleep(uint64_t slp, std::atomic_bool *const stop_flag, source *const src)
{
	bool unreg = slp >= 1000000;

	if (unreg && src)
		src -> stop();

	while(slp > 0 && !*stop_flag) {
		uint64_t cur = slp > 100000 ? 100000 : slp;
		slp -= cur;

		struct timespec req { 0, long(cur * 1000l) };
		mynanosleep(req);
	}

	if (unreg && src)
		src -> start();
}

void *find_symbol(void *library, const char *const symbol, const char *const what, const char *const library_name)
{
	void *ret = dlsym(library, symbol);

	if (!ret)
		error_exit(true, "Failed finding %s \"%s\" in %s: %s", what, symbol, library_name, dlerror());

	return ret;
}

std::string un_url_escape(const std::string & in)
{
	CURL *ch = curl_easy_init();
	if (!ch)
		error_exit(false, "Failed to initialize CURL session");

	char *temp = curl_easy_unescape(ch, in.c_str(), 0, nullptr);
	std::string out = temp;

	curl_free(temp);
	curl_easy_cleanup(ch);

	return out;
}

std::string url_escape(const std::string & in)
{
	CURL *ch = curl_easy_init();
	if (!ch)
		error_exit(false, "Failed to initialize CURL session");

	char *temp = curl_easy_escape(ch, in.c_str(), in.size());
	std::string out = temp;

	curl_free(temp);
	curl_easy_cleanup(ch);

	return out;
}

std::vector<std::string> * split(std::string in, std::string splitter)
{
	std::vector<std::string> *out = new std::vector<std::string>;
	size_t splitter_size = splitter.size();

	for(;;)
	{
		size_t pos = in.find(splitter);
		if (pos == std::string::npos)
			break;

		std::string before = in.substr(0, pos);
		out -> push_back(before);

		size_t bytes_left = in.size() - (pos + splitter_size);
		if (bytes_left == 0)
		{
			out -> push_back("");
			return out;
		}

		in = in.substr(pos + splitter_size);
	}

	if (in.size() > 0)
		out -> push_back(in);

	return out;
}

std::string search_replace(const std::string & in, const std::string & search, const std::string & replace)
{
	std::string work = in;
	std::string::size_type pos = 0u;

	while((pos = work.find(search, pos)) != std::string::npos) {
		work.replace(pos, search.length(), replace);
		pos += replace.length();
	}

	return work;
}

std::vector<file_t> * load_filelist(const std::string & dir, const bool with_subdirs, const std::string & prefix)
{
	auto *out = new std::vector<file_t>;

	DIR *d = opendir(dir.c_str());
	if (!d)
		return out;

	for(;;) {
		struct dirent *de = readdir(d);
		if (!de)
			break;

		std::string name = de->d_name;

		struct stat st;
		if (fstatat(dirfd(d), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) == -1) {
			log(LL_WARNING, "fstatat failed for %s: %s", name.c_str(), strerror(errno));
			continue;
		}

		if (S_ISDIR(st.st_mode) && with_subdirs && name != "." && name != "..") {
			std::vector<file_t> *subdir = load_filelist(dir + "/" + name, with_subdirs, prefix);

			for(auto f : *subdir)
				out->push_back({ name + "/" + f.name, f.last_change, f.size });

			delete subdir;

			continue;
		}

		if (!S_ISREG(st.st_mode))
			continue;

		if (strncmp(name.c_str(), prefix.c_str(), prefix.size()) == 0) {
			file_t f = { name, st.st_mtim.tv_sec, st.st_size };

			out -> push_back(f);
		}
	}

	closedir(d);

	return out;
}

std::string myctime(const time_t t)
{
	struct tm ptm;
	localtime_r(&t, &ptm);

	char buffer[4096];
	strftime(buffer, sizeof buffer, "%c", &ptm);

	return buffer;
}

int connect_to(std::string hostname, int port, std::atomic_bool *abort)
{
	std::string portstr = myformat("%d", port);
	int fd = -1;

        struct addrinfo hints{ 0 };
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        struct addrinfo* result = nullptr;
        int rc = getaddrinfo(hostname.c_str(), portstr.c_str(), &hints, &result);
        if (rc) {
                log(LL_ERR, "Cannot resolve host name %s: %s", hostname.c_str(), gai_strerror(rc));
		freeaddrinfo(result);
		return -1;
        }

	for (struct addrinfo *rp = result; rp != nullptr; rp = rp -> ai_next) {
		fd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
		if (fd == -1)
			continue;

		int old_flags = fcntl(fd, F_GETFL, 0);
		if (old_flags == -1)
			error_exit(true, "fcntl failed");
		if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
			error_exit(true, "fcntl failed");

		/* wait for connection */
		/* connect to peer */
		if (connect(fd, rp -> ai_addr, rp -> ai_addrlen) == 0) {
			/* connection made, return */
			(void)fcntl(fd, F_SETFL, old_flags);
			freeaddrinfo(result);
			return fd;
		}

		for(;;) {
			fd_set wfds;
			FD_ZERO(&wfds);
			FD_SET(fd, &wfds);

			struct timeval tv { 0, 100 * 1000 };

			/* wait for connection */
			rc = select(fd + 1, nullptr, &wfds, nullptr, &tv);
			if (rc == 0)	// time out
			{
				// timeout is handled implicitly in this loop
			}
			else if (rc == -1)	// error
			{
				if (errno == EINTR || errno == EAGAIN || errno == EINPROGRESS)
					continue;

				log(LL_ERR, "Select failed during connect to %s: %s", hostname.c_str(), strerror(errno));
				break;
			}
			else if (FD_ISSET(fd, &wfds))
			{
				int optval=0;
				socklen_t optvallen = sizeof(optval);

				/* see if the connect succeeded or failed */
				getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optvallen);

				/* no error? */
				if (optval == 0)
				{
					fcntl(fd, F_SETFL, old_flags);
					freeaddrinfo(result);
					return fd;
				}
			}

			if (abort && abort -> exchange(false))
			{
				log(LL_INFO, "Connect to %s:%d aborted", hostname.c_str(), port);
				abort -> store(true);
				close(fd);
				freeaddrinfo(result);
				return -1;
			}
		}

		close(fd);
		fd = -1;
	}

	freeaddrinfo(result);

	return fd;
}

long int hash(const std::string & what)
{
	return std::hash<std::string>{}(what);
}

void * allocate_0x00(const size_t size)
{
	return calloc(1, size);
}

void * duplicate(const void *const org, const size_t size)
{
	void *temp = malloc(size);
	memcpy(temp, org, size);

	return temp;
}

void * duplicate(const void *const org, const size_t size, const int line_len, const int padding)
{
	uint8_t *out = (uint8_t *)malloc(size), *out_work = out, *org_work = (uint8_t *)org;
	size_t work_size = size;

	const int stride = line_len + padding;

	while(work_size > 0) {
		memcpy(out_work, org_work, line_len);

		org_work += stride;
		out_work += line_len;

		work_size -= stride;
	}

	return out;
}

// NOTE: requires a path ending in / (or filename)
void create_path(const std::string & filename)
{
	size_t last_slash = filename.rfind('/'); // windows users may want to replace this by a backslash
	if (last_slash == std::string::npos) // no path
		return;

	std::string path_only = filename.substr(0, last_slash);
	size_t pos = 0;

	for(;;) {
		pos = path_only.find('/', pos + 1);

		std::string cur = pos == std::string::npos ? path_only : path_only.substr(0, pos);

		std::filesystem::path p(cur);

		if (std::filesystem::exists(p) == false) {
			try {
				if (!std::filesystem::create_directories(p))
					log(LL_ERR, "Problem creating directory %s", cur.c_str());
			}
			catch(const std::filesystem::filesystem_error & e) {
				log(LL_ERR, "Problem creating directory %s: %s", cur.c_str(), e.what());
			}
		}

		if (pos == std::string::npos)
			break;
	}
}

std::string str_tolower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });

	return s;
}

std::string base64_decode(const std::string & in)
{
	const std::string base64_characters{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
	int i = 0;
	unsigned char buffer[4] { 0 };
	std::string out;

	for(size_t pos=0; pos<in.size(); pos++) {
		if (in[pos] == '=')
			break;

		buffer[i++] = in[pos];

		if (i == 4) {
			for(int idx = 0; idx<4; idx++)
				buffer[idx] = base64_characters.find(buffer[idx]);

			out += (buffer[0] << 2) + ((buffer[1] & 0x30) >> 4);
			out += ((buffer[1] & 0xf) << 4) + ((buffer[2] & 0x3c) >> 2);
			out += ((buffer[2] & 0x3) << 6) + buffer[3];

			i = 0;
		}
	}

	if (i) {
		char buffer2[3]{ 0 };

		for(int idx=i; idx<4; idx++)
			buffer[idx] = 0;

		for(int idx=0; idx<4; idx++)
			buffer[idx] = base64_characters.find(buffer[idx]);

		buffer2[0] = (buffer[0] << 2) + ((buffer[1] & 0x30) >> 4);
		buffer2[1] = ((buffer[1] & 0xf) << 4) + ((buffer[2] & 0x3c) >> 2);
		buffer2[2] = ((buffer[2] & 0x3) << 6) + buffer[3];

		for(int idx=0; idx < i - 1; idx++)
			out += buffer2[idx];
	}

	return out;
}

void dump_stacktrace()
{
#if defined(__GLIBC__)
        void *trace[128] { nullptr };
        int trace_size = backtrace(trace, 128);
        char **messages = backtrace_symbols(trace, trace_size);
#endif

#if defined(__GLIBC__)
	log(LL_INFO, "Execution path:");

	for(int index=0; index<trace_size; ++index)
		log(LL_INFO, "\t%d %s", index, messages[index]);
#endif
}

double hue_to_rgb(const double m1, const double m2, double h)
{
	while(h < 0.0)
		h += 1.0;

	while(h > 1.0)
		h -= 1.0;

	if (6.0 * h < 1.0)
		return (m1 + (m2 - m1) * h * 6.0);

	if (2.0 * h < 1.0)
		return m2;

	if (3.0 * h < 2.0)
		return (m1 + (m2 - m1) * ((2.0 / 3.0) - h) * 6.0);

	return m1;
}

void hls_to_rgb(const double H, const double L, const double S, double *const r, double *const g, double *const b)
{
	if (S == 0) {
		*r = *g = *b = L;
		return;
	}

	double m2 = 0;

	if (L <= 0.5)
		m2 = L * (1.0 + S);
	else
		m2 = L + S - L * S;

	double m1 = 2.0 * L - m2;

	*r = hue_to_rgb(m1, m2, H + 1.0/3.0);
	*g = hue_to_rgb(m1, m2, H);
	*b = hue_to_rgb(m1, m2, H - 1.0/3.0);
}

void rgb_to_hls(const int R, const int G, const int B, double *const h, double *const s, double *const l)
{
	// Make r, g, and b fractions of 1
	double r = R / 255.0;
	double g = G / 255.0;
	double b = B / 255.0;

	// Find greatest and smallest channel values
	double cmin = std::min(r, std::min(g, b));
	double cmax = std::max(r, std::max(g, b));
	double delta = cmax - cmin;

	*h = *s = *l = 0.0;

	// Calculate hue
	// No difference
	if (delta == 0)
		*h = 0;
	// Red is max
	else if (cmax == r)
		*h = fmod((g - b) / delta, 6.0);
	// Green is max
	else if (cmax == g)
		*h = (b - r) / delta + 2;
	// Blue is max
	else
		*h = (r - g) / delta + 4;

	*h *= 60.0;

	// Make negative hues positive behind 360°
	if (*h < 0)
		*h += 360.0;

	// Calculate lightness
	*l = (cmax + cmin) / 2;

	// Calculate saturation
	*s = delta == 0 ? 0 : delta / (1 - fabs(2 * *l - 1));
}

int hex_to_val(char c)
{
	c = tolower(c);

	if (c >= 'a')
		return c - 'a' + 10;

	return c - '0';
}

void hex_str_to_rgb(const std::string & in, uint8_t *const r, uint8_t *const g, uint8_t *const b)
{
	if (in.size() >= 6) {
		std::string work = in.size() == 7 ? in.substr(1) : in;

		*r = (hex_to_val(work.at(0)) << 4) | hex_to_val(work.at(1));
		*g = (hex_to_val(work.at(2)) << 4) | hex_to_val(work.at(3));
		*b = (hex_to_val(work.at(4)) << 4) | hex_to_val(work.at(5));
	}
}

std::string get_file_size(const off_t size)
{
	constexpr off_t size_kB = 1024;
	constexpr off_t size_MB = 1024 * 1024;
	constexpr off_t size_GB = 1024 * 1024 * 1024;
	constexpr off_t size_TB = 1024ll * 1024ll * 1024ll * 1024ll;

	if (size < size_kB)
		return myformat("%ld", size);

	if (size < size_MB)
		return myformat("%ldkB", (size + size_kB - 1) / size_kB);

	if (size < size_GB)
		return myformat("%ldMB", (size + size_MB - 1) / size_MB);

	if (size < size_TB)
		return myformat("%ldGB", (size + size_GB - 1) / size_GB);

	return "overflow";
}

std::string get_file_size(const std::string & filename)
{
	struct stat st;

	if (stat(filename.c_str(), &st) == -1)
		return "-";

	return get_file_size(st.st_size);
}

std::pair<std::string, std::string> get_file_age(const time_t t)
{
	struct tm timeinfo;
	localtime_r(&t, &timeinfo);

	char buffer[256] { 0 };
	strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", &timeinfo);

	std::string full_format = buffer;

	unsigned long int age = time(nullptr) - t;

	int seconds = age;
	int minutes = age / 60;
	int hours = age / 3600;
	int days = age / 86400;
	int weeks = age / 604800;
	int months = age / 2600640;
	int years = age / 31207680;

	if (seconds <= 60)
		return { full_format, myformat("%d seconds", seconds) };

	if (minutes <= 60)
		return { full_format, myformat("%d minutes", minutes) };

	if (hours <= 24)
		return { full_format, myformat("%d hours", hours) };

	if (days <= 7)
		return { full_format, myformat("%d days", days) };

	if (weeks <= 4)
		return { full_format, myformat("%d weeks", weeks) };

	if (months <= 12)
		return { full_format, myformat("%d months", months) };

	return { full_format, myformat("%d years", years) };
}

std::pair<std::string, std::string> get_file_age(const std::string & filename)
{
	struct stat st;

	if (stat(filename.c_str(), &st) == -1)
		return { strerror(errno), "-" };

	return get_file_age(st.st_mtime);
}

std::string bin_to_hex(const uint8_t *const in, const size_t n)
{
	std::string out;

	for(size_t i=0; i<n; i++)
		out += myformat("%02x", in[i]);

	return out;
}

uint8_t *gen_random(size_t n)
{
	uint8_t *out = (uint8_t *)malloc(n);

	int fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		error_exit(true, "Cannot open random device");

	uint8_t *p = out;
	while(n) {
		int rc = read(fd, p, n);
		if (rc <= 0) {
			if (errno == EAGAIN)
				continue;

			error_exit(true, "Cannot read from random device");
		}

		p += rc;
		n -= rc;
	}

	close(fd);

	return out;
}

bool compare_equal_wo_case(const std::string & a, const std::string & b)
{
	if (a.size() != b.size())
		return false;

	return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) {
                          return tolower(a) == tolower(b);
                      });
}

void handle_fps(std::atomic_bool *stop_flag, const double fps, const uint64_t frame_start)
{
	if (fps <= 0.)
		return;

	uint64_t after_ts = get_us();
	uint64_t frame_took = after_ts - frame_start;
	int64_t sleep_left = 1000000 / fps - frame_took;
	log(LL_DEBUG, "frame took %f, sleep left %f", frame_took / 1000000.0, sleep_left / 1000000.0);

	if (sleep_left > 0)
		mysleep(sleep_left, stop_flag, nullptr);
}

std::string substr(const UChar32 *const utf32_str, const int idx, const int n)
{
        std::string out;

        for(int i=idx; i<idx + n; i++)
                out += utf32_str[i];

        return out;
}
