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

#include "ImageUtils.h"
#include "Log.h"
#include "Timer.h"

#include <OpenImageIO/filesystem.h>

#include <LibRaw/libraw.h>
#include <libraw/libraw_version.h>

static std::string get_file_ext(const std::string& filepath)
{
    std::string::size_type idx = filepath.rfind('.');
    std::string ext = (idx == std::string::npos) ? "" : filepath.substr(idx + 1);
    for (auto&& c : ext) { c = ::tolower(c); }
    return ext;
}

static image_utils::ImageFileFormat get_file_format(const std::string& ext)
{
    if (ext == "bmp") {
        return image_utils::ImageFileFormat::BMP;
    }
    else if (ext == "png") {
        return image_utils::ImageFileFormat::PNG;
    }
    else if (ext == "jpg" || ext == "jpe" || ext == "jpeg" || ext == "jif" || ext == "jfif" || ext == "jfi") {
        return image_utils::ImageFileFormat::JPG;
    }
    else if (ext == "tga" || ext == "tpic") {
        return image_utils::ImageFileFormat::TGA;
    }
    else if (ext == "exr") {
        return image_utils::ImageFileFormat::EXR;
    }
    else if (ext == "hdr") {
        return image_utils::ImageFileFormat::HDR;
    }
    else if (ext == "tif" || ext == "tiff" || ext == "tx" || ext == "env" || ext == "sm" || ext == "vsm") {
        return image_utils::ImageFileFormat::TIF;
    }
    else if (ext == "jp2" || ext == "j2k") {
		return image_utils::ImageFileFormat::JP2;
	}
	else if (ext == "webp") {
		return image_utils::ImageFileFormat::WEBP;
	}
    return image_utils::ImageFileFormat::UNKNOWN;
}

OIIO::TypeDesc image_utils::get_output_format(const std::string& output_filename, const int arg, bool& r_defaulted)
{
    OIIO::TypeDesc output_format;
    std::string output_ext = get_file_ext(output_filename);
    image_utils::ImageFileFormat file_format = get_file_format(output_ext);

    r_defaulted = false;

    switch (file_format)
    {
    case image_utils::ImageFileFormat::BMP:
        output_format = OIIO::TypeDesc::UINT8;
        if (arg != 8) {
            r_defaulted = true;
        }

        break;
    case image_utils::ImageFileFormat::PNG:
        if (arg == 8) {
            output_format = OIIO::TypeDesc::UINT8;
        }
        else if (arg == 16) {
            output_format = OIIO::TypeDesc::UINT16;
        }
        else {
            output_format = OIIO::TypeDesc::UINT16;
            r_defaulted = true;
        }
        
        break;
    case image_utils::ImageFileFormat::JPG:
        output_format = OIIO::TypeDesc::UINT8;
        if (arg != 8) {
            r_defaulted = true;
        }
        
        break;
    case image_utils::ImageFileFormat::TGA:
        if (arg == 8) {
            output_format = OIIO::TypeDesc::UINT8;
        }
        else if (arg == 16) {
            output_format = OIIO::TypeDesc::UINT16;
        }
        else {
            output_format = OIIO::TypeDesc::UINT16;
            r_defaulted = true;
        }
        
        break;
    case image_utils::ImageFileFormat::EXR:
        if (arg == 16) {
            output_format = OIIO::TypeDesc::HALF;
        }
        else if (arg == 32) {
            output_format = OIIO::TypeDesc::FLOAT;
        }
        else {
            output_format = OIIO::TypeDesc::FLOAT;
            r_defaulted = true;
        }
        
        break;
    case image_utils::ImageFileFormat::HDR:
        output_format = OIIO::TypeDesc::FLOAT;
        if (arg != 32) {
            r_defaulted = true;
        }
        
        break;
    case image_utils::ImageFileFormat::TIF:
        if (arg == 8) {
            output_format = OIIO::TypeDesc::UINT8;
        }
        else if (arg == 16) {
            output_format = OIIO::TypeDesc::UINT16;
        }
        else if (arg == 32) {
            output_format = OIIO::TypeDesc::FLOAT;
        }
        else {
            output_format = OIIO::TypeDesc::UINT16;
            r_defaulted = true;
        }

        break;
    case image_utils::ImageFileFormat::JP2:
        if (arg == 8) {
            output_format = OIIO::TypeDesc::UINT8;
        }
        else if (arg == 16) {
            output_format = OIIO::TypeDesc::UINT16;
        }
        else {
            output_format = OIIO::TypeDesc::UINT16;
            r_defaulted = true;
        }
		
        break;
    case image_utils::ImageFileFormat::WEBP:
        output_format = OIIO::TypeDesc::UINT8;
        if (arg != 8) {
            r_defaulted = true;
        }

        break;
    case image_utils::ImageFileFormat::UNKNOWN:
    default:
        printf("Unknown output image file extension: *.%s\n", output_ext.c_str());
        return OIIO::TypeDesc::UINT8;
    }

    return output_format;
}

