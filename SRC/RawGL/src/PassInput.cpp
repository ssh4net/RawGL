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

template <> int32_t str_to_numeric(hres& hr, const std::string& str_val)
{
    int ret = 0;

    if (hres::OK == hr) {
        try {
            ret = ((str_val == "false") ? (0) : ((str_val == "true" ? 1 : std::stoi(str_val))));
        }
        catch (const std::invalid_argument& e_arg) {
            hr = hres::ERR;
            //LOG(error) << "Unable to parse invalid integer value (\"" << str_val << "\"):\n" << e_arg.what();
        }
        catch (const std::out_of_range& e_oor) {
            hr = hres::ERR;
            //LOG(error) << "Unable to parse integer value (\"" << str_val << "\") (out of range):\n" << e_oor.what();
        }
    }

    return ret;
}

template <> float_t str_to_numeric(hres& hr, const std::string& str_val)
{
    float ret = 0.0f;

    if (hres::OK == hr) {
        try {
            ret = std::stof(str_val);
        }
        catch (const std::invalid_argument& e_arg) {
            hr = hres::ERR;
            //LOG(error) << "Unable to parse invalid float value (\"" << str_val << "\"):\n" << e_arg.what();
        }
        catch (const std::out_of_range& e_oor) {
            hr = hres::ERR;
            //LOG(error) << "Unable to parse float value (\"" << str_val << "\") (out of range):\n" << e_oor.what();
        }
    }

    return ret;
}

template <> int32_t str_to_numeric(const std::string& str_val)
{
    hres hr = hres::OK;
    return str_to_numeric<int32_t>(hr, str_val);
}

template <> float_t str_to_numeric(const std::string& str_val)
{
    hres hr = hres::OK;
    return str_to_numeric<float_t>(hr, str_val);
}

// NOTE: First attribute value key should be a default value
const std::vector<PassInput::TexAttr> PassInput::TEX_ATTR_ARR = {
    {
        "min",
        &_pass_input_set_tex_min,
        {
            {"l", GL_LINEAR, "GL_LINEAR"},
            {"n", GL_NEAREST, "GL_NEAREST"},
            {"ll", GL_LINEAR_MIPMAP_LINEAR, "GL_LINEAR_MIPMAP_LINEAR"},
            {"ln", GL_LINEAR_MIPMAP_NEAREST, "GL_LINEAR_MIPMAP_NEAREST"},
            {"nl", GL_NEAREST_MIPMAP_LINEAR, "GL_NEAREST_MIPMAP_LINEAR"},
            {"nn", GL_NEAREST_MIPMAP_NEAREST, "GL_NEAREST_MIPMAP_NEAREST"},
        },
        "Texture minification function",
    },
    {
        "mag",
        &_pass_input_set_tex_mag,
        {
            {"l", GL_LINEAR, "GL_LINEAR"},
            {"n", GL_NEAREST, "GL_NEAREST"},
        },
        "Texture magnification function",
    },
    {
        "wrps",
        &_pass_input_set_tex_s,
        {
            {"ce", GL_CLAMP_TO_EDGE, "GL_CLAMP_TO_EDGE"},
            {"r", GL_REPEAT, "GL_REPEAT"},
            {"cb", GL_CLAMP_TO_BORDER, "GL_CLAMP_TO_BORDER"},
            {"mr", GL_MIRRORED_REPEAT, "GL_MIRRORED_REPEAT"},
            {"mce", GL_MIRROR_CLAMP_TO_EDGE, "GL_MIRROR_CLAMP_TO_EDGE"},
        },
        "Texture wrap s-axis",
    },
    {
        "wrpt",
        &_pass_input_set_tex_t,
        {
            {"ce", GL_CLAMP_TO_EDGE, "GL_CLAMP_TO_EDGE"},
            {"r", GL_REPEAT, "GL_REPEAT"},
            {"cb", GL_CLAMP_TO_BORDER, "GL_CLAMP_TO_BORDER"},
            {"mr", GL_MIRRORED_REPEAT, "GL_MIRRORED_REPEAT"},
            {"mce", GL_MIRROR_CLAMP_TO_EDGE, "GL_MIRROR_CLAMP_TO_EDGE"},
        },
        "Texture wrap t-axis",
    },
};

