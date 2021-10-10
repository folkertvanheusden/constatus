// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
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
