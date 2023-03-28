/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Common.h"

#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/imageio.h>

//extern const int IMAGE_CHANNELS_USED;

namespace image_utils {

struct Img {
	int width;
	int height;
	float* pixels;
};

/// <summary>
/// Supported image file formats.
/// Values are equal to next file extensions
/// (converted by image_utils::get_file_format(ext)):
/// UNKNOWN - Unknown file format.
/// BMP - *.bmp;
/// PNG - *.png;
/// JPG - *.jpg, *.jpe, *.jpeg, *.jif ;
/// TGA - *.tga, *.tpic;
/// EXR - *.exr;
/// HDR - *.hdr;
/// TIF - *.tif, *.tiff, *.tx, *.env, *.sm, *.vsm;
/// </summary>
enum class ImageFileFormat {
	UNKNOWN,
	BMP,
	PNG,
	JPG,
	TGA,
	EXR,
	HDR,
	TIF,
	JP2,
	WEBP
};

/// <summary>
/// Output file format (OIIO::TypeDesc:: ... ) relative to passed argument
/// and supported values.
/// </summary>
/// <param name="arg"></param>
/// <param name="r_defaulted"></param>
/// <returns></returns>
extern OIIO::TypeDesc get_output_format(const std::string& output_filename, const int arg, bool& r_defaulted);

/// <summary>
/// Callback function used to display image read / write progress.
/// </summary>
/// <param name="opaque_data"></param>
/// <param name="portion_done"></param>
/// <returns>`true` if finished</returns>
extern bool progress_callback(void* opaque_data, float portion_done);

/// <summary>
/// Load image file data from given filepath.
/// </summary>
/// <param name="filepath"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="pixels"></param>
/// <returns></returns>
extern bool load_image(const std::string& filepath, const std::map<std::string, std::string>& attributes, int& width, int& height, void*& pixels, int& channels, int& alphaChannel, OIIO::TypeDesc &format);
}

// TODO: Move function to File_IO
extern std::string get_file_ext(const std::string& filepath);