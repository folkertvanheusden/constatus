// (C) 2017-2023 by folkert van heusden, released under the MIT license

#include "announce_upnp.h"
#if HAVE_RYGEL == 1
#include "log.h"
#include "utils.h"
#include "http_server.h"
#include "icons.h"

#include <sys/utsname.h>

announce_upnp::announce_upnp(configuration_t *const cfg, const std::string & id, const std::string & descr, const std::vector<std::string> & announce_ids, const std::vector<std::string> &interfaces) : interface(id, descr), cfg(cfg), announce_ids(announce_ids), interfaces(interfaces)
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif
}

announce_upnp::~announce_upnp()
{
}

void announce_upnp::stop()
{
	g_main_loop_quit(loop);

	interface::stop();
}

void announce_upnp::operator()()
{
	log(id, LL_INFO, "UPNP announcer started");

	set_thread_name("UPNP_" + announce_ids.at(0));

	GError *error = nullptr;
	rygel_media_engine_init(&error);

	if (error != nullptr) {
		log(id, LL_ERR, "Could not initialize media engine: %s", error->message);
		g_error_free(error);
		return;
	}

	g_set_application_name("Constatus");

	RygelSimpleContainer *root_container = rygel_simple_container_new_root("Constatus");

	struct utsname un;
	uname(&un);
	std::string name = myformat("Constatus - %s", un.nodename);

	RygelMediaServer *server = rygel_media_server_new(name.c_str(), RYGEL_MEDIA_CONTAINER(root_container), RYGEL_PLUGIN_CAPABILITIES_NONE);

	std::string icon;

	int nr = 1;

	for(auto id : announce_ids) {
		interface *const h = find_interface_by_id(cfg, id);
		if (!h) {
			log(id, LL_ERR, "id \"%s\" not known", id.c_str());
			return;
		}

		if (h->get_class_type() != CT_HTTPSERVER) {
			log(id, LL_ERR, "id \"%s\" is not an HTTP server", id.c_str());
			return;
		}

		if (nr == 1) {
			RygelPlugin *plugin = nullptr;
			g_object_get(G_OBJECT(server), "plugin", &plugin, nullptr);
			RygelIconInfo *icon_info = rygel_icon_info_new("image/jpeg", ".jpg");
			icon = ((http_server *)h)->get_base_url() + "/icon.jpg";
			icon_info->uri = (gchar *)icon.c_str();
			icon_info->width = icon_info->height = 64;
			icon_info->size = sizeof fierman_icon_jpg;
			rygel_plugin_add_icon(plugin, icon_info);
			rygel_plugin_set_title(plugin, "Constatus");
		}

		auto urls_vid = ((http_server *)h)->get_motion_video_urls();

		for(auto url : urls_vid) {
			log(id, LL_INFO, "Announce \"%s\"", url.second.c_str());

			auto item = RYGEL_MEDIA_ITEM(rygel_video_item_new(myformat("%u", nr++).c_str(), RYGEL_MEDIA_CONTAINER(root_container), url.first.c_str(), RYGEL_VIDEO_ITEM_UPNP_CLASS));
			rygel_media_file_item_set_mime_type(RYGEL_MEDIA_FILE_ITEM(item), "video/x-motion-jpeg");
			rygel_media_object_add_uri(RYGEL_MEDIA_OBJECT(item), url.second.c_str());
			rygel_simple_container_add_child_item(root_container, item);
			rygel_media_file_item_add_engine_resources(RYGEL_MEDIA_FILE_ITEM(item), nullptr, nullptr);
		}

		auto urls_still = ((http_server *)h)->get_motion_still_images_urls();

		for(auto url : urls_still) {
			log(id, LL_INFO, "Announce \"%s\"", url.second.c_str());

			auto item = RYGEL_MEDIA_ITEM(rygel_video_item_new(myformat("%u", nr++).c_str(), RYGEL_MEDIA_CONTAINER(root_container), url.first.c_str(), RYGEL_VIDEO_ITEM_UPNP_CLASS));
			rygel_media_file_item_set_mime_type(RYGEL_MEDIA_FILE_ITEM(item), "image/jpeg");
			rygel_media_object_add_uri(RYGEL_MEDIA_OBJECT(item), url.second.c_str());
			rygel_simple_container_add_child_item(root_container, item);
			rygel_media_file_item_add_engine_resources(RYGEL_MEDIA_FILE_ITEM(item), nullptr, nullptr);
		}
	}

	for(auto interface : interfaces)
		rygel_media_device_add_interface(RYGEL_MEDIA_DEVICE(server), interface.c_str());

	loop = g_main_loop_new(nullptr, FALSE);

	g_main_loop_run(loop);

	log(id, LL_INFO, "UPNP announcer terminating");
}
#endif
