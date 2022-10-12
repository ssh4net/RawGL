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

#include "Sequence.h"
#include "Timer.h"

PassOutput::PassOutput() :
	internalFormatText("rgba32f"),
	channels(3),
	alphaChannel(-1),
	bits(16)
{
}

void PassOutput::saveTexture()
{
    if (path.empty())
        return;

    Timer timer;

    LOG(info) << "Saving image: " << path;

    std::unique_ptr<OIIO::ImageOutput> output = OIIO::ImageOutput::create(path);

    if (!output) {
        LOG(error) << "Can't open file: " << OIIO::geterror();
        return;
    }

    /*
                //LOG(info) << "RB OUTPUT INDEX: %i\n\n", output.output->location);
                glReadBuffer(GL_COLOR_ATTACHMENT0 + output.output->location);

                glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
                //glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
                //glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

                glReadPixels(0, 0, pass.width, pass.height, GL_RGBA, GL_FLOAT, imageData);
    */

    // Start writing
    OIIO::ImageSpec spec(texture->getWidth(), texture->getHeight(), texture->getChannels(), format);

	// set the user-provided attributes
	for (auto const& a : attributes)
	{
		spec.attribute(a.first, a.second);
	}

    //spec["alpha_channel"] = alphaChannel;
    spec.alpha_channel = alphaChannel;

    if (!output->open(path, spec))
    {
        LOG(error) << "Can't open file for writing: " << OIIO::geterror();
        return;
    }

    // Figure out OIIO pixel format from the texture internal format
    const GLenum internalFormat = texture->getInternalFormat();
    OIIO::TypeDesc pf;
    GLenum type;
        
    switch (internalFormat)
    {
    case GL_R8:
    case GL_RG8:
    case GL_RGB8:
    case GL_RGBA8:
        pf = OIIO::TypeDesc::UINT8;
        type = GL_UNSIGNED_BYTE;
        break;
    case GL_R16:
    case GL_RG16:
    case GL_RGB16:
    case GL_RGBA16:
        pf = OIIO::TypeDesc::UINT16;
        type = GL_UNSIGNED_SHORT;
        break;
    case GL_R16F:
    case GL_RG16F:
    case GL_RGB16F:
    case GL_RGBA16F:
        pf = OIIO::TypeDesc::HALF;
        type = GL_HALF_FLOAT;
        break;
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F:
        pf = OIIO::TypeDesc::FLOAT;
        type = GL_FLOAT;
        break;
    default:
        assert(0);
        exit(1);
        return;
    }

    // Read the framebuffer data
    void* data = texture->getData(type);

    if (!output->write_image(pf, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride, (*image_utils::progress_callback)))
        LOG(error) << "Can't write file: " << OIIO::geterror();
    else
    {
        if (!output->close())
            LOG(error) << "Can't close file after writing: " << OIIO::geterror();
        else
            LOG(info) << "Finished in " << timer.nowText();
    }

    free(data);
}
