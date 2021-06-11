// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <assert.h>
#include <string>

#include "parameters.h"
#include "log.h"

parameter::parameter(const std::string & name, const std::string & descr, const bool v) : type(V_BOOL), name(name), descr(descr), vb(v)
{
}

parameter::parameter(const std::string & name, const std::string & descr, const int v) : type(V_INT), name(name), descr(descr), vi(v)
{
}

parameter::parameter(const std::string & name, const std::string & descr, const double v) : type(V_DOUBLE), name(name), descr(descr), vd(v)
{
}

parameter::parameter(const std::string & name, const std::string & descr, const std::string & v) : type(V_STRING), name(name), descr(descr), vs(v)
{
}

parameter::~parameter()
{
}

bool parameter::get_value_bool() const
{
	assert(type == V_BOOL);

	lock.lock_shared();
	bool rc = vb;
	lock.unlock_shared();

	return rc;
}

int parameter::get_value_int() const
{
	assert(type == V_INT);

	lock.lock_shared();
	int rc = vi;
	lock.unlock_shared();

	return rc;
}

double parameter::get_value_double() const
{
	assert(type == V_DOUBLE);

	lock.lock_shared();
	double rc = vd;
	lock.unlock_shared();

	return rc;
}

std::string parameter::get_value_string() const
{
	assert(type == V_STRING);

	lock.lock_shared();
	std::string rc = vs;
	lock.unlock_shared();

	return rc;
}

void parameter::set_value(const bool new_value)
{
	assert(type == V_BOOL);

	lock.lock();
	vb = new_value;
	lock.unlock();
}


void parameter::set_value(const int new_value)
{
	assert(type == V_INT);

	lock.lock();
	vi = new_value;
	lock.unlock();
}

void parameter::set_value(const double new_value)
{
	assert(type == V_DOUBLE);

	lock.lock();
	vd = new_value;
	lock.unlock();
}

void parameter::set_value(const std::string & new_value)
{
	assert(type == V_STRING);

	lock.lock();
	vs = new_value;
	lock.unlock();
}

bool parameter::get_value_bool(const std::map<std::string, parameter *> & in, const std::string & key)
{
	auto it = in.find(key);

	if (it != in.end())
		return it->second->get_value_bool();

	log(LL_ERR, "Internal error: \"%s\" not found", key.c_str());
	assert(false);

	return false;
}

int parameter::get_value_int(const std::map<std::string, parameter *> & in, const std::string & key)
{
	auto it = in.find(key);

	if (it != in.end())
		return it->second->get_value_int();

	log(LL_ERR, "Internal error: \"%s\" not found", key.c_str());
	assert(false);

	return -1;
}

double parameter::get_value_double(const std::map<std::string, parameter *> & in, const std::string & key)
{
	auto it = in.find(key);

	if (it != in.end())
		return it->second->get_value_double();

	log(LL_ERR, "Internal error: \"%s\" not found", key.c_str());
	assert(false);

	return -1.0;
}

std::string parameter::get_value_string(const std::map<std::string, parameter *> & in, const std::string & key)
{
	auto it = in.find(key);

	if (it != in.end())
		return it->second->get_value_string();

	log(LL_ERR, "Internal error: \"%s\" not found", key.c_str());
	assert(false);

	return "";
}

void parameter::add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const bool v)
{
	to.insert(std::pair<std::string, parameter *>(key, new parameter(key, descr, v)));
}

void parameter::add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const int v)
{
	to.insert(std::pair<std::string, parameter *>(key, new parameter(key, descr, v)));
}

void parameter::add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const double v)
{
	to.insert(std::pair<std::string, parameter *>(key, new parameter(key, descr, v)));
}

void parameter::add_value(std::map<std::string, parameter *> & to, const std::string & key, const std::string & descr, const std::string & v)
{
	to.insert(std::pair<std::string, parameter *>(key, new parameter(key, descr, v)));
}
