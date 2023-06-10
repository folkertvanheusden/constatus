// (C) 2017-2023 by folkert van heusden, released under the MIT license
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
