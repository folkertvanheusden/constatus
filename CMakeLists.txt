cmake_minimum_required(VERSION 3.7)

# set the project name
project(constatus VERSION 5.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-D_FILE_OFFSET_BITS=64")

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(constatus
	src/announce_upnp.cpp
	src/audio.cpp
	src/audio_alsa.cpp
	src/audio_trigger.cpp
	src/cfg.cpp
	src/cleaner.cpp
	src/controls.cpp
	src/controls_software.cpp
	src/controls_v4l.cpp
	src/db.cpp
	src/default-stylesheet.cpp
	src/draw.cpp
	src/draw_text.cpp
	src/draw_text_bitmap.cpp
	src/draw_text_font.cpp
	src/encoding.cpp
	src/error.cpp
	src/exec.cpp
	src/icons.cpp
	src/filter_add_bitmap.cpp
	src/filter_add_scaled_text.cpp
	src/filter_add_text.cpp
	src/filter_anigif_overlay.cpp
	src/filter_apply_mask.cpp
	src/filter_average.cpp
	src/filter_boost_contrast.cpp
	src/filter_chromakey.cpp
	src/filter_copy.cpp
	src/filter.cpp
	src/filter_despeckle.cpp
	src/filter_draw.cpp
	src/filter_grayscale.cpp
	src/filter_lcdproc.cpp
	src/filter_marker_simple.cpp
	src/filter_median.cpp
	src/filter_mirror_h.cpp
	src/filter_mirror_v.cpp
	src/filter_motion_only.cpp
	src/filter_noise_neighavg.cpp
	src/filter_overlay.cpp
	src/filter_plugin.cpp
	src/filter_plugin_frei0r.cpp
	src/filter_scroll.cpp
	src/gui.cpp
	src/gui_sdl.cpp
	src/http_auth.cpp
	src/http_auth_pam.cpp
	src/http_client.cpp
	src/http_cookies.cpp
	src/http_server_content.cpp
	src/http_server.cpp
	src/http_server_rest.cpp
	src/http_server_support.cpp
	src/http_utils.cpp
	src/instance.cpp
	src/interface.cpp
	src/log.cpp
	src/main.cpp
	src/meta.cpp
	src/motion_trigger.cpp
	src/motion_trigger_generic.cpp
	src/motion_trigger_other_source.cpp
	src/parameters.cpp
	src/picio.cpp
	src/pos.cpp
	src/ptz.cpp
	src/ptz_v4l.cpp
	src/resize.cpp
	src/resize_crop.cpp
	src/resize_fine.cpp
	src/schedule.cpp
	src/selection_mask.cpp
	src/source.cpp
	src/source_black.cpp
	src/source_delay.cpp
	src/source_ffmpeg.cpp
	src/source_filesystem_jpeg.cpp
	src/source_gstreamer.cpp
	src/source_http_jpeg.cpp
	src/source_http_mjpeg.cpp
	src/source_pipewire.cpp
	src/source_pixelflood.cpp
	src/source_plugin.cpp
	src/source_static.cpp
	src/source_v4l.cpp
	src/stats_tracker.cpp
	src/target_avi.cpp
	src/target.cpp
	src/target_extpipe.cpp
	src/target_ffmpeg.cpp
	src/target_gstreamer.cpp
	src/target_jpeg.cpp
	src/target_new_source.cpp
	src/target_pipewire.cpp
	src/target_pixelflood.cpp
	src/target_plugin.cpp
	src/target_vnc.cpp
	src/utils.cpp
	src/v4l2_loopback.cpp
	src/video_frame.cpp
	src/view_3d.cpp
	src/view_all.cpp
	src/view.cpp
	src/view_html_all.cpp
	src/view_html_grid.cpp
	src/view_pip.cpp
	src/view_ss.cpp
	src/ws_server.cpp
	src/ws_server_tls.cpp
	)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(constatus Threads::Threads)

include(FindPkgConfig)

pkg_check_modules(JANSSON REQUIRED jansson)
target_link_libraries(constatus ${JANSSON_LIBRARIES})
target_include_directories(constatus PUBLIC ${JANSSON_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${JANSSON_CFLAGS_OTHER})

pkg_check_modules(LIBAVFORMAT libavformat)
target_link_libraries(constatus ${LIBAVFORMAT_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBAVFORMAT_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBAVFORMAT_CFLAGS_OTHER})
pkg_check_modules(LIBSWSCALE libswscale)
target_link_libraries(constatus ${LIBSWSCALE_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBSWSCALE_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBSWSCALE_CFLAGS_OTHER})
pkg_check_modules(LIBAVCODEC libavcodec)
target_link_libraries(constatus ${LIBAVCODEC_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBAVCODEC_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBAVCODEC_CFLAGS_OTHER})
pkg_check_modules(LIBAVUTIL libavutil)
target_link_libraries(constatus ${LIBAVUTIL_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBAVUTIL_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBAVUTIL_CFLAGS_OTHER})
pkg_check_modules(LIBSWRESAMPLE libswresample)
target_link_libraries(constatus ${LIBSWRESAMPLE_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBSWRESAMPLE_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBSWRESAMPLE_CFLAGS_OTHER})

# gstreamer
pkg_check_modules(GSTREAMER gstreamer-1.0)
target_link_libraries(constatus ${GSTREAMER_LIBRARIES})
target_include_directories(constatus PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${GSTREAMER_CFLAGS_OTHER})
pkg_check_modules(GSTREAMERAPP gstreamer-app-1.0)
target_link_libraries(constatus ${GSTREAMERAPP_LIBRARIES})
target_include_directories(constatus PUBLIC ${GSTREAMERAPP_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${GSTREAMERAPP_CFLAGS_OTHER})

pkg_check_modules(LIBCONFIG REQUIRED libconfig++)
target_link_libraries(constatus ${LIBCONFIG_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBCONFIG_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBCONFIG_CFLAGS_OTHER})

pkg_check_modules(LIBPNG REQUIRED libpng)
target_link_libraries(constatus ${LIBPNG_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBPNG_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBPNG_CFLAGS_OTHER})

pkg_check_modules(EXIV2 exiv2)
target_link_libraries(constatus ${EXIV2_LIBRARIES})
target_include_directories(constatus PUBLIC ${EXIV2_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${EXIV2_CFLAGS_OTHER})

pkg_check_modules(FREI0R frei0r)
target_link_libraries(constatus ${FREI0R_LIBRARIES})
target_include_directories(constatus PUBLIC ${FREI0R_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${FREI0R_CFLAGS_OTHER})

pkg_check_modules(LIBV4L2 libv4l2)
target_link_libraries(constatus ${LIBV4L2_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBV4L2_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBV4L2_CFLAGS_OTHER})

pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
target_link_libraries(constatus ${FONTCONFIG_LIBRARIES})
target_include_directories(constatus PUBLIC ${FONTCONFIG_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${FONTCONFIG_CFLAGS_OTHER})
pkg_check_modules(FREETYPE2 REQUIRED freetype2)
target_link_libraries(constatus ${FREETYPE2_LIBRARIES})
target_include_directories(constatus PUBLIC ${FREETYPE2_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${FREETYPE2_CFLAGS_OTHER})
pkg_check_modules(ICUI18N REQUIRED icu-i18n)
target_link_libraries(constatus ${ICUI18N_LIBRARIES})
target_include_directories(constatus PUBLIC ${ICUI18N_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${ICUI18N_CFLAGS_OTHER})

pkg_check_modules(LIBCURL REQUIRED libcurl)
target_link_libraries(constatus ${LIBCURL_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBCURL_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBCURL_CFLAGS_OTHER})

pkg_check_modules(LIBJPEG REQUIRED libturbojpeg)
target_link_libraries(constatus ${LIBJPEG_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBJPEG_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBJPEG_CFLAGS_OTHER})

pkg_check_modules(OPENSSL openssl)
target_link_libraries(constatus ${OPENSSL_LIBRARIES})
target_include_directories(constatus PUBLIC ${OPENSSL_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${OPENSSL_CFLAGS_OTHER})

pkg_check_modules(LIBSDL2 sdl2)
target_link_libraries(constatus ${LIBSDL2_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBSDL2_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBSDL2_CFLAGS_OTHER})

pkg_check_modules(RYGEL rygel-server-2.6)
target_link_libraries(constatus ${RYGEL_LIBRARIES})
target_include_directories(constatus PUBLIC ${RYGEL_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${RYGEL_CFLAGS_OTHER})

pkg_check_modules(LIBXML libxml-2.0)
target_link_libraries(constatus ${LIBXML_LIBRARIES})
target_include_directories(constatus PUBLIC ${LIBXML_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${LIBXML_CFLAGS_OTHER})

pkg_check_modules(GUPNP gupnp-1.0)
target_link_libraries(constatus ${GUPNP_LIBRARIES})
target_include_directories(constatus PUBLIC ${GUPNP_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${GUPNP_CFLAGS_OTHER})

pkg_check_modules(ALSA alsa)
target_link_libraries(constatus ${ALSA_LIBRARIES})
target_include_directories(constatus PUBLIC ${ALSA_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${ALSA_CFLAGS_OTHER})

pkg_check_modules(IMAGICK ImageMagick++)
target_link_libraries(constatus ${IMAGICK_LIBRARIES})
target_include_directories(constatus PUBLIC ${IMAGICK_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${IMAGICK_CFLAGS_OTHER})


# pipewire
find_package(PipeWire)
pkg_check_modules(PIPEWIRE libpipewire-0.3)
target_link_libraries(constatus ${PIPEWIRE_LIBRARIES})
target_include_directories(constatus PUBLIC ${PIPEWIRE_INCLUDE_DIRS})
target_compile_options(constatus PUBLIC ${PIPEWIRE_CFLAGS_OTHER})

find_package(websocketpp)

CHECK_INCLUDE_FILE(pbm.h NETPBM)
if (NETPBM)
	target_link_libraries(constatus netpbm)
	set(NETPBM_FOUND 1)
else()
	message(STATUS "No netpbm library")
endif()

CHECK_INCLUDE_FILE(security/pam_appl.h LIBPAM)
if (LIBPAM)
	target_link_libraries(constatus pam)
	set(LIBPAM_FOUND 1)
else()
	message(STATUS "No pam library")
endif()

include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(cppconn/connection.h MYSQLPP)
if (MYSQLPP)
	target_link_libraries(constatus mysqlcppconn)
	set(MYSQLPP_FOUND 1)
else()
	CHECK_INCLUDE_FILE_CXX(mysql-cppconn-8/jdbc/mysql_connection.h MYSQLPP_RPM)
	if (MYSQLPP_RPM)
		target_link_libraries(constatus mysqlcppconn)
		set(MYSQLPP_FOUND_RPM 1)
	else()
		message(STATUS "No MySQL library")
	endif()
endif()

target_link_libraries(constatus ${CMAKE_DL_LIBS})

target_link_libraries(constatus -lstdc++fs)
target_link_libraries(constatus -lutil)

configure_file(config.h.in config.h)
target_include_directories(constatus PUBLIC "${PROJECT_BINARY_DIR}")

target_link_libraries(constatus "-Wl,--export-dynamic -latomic")

install(TARGETS constatus DESTINATION bin)
install(FILES man/constatus.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
install(FILES constatus.cfg DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/constatus)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/constatus)
install(FILES motion-to-constatus.py DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/constatus)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/constatus)
install(FILES README.rest DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/constatus)
install(DIRECTORY examples DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/constatus)

target_link_libraries(constatus -lboost_system)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION True)
set(CMAKE_CXX_FLAGS "-Ofast -ffast-math")
