// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
std::string emit_stats_refresh_js(const std::string & id, const bool fps, const bool cpu, const bool bw, const bool cc);

std::string describe_interface(const instance *const inst, const interface *const i, const bool short_);

bool pause(instance *const cfg, const std::string & which, const bool p);
bool toggle_pause(instance *const cfg, const std::string & which);
bool start_stop(interface *const i, const bool strt);
bool toggle_start_stop(interface *const i);

bool take_a_picture(source *const s, const std::string & snapshot_dir, const int quality, const bool handle_failure);

interface * start_a_video(source *const s, const std::string & snapshot_dir, const int quality, configuration_t *const cfg, const bool is_view_proxy);

bool validate_file(const std::string & snapshot_dir, const bool with_subdirs, const std::string & filename);
