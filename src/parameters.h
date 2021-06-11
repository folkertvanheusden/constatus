// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <map>
#include <shared_mutex>
#include <string>

typedef enum { V_BOOL, V_INT, V_DOUBLE, V_STRING } par_value_t;

class parameter
{
private:
	mutable std::shared_mutex lock;
	const par_value_t type;
	const std::string name, descr;

	bool vb { false };
	double vd { 0. };
	int vi { 0 };
	std::string vs;

public:
	parameter(const std::string & name, const std::string & descr, const bool v);
	parameter(const std::string & name, const std::string & descr, const int v);
	parameter(const std::string & name, const std::string & descr, const double v);
	parameter(const std::string & name, const std::string & descr, const std::string & v);
	virtual ~parameter();

	std::string get_name() const { return name; }
	std::string get_descr() const { return descr; }

	par_value_t get_type() const { return type; }

	bool get_value_bool() const;
	int get_value_int() const;
	double get_value_double() const;
	std::string get_value_string() const;

	void set_value(const bool new_value);
	void set_value(const int new_value);
	void set_value(const double new_value);
	void set_value(const std::string & new_value);

	static bool get_value_bool(const std::map<std::string, parameter *> & in, const std::string & key);
	static int get_value_int(const std::map<std::string, parameter *> & in, const std::string & key);
	static double get_value_double(const std::map<std::string, parameter *> & in, const std::string & key);
	static std::string get_value_string(const std::map<std::string, parameter *> & in, const std::string & key);

	static void add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const bool v);
	static void add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const int v);
	static void add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const double v);
	static void add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const std::string & v);
};
