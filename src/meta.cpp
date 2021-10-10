// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <dlfcn.h>
#include <cstring>

#include "meta.h"
#include "utils.h"
#include "error.h"

meta::meta()
{
}

meta::~meta()
{
	for(meta_plugin_t & cur : plugins) {
		cur.uninit_plugin(cur.arg);
		dlclose(cur.handle);
	}

	for(auto it : m_bitmap)
		delete it.second.second;
}

void meta::add_plugin(const std::string & plugin_filename, const std::string & parameter)
{
	meta_plugin_t mp;

	mp.handle = dlopen(plugin_filename.c_str(), RTLD_NOW);
	if (!mp.handle)
		error_exit(true, "Failed opening source plugin library %s: %s", plugin_filename.c_str(), dlerror());

	mp.init_plugin = (init_plugin_t)find_symbol(mp.handle, "init_plugin", "meta plugin", plugin_filename.c_str());
	mp.uninit_plugin = (uninit_plugin_t)find_symbol(mp.handle, "uninit_plugin", "meta plugin", plugin_filename.c_str());

	mp.arg = mp.init_plugin(this, parameter.c_str());

	plugins.push_back(mp);
}

bool meta::get_int(const std::string & key, std::pair<uint64_t, int> *const val) const
{
	std::shared_lock lck(m_int_lock);

	bool rc = true;
	if (auto it = m_int.find(key); it == m_int.end() || (it -> second.first < get_us() && it -> second.first != 0))
		rc = false;
	else
		*val = it -> second;

	return rc;
}

void meta::set_int(const std::string & key, const std::pair<uint64_t, int> & v)
{
	const std::lock_guard<std::shared_mutex> lck(m_int_lock);

	m_int.erase(key);
	m_int.insert(std::pair<std::string, std::pair<uint64_t, int> >(key, v));
}

bool meta::get_double(const std::string & key, std::pair<uint64_t, double> *const val) const
{
	std::shared_lock lck(m_double_lock);

	bool rc = true;
	if (auto it = m_double.find(key); it == m_double.end() || (it -> second.first < get_us() && it -> second.first != 0))
		rc = false;
	else
		*val = it -> second;

	return rc;
}

void meta::set_double(const std::string & key, const std::pair<uint64_t, double> & v)
{
	const std::lock_guard<std::shared_mutex> lck(m_double_lock);

	m_double.erase(key);
	m_double.insert(std::pair<std::string, std::pair<uint64_t, double> >(key, v));
}

bool meta::get_string(const std::string & key, std::pair<uint64_t, std::string> *const val) const
{
	std::shared_lock lck(m_string_lock);

	bool rc = true;
	if (auto it = m_string.find(key); it == m_string.end() || (it -> second.first < get_us() && it -> second.first != 0))
		rc = false;
	else
		*val = it -> second;

	return rc;
}

void meta::set_string(const std::string & key, const std::pair<uint64_t, std::string> & v)
{
	const std::lock_guard<std::shared_mutex> lck(m_string_lock);

	m_string.erase(key);
	m_string.insert(std::pair<std::string, std::pair<uint64_t, std::string> >(key, v));
}

bool meta::get_bitmap(const std::string & key, std::pair<uint64_t, video_frame *> *const val) const
{
	std::shared_lock lck(m_bitmap_lock);

	bool rc = true;
	if (auto it = m_bitmap.find(key); it == m_bitmap.end() || (it -> second.first < get_us() && it -> second.first != 0))
		rc = false;
	else {
		*val = it->second;

		val->second = it->second.second->duplicate({ });
	}

	return rc;
}

void meta::set_bitmap(const std::string & key, const std::pair<uint64_t, video_frame *> & v)
{
	const std::lock_guard<std::shared_mutex> lck(m_bitmap_lock);

	auto it = m_bitmap.find(key);
	if (it != m_bitmap.end()) {
		delete it->second.second;

		m_bitmap.erase(it);
	}

	m_bitmap.insert(std::pair<std::string, std::pair<uint64_t, video_frame *> >(key, v));
}

std::vector<std::string> meta::get_bitmap_keys() const
{
	std::shared_lock lck(m_bitmap_lock);

	std::vector<std::string> out;

	for(auto it : m_bitmap)
		out.push_back(it.first);

	return out;
}