const void _pass_input_set_tex_min(PassInput& pi, const GLint& val)
{
    pi.tex_min = val;
}

const void _pass_input_set_tex_mag(PassInput& pi, const GLint& val)
{
    pi.tex_mag = val;
}

const void _pass_input_set_tex_s(PassInput& pi, const GLint& val)
{
    pi.tex_s = val;
}

const void _pass_input_set_tex_t(PassInput& pi, const GLint& val)
{
    pi.tex_t = val;
}

const void PassInput::eval_tex_attr(hres& hr, const std::string& name, const std::string& attr_val_name)
{
    if (hres::OK == hr) {
        for (const auto& tex_attr : PassInput::TEX_ATTR_ARR) {
            if (name == tex_attr.name) {
                for (const auto& possible_val : tex_attr.possible_values) {
                    if (attr_val_name == possible_val.key) {
                        tex_attr.func(*this, possible_val.gl_value);
                        return;
                    }
                }
            }
        }
    }

    hr = hres::ERR;
}

std::string PassInput::get_possible_tex_attr_fmt()
{
    std::string ret;

    for (const auto& tex_attr : PassInput::TEX_ATTR_ARR) {
        ret += tex_attr.name + " - " + tex_attr.desc + ": [";

        for (size_t i = 0; i < tex_attr.possible_values.size(); ++i) {
            auto& possible_val = tex_attr.possible_values[i];
            ret += possible_val.key + " (";
            if (i == 0) {
                ret += "default, ";
            }
            ret += possible_val.desc + ")";

            if (i < tex_attr.possible_values.size() - 1) {
                ret += ", ";
            }
        }
        ret += "]\n";
    }

    return ret;
}

PassInput::PassInput()
{
    memset(ints, 0, sizeof(GLint) * NUM_INTS);
    memset(floats, 0.0f, sizeof(GLfloat) * NUM_FLOATS);
    uniform = nullptr;

    // set some default values for unspecificed attributes
    // in our own way, which may differ from the library
    attributes["oiio:ColorSpace"] = "Linear";
    attributes["raw:colorSpace"] = "raw";
    attributes["raw:demosaic"] = "AAHD";
    attributes["raw:user_flip"] = "-1";
    attributes["raw:use_camera_wb"] = "0";

    // Initialize texture attributes to first of possible values in list
    for (const auto& tex_attr : PassInput::TEX_ATTR_ARR) {
        tex_attr.func(*this, tex_attr.possible_values[0].gl_value);
    }
}


#if 0

void PassInput::loadTexture()
{
    Timer timer;
    int width, height, channels;
    float* data = nullptr;

    LOG(info) << "Loading image: " << texturePath;

    if (!image_utils::load_image(texturePath, width, height, data, channels, colorSpace, demosaic, userFlip)) {
        LOG(error) << OIIO::geterror();

        if (data)
            free(data);

        exit(1);
    }

    LOG(info) << "Finished in " << timer.nowText();

    LOG(info) << "Success!\n\n");
    /*
        if (channels != 1 && channels != 3)
        {
            LOG(error) << "Only grayscale and RGB images are supported.");

            if (data)
                free(data);

            exit(1);
        }
    */



    texture = textureMgr->add(width, height, formats[channels - 1], data);

    if (data)
        free(data);
}

std::shared_ptr<Texture> TextureMgr::add(int width, int height, int internalFormat, const float* data = nullptr);
// is the texture already loaded?
auto it = m_textures.find(input.texturePath);

if (it == m_textures.end())
{
    // no, so load it
    const GLenum formats[4] = { GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F };
    it = m_textures.insert({ input.texturePath, std::make_shared<Texture>(width, height, formats[channels - 1], data) }).first;
}

return it->second
}

#endif
