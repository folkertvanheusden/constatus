// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>
#include <vector>

class interface;

class instance
{
public:
	instance();
	virtual ~instance();

	std::vector<interface *> interfaces;
	std::string name, descr;
};
