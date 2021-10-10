// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <png.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#if HAVE_EXIV2 == 1
#include <exiv2/exiv2.hpp>
#endif
#include <stdexcept>

#include "error.h"
#include "meta.h"
#include "log.h"
#include "utils.h"
#include "picio.h"

#if HAVE_NETPBM == 1
extern "C" {
#include <pbm.h>
}
#endif

thread_local myjpeg my_jpeg;

static void libpng_error_handler(png_structp png, png_const_charp msg)
{
	error_exit(false, "libpng error: %s", msg);
}

static void libpng_warning_handler(png_structp png, png_const_charp msg)
{
	log(LL_WARNING, "libpng warning: %s", msg);
}

// rgbA (!)
void read_PNG_file_rgba(FILE *fh, int *w, int *h, uint8_t **pixels)
{
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, libpng_error_handler, libpng_warning_handler);
	if (!png)
		error_exit(false, "png_create_read_struct(PNG_LIBPNG_VER_STRING) failed");

	png_infop info = png_create_info_struct(png);
	if (!info)
		error_exit(false, "png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png))) {
		log(LL_INFO, "PNG decode error");
		return;
	}

	png_init_io(png, fh);

	png_read_info(png, info);

	*w = png_get_image_width(png, info);
	*h = png_get_image_height(png, info);
	int color_type = png_get_color_type(png, info);
	int bit_depth = png_get_bit_depth(png, info);

	// Read any color_type into 8bit depth, RGBA format.
	// See http://www.libpng.org/pub/png/libpng-manual.txt

	if (bit_depth == 16)
		png_set_strip_16(png);

	if(color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	// PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);

	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// These color_type don't have an alpha channel then fill it with 0xff.
	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	png_read_update_info(png, info);

	*pixels = (uint8_t *)malloc(IMS(*w, *h, 4));

	png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * *h);
	for(int y = 0; y < *h; y++)
		row_pointers[y] = &(*pixels)[y * *w * 4];

	png_read_image(png, row_pointers);
	free(row_pointers);

	png_destroy_read_struct(&png, &info, nullptr);
}

void write_PNG_file(FILE *fh, int ncols, int nrows, unsigned char *pixels)
{
	png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * nrows);
	if (!row_pointers)
		error_exit(true, "write_PNG_file error allocating row-pointers");
	for(int y=0; y<nrows; y++)
		row_pointers[y] = &pixels[y*ncols*3];

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, libpng_error_handler, libpng_warning_handler);
	if (!png)
		error_exit(false, "png_create_write_struct failed");

	png_infop info = png_create_info_struct(png);
	if (info == NULL)
		error_exit(false, "png_create_info_struct failed");

	png_init_io(png, fh);

	png_set_compression_level(png, 3);

	png_set_IHDR(png, info, ncols, nrows, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_text text_ptr[2];
	text_ptr[0].key = (png_charp)"Author";
	text_ptr[0].text = (png_charp)NAME " " VERSION;
	text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[1].key = (png_charp)"URL";
	text_ptr[1].text = (png_charp)"http://www.vanheusden.com/constatus/";
	text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
	png_set_text(png, info, text_ptr, 2);

	png_write_info(png, info);

	png_write_image(png, row_pointers);

	png_write_end(png, NULL);

	png_destroy_write_struct(&png, &info);

	free(row_pointers);
}

#if HAVE_NETPBM == 1
void load_PBM_file(FILE *const fh, int *const w, int *const h, uint8_t **out)
{
	bit **pbm = pbm_readpbm(fh, w, h);

	*out = (uint8_t *)malloc(IMUL(*w, *h, sizeof(uint8_t)));

	for(int y=0; y<*h; y++) {
		for(int x=0; x<*w; x++)
			(*out)[y * *w + x] = pbm[y][x];
	}

	pbm_freearray(pbm, *h);
}
#endif

myjpeg::myjpeg()
{
	jpegCompressor = tjInitCompress();

	jpegDecompressor = tjInitDecompress();
}

myjpeg::~myjpeg()
{
	tjDestroy(jpegDecompressor);

	tjDestroy(jpegCompressor);
}