bool image_utils::progress_callback(void* opaque_data, float portion_done)
{
	// TODO: Configure the boost logger to not add EOL in this case.
	//LOG(info) << "\rProgress: " << std::fixed << std::setw(0) << std::setprecision(1) << (portion_done * 100.f) << "%.";
	return (portion_done >= 1.f);
}

bool image_utils::load_image(
    const std::string& filepath,
    const std::map<std::string, std::string>& attributes,
    int& width,
    int& height,
    void*& pixels,
    int& channels,
    int& alphaChannel,
	OIIO::TypeDesc &format)
{
    OIIO::ImageInput::unique_ptr in;
    OIIO::ImageSpec specin;

    // std::cout << "image_utils::load_image: " << filepath << std::endl;

    // set the user-provided attributes
    for (auto const& a : attributes)
    {
        specin.attribute(a.first, a.second);
    }

    /*
        if (colors == "ACES") {
            specin.attribute("oiio:ColorSpace", "ACES");
            specin.attribute("raw:ColorSpace", "ACES");
        }
        else if (colors == "raw") {
            specin.attribute("oiio:ColorSpace", "Linear");
            specin.attribute("raw:ColorSpace", "raw");
        }
        else if (colors == "ProPhoto") {
            specin.attribute("oiio:ColorSpace", "Linear");
            specin.attribute("raw:ColorSpace", "ProPhoto");
        }
        else if (colors == "ProPhoto-linear") {
            specin.attribute("oiio:ColorSpace", "Linear");
            specin.attribute("raw:ColorSpace", "ProPhoto-linear");
        }
        else if (colors == "XYZ") {
            specin.attribute("oiio:ColorSpace", "Linear");
            specin.attribute("raw:ColorSpace", "XYZ");
        }
        else {
            specin.attribute("oiio:ColorSpace", "sRGB");
            specin.attribute("raw:ColorSpace", "sRGB");
        }

        specin.attribute("raw:Demosaic", demosaic);
        specin.attribute("raw:use_camera_wb", 0);
        specin.attribute("raw:HighlightMode", 0);
        specin.attribute("raw:user_flip", userFlip);
    */

    // TODO: Fix this asap, OIIO crashes in GIFInput when a JPEG image file doesn't exist on disk.
    if (!OIIO::Filesystem::exists(filepath))
    {
        LOG(error) << "Can't open image " << filepath;
        return false;
    }

    in = OIIO::ImageInput::open(filepath, &specin);

    if (in)
    {
        LOG(info) << "Loading image " << filepath;

        std::string ext = get_file_ext(filepath);

        if (ext == "crw" || ext == "cs1" || ext == "dc2" || ext == "dcr" || ext == "dng" || ext == "erf" || ext == "fff" || ext == "k25" || ext == "kdc" || ext == "mdc" || ext == "mos" || ext == "mrw" || ext == "nef" || ext == "orf" || ext == "pef" || ext == "pxn" || ext == "raf" || ext == "raw" || ext == "rdc" || ext == "sr2" || ext == "srf" || ext == "x3f" || ext == "arw" || ext == "3fr" || ext == "cine" || ext == "ia" || ext == "kc2" || ext == "mef" || ext == "nrw" || ext == "qtk" || ext == "rw2" || ext == "sti" || ext == "rwl" || ext == "srw" || ext == "drf" || ext == "dsc" || ext == "ptx" || ext == "cap" || ext == "iiq" || ext == "rwz" || ext == "cr3")
        {
            LOG(debug) << " user attributes:";
            LOG(debug) << "  oiio:ColorSpace: " << specin.get_string_attribute("oiio:ColorSpace");
            LOG(debug) << "  raw:ColorSpace: " << specin.get_string_attribute("raw:ColorSpace");
            LOG(debug) << "  raw:Demosaic: " << specin.get_string_attribute("raw:Demosaic");
            LOG(debug) << "  raw:use_camera_wb: " << specin.get_int_attribute("raw:use_camera_wb");
            LOG(debug) << "  raw:HighlightMode: " << specin.get_int_attribute("raw:HighlightMode");
            LOG(debug) << "  raw:user_flip: " << specin.get_int_attribute("raw:user_flip");
        }

        Timer timer;
        const OIIO::ImageSpec& spec = in->spec();

        width = spec.width;
        height = spec.height;
        channels = spec.nchannels;
        alphaChannel = spec.alpha_channel;

        LOG(debug) << " size: " << width << "x" << height;
		
		// fix for TIF with alpha channel
		// Unassociated alpha treat as additional channel instead of alpha channel
        // TODO: find a better way to do this
        if (channels > 3 && alphaChannel == -1) {
            LOG(debug) << "OIIO don't detected alpha channel.";
			LOG(debug) << "Found " << channels << " channels, last set to alpha.";
            alphaChannel = channels - 1;
        }
	    
        format = spec.format;

        LOG(debug) << " channels: " << channels;

        if (alphaChannel > 3)
        {
            LOG(warning) << " alpha channel index: " << alphaChannel << " (unsupported, setting to none).";
            alphaChannel = -1;
        }
        else
            LOG(debug) << " alpha channel index: " << alphaChannel;

		// show EXIF metadata
  //    auto dateTimeOriginal = spec.get_string_attribute("Exif:DateTimeOriginal");
  //    LOG(info) << "Exif:DateTimeOriginal: %s\n", dateTimeOriginal.c_str());

  //    auto imageUniqueID = spec.get_string_attribute("Exif:ImageUniqueID");
  //    LOG(info) << "Exif:ImageUniqueID: %s\n", imageUniqueID.c_str());

	// Determine bytes per pixel
    int bytes;

	switch (format.basetype)
	{
    case OIIO::TypeDesc::UINT8:
		bytes = 1;
		break;
	case OIIO::TypeDesc::UINT16:
	case OIIO::TypeDesc::HALF:
		bytes = 2;
		break;
	case OIIO::TypeDesc::UINT32:
	case OIIO::TypeDesc::FLOAT:
		bytes = 4;
		break;
	default:
        LOG(error) << "Unsupported pixel format " << format;
        printf("%i\n", (int)format);
		exit(1);
		return false;
	}
        // malloc requred size_t (unsinged int) so we need to cast to that.
        // othwise we have int overflow for large textures.
        std::size_t pmem_size = (std::size_t)width * (std::size_t)height * (std::size_t)(channels * bytes);
		pixels = malloc(pmem_size);

        if (in->read_image(0, 0, 0, channels, format, pixels, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, (*progress_callback)))
        {
            if (in->close())
            {
                LOG(debug) << "Finished in " << timer.nowText();
                return true;
            }
        }
    }

	LOG(error) << "Can't open image " << filepath << ": " << OIIO::geterror();

    return false;
}
