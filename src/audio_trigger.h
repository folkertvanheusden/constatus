// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "config.h"
#include <atomic>
#include <stdlib.h>

#include "cfg.h"
#include "interface.h"

class audio;
class source;

class audio_trigger : public interface
{
protected:
	instance *const i;
	source *const s;
	audio *const a;
	const int threshold;
	const size_t min_n_triggers;

	const std::vector<std::string> trigger_targets;

	std::vector<interface *> event_clients;

	void register_notifiers();
	void unregister_notifiers();

public:
	audio_trigger(const std::string & id, const std::string & descr, instance *const i, source *const s, const int threshold, const size_t min_n_triggers, const std::vector<std::string> & trigger_targets);
	virtual ~audio_trigger();

	virtual void operator()() override;
};
