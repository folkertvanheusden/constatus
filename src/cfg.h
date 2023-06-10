// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "failure.h"
#include "stats_tracker.h"
#include "gen.h"
#include "utils.h"

class cleaner;
class feed;
class instance;
class interface;
class resize;
class view;

typedef struct
{
	std::string              search_path;

	std::vector<instance *>  instances;
	mutable std::timed_mutex lock;

	std::string              cfg_file;

	resize                  *r;

	std::vector<interface *> global_http_servers;

	std::vector<interface *> guis;

	std::string db_url, db_user, db_pass;
	cleaner                 *clnr;

	std::vector<interface *> announcers;

	std::map<std::string, feed *> text_feeds;

	stats_tracker           *st;

	listen_adapter_t         nrpe;
} configuration_t;

class source;

void find_by_id(const configuration_t *const cfg, const std::string & id, instance **inst, interface **i);

source *find_source(instance *const inst);

view *find_view(instance *const inst);
view *find_view_by_child(const configuration_t *const cfg, const std::string & child_id);
view *find_view(instance *const views, const std::string & id);

std::vector<interface *> find_motion_triggers(instance *const inst);
bool check_for_motion(instance *const inst);

instance *find_instance_by_interface(const configuration_t *const cfg, const interface *i_f);
instance *find_instance_by_name(const configuration_t *const cfg, const std::string & name);

interface *find_interface_by_id(instance *const inst, const std::string & id);
interface *find_interface_by_id(configuration_t *const cfg, const std::string & id);

bool load_configuration(configuration_t *const cfg, const bool verbose, const int ll);

std::optional<std::pair<std::string, error_state_t> > find_errors(configuration_t *const cfg);
std::optional<std::pair<std::string, error_state_t> > find_errors(instance *const inst);

failure_t default_failure();