bool myjpeg::write_JPEG_memory(const meta *const m, const int ncols, const int nrows, const int quality, const uint8_t *const pixels, uint8_t **out, size_t *out_len)
{
	uint8_t *temp = NULL;
	size_t temp_len = 0;

	//// GENERATE JPEG ////
	unsigned long int len = 0;
	if (tjCompress2(jpegCompressor, pixels, ncols, 0, nrows, TJPF_RGB, &temp, &len, TJSAMP_444, quality, TJFLAG_FASTDCT) == -1) {
		log(LL_ERR, "Failed compressing frame: %s (%dx%d @ %d)", tjGetErrorStr(), ncols, nrows, quality);
		return false;
	}

	temp_len = len;

	//// add EXIF ////

	std::pair<uint64_t, double> longitude, latitude;

#if HAVE_EXIV2 == 1
	if (m && m -> get_double("$longitude$", &longitude) && m -> get_double("$latitude$", &latitude)) {
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open((const Exiv2::byte *)temp, temp_len);
		Exiv2::ExifData exifData;

		if (exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSVersionID")) == exifData.end()) {
			Exiv2::Value::AutoPtr value = Exiv2::Value::create(Exiv2::unsignedByte);
			value->read("2 0 0 0");
			exifData.add(Exiv2::ExifKey("Exif.GPSInfo.GPSVersionID"), value.get());

			exifData["Exif.GPSInfo.GPSMapDatum"] = "WGS-84";
		}

		exifData["Exif.GPSInfo.GPSLatitudeRef"] = (latitude.second < 0 ) ? "S" : "N";

		int degLatitude = fabs(latitude.second);
		int minLatitude = (fabs(latitude.second) - degLatitude)*60;
		int secLatitude = ((fabs(latitude.second) - degLatitude)*60 - minLatitude)*60;

		char scratchBufLatitude[128];
		snprintf(scratchBufLatitude, sizeof scratchBufLatitude, "%d/1 %d/1 %d/1", degLatitude, minLatitude, secLatitude);
		exifData["Exif.GPSInfo.GPSLatitude"] = scratchBufLatitude;

		exifData["Exif.GPSInfo.GPSLongitudeRef"] = (longitude.second < 0 ) ? "W" : "E";

		int degLongitude = fabs(longitude.second);
		int minLongitude = (fabs(longitude.second) - degLongitude)*60;
		int secLongitude = ((fabs(longitude.second) - degLongitude)*60 - minLongitude )*60;

		char scratchBufLongitude[128];
		snprintf(scratchBufLongitude, sizeof scratchBufLongitude, "%d/1 %d/1 %d/1", degLongitude, minLongitude, secLongitude);
		exifData["Exif.GPSInfo.GPSLongitude"] = scratchBufLongitude;

		image->setExifData(exifData);
		image->writeMetadata();

		*out_len = image->io().size();

		*out = (uint8_t *)duplicate(image->io().mmap(), *out_len);

		tjFree(temp);
	}
	else
#endif
	{
		*out = temp;
		*out_len = temp_len;
	}

	return true;
}

bool myjpeg::read_JPEG_memory(unsigned char *in, int n_bytes_in, int *w, int *h, unsigned char **pixels)
{
	bool ok = true;

	int jpeg_subsamp = 0;
	if (tjDecompressHeader2(jpegDecompressor, in, n_bytes_in, w, h, &jpeg_subsamp) == -1) {
		log(LL_ERR, "Failed decompressing frame(-header): %s", tjGetErrorStr());
		return false;
	}

	*pixels = (unsigned char *)malloc(IMS(*w, *h, 3));
	if (tjDecompress2(jpegDecompressor, in, n_bytes_in, *pixels, *w, 0/*pitch*/, *h, TJPF_RGB, TJFLAG_FASTDCT) == -1) {
		log(LL_ERR, "Failed decompressing frame: %s", tjGetErrorStr());
		free(*pixels);
		*pixels = nullptr;
		ok = false;
	}

	return ok;
}

void myjpeg::rgb_to_i420(tjhandle t, const uint8_t *const in, const int width, const int height, uint8_t **const out)
{
	*out = (uint8_t *)malloc(width * height + width * height / 2);

        const int pos = width * height;
        uint8_t *y_ptr = *out;
        uint8_t *u_ptr = *out + pos + (pos >> 2);
        uint8_t *v_ptr = *out + pos;
	uint8_t *dstPlanes[] = { y_ptr, u_ptr, v_ptr };

	tjEncodeYUVPlanes(t, in, width, 0, height, TJPF_BGR, dstPlanes, nullptr, TJSAMP_420, 0);
}

void myjpeg::i420_to_rgb(tjhandle t, const uint8_t *const in, const int width, const int height, uint8_t **const out)
{
        const int pos = width * height;
        const uint8_t *y_ptr = in;
        const uint8_t *u_ptr = in + pos + (pos >> 2);
        const uint8_t *v_ptr = in + pos;
	const uint8_t *srcPlanes[] = { y_ptr, u_ptr, v_ptr };

	*out = (uint8_t *)malloc(IMS(width, height, 3));

	tjDecodeYUVPlanes(t, srcPlanes, nullptr, TJSAMP_420, *out, width, 0, height, TJPF_BGR, 0);
}

tjhandle myjpeg::allocate_transformer()
{
	return tjInitTransform();
}

void myjpeg::free_transformer(tjhandle h)
{
	tjDestroy(h);
}
