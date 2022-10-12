#include "GLProgramManager.h"
#include "Log.h"

GLProgramManager g_glslProgramManager;

//
// Program loading
//

std::shared_ptr<GLProgram> GLProgramManager::loadVertFrag(const std::string& path)
{
	LOG(info) << "Loading program from a single text file (vertex, fragment): " << path;

	auto it = m_list.find(path);

	if (it == m_list.end())
	{
		// Attempt to load
		std::string text;

		if (!loadTextFile(path, text))
			return nullptr;

		const std::string version;// ("#version 460 core\n");

		// TODO: Parse existing #version, if any, and insert macros after it

		std::vector<std::shared_ptr<GLShader>> shaders
		{
			std::make_shared<GLShader>(GL_VERTEX_SHADER, version + "#define RAWGL_VERTEX_SHADER\n" + text),
			std::make_shared<GLShader>(GL_FRAGMENT_SHADER, version + "#define RAWGL_FRAGMENT_SHADER\n" + text)
			//std::make_shared<GLShader>(GL_VERTEX_SHADER, "#version 460 core\n#define RAWGL_VERTEX_SHADER\n" + text),
			//std::make_shared<GLShader>(GL_FRAGMENT_SHADER, "#version 460 core\n#define RAWGL_FRAGMENT_SHADER\n" + text)
		};

		//shaders.reserve(2);
		//shaders.emplace_back(GL_VERTEX_SHADER, "#define RAWGL_VERTEX_SHADER\n" + text);
		//shaders.emplace_back(GL_FRAGMENT_SHADER, "#define RAWGL_FRAGMENT_SHADER\n" + text);

		it = m_list.insert({ path, std::make_shared<GLProgram>(shaders) }).first;
	}

	return it->second;
}

std::shared_ptr<GLProgram> GLProgramManager::loadVertFrag(const std::string paths[])
{
	LOG(info) << "Loading program from files (vertex, fragment): " << std::endl << paths[0] << "," << std::endl << paths[1];

	auto name = paths[0] + ":" + paths[1];
	auto it = m_list.find(name);

	if (it == m_list.end())
	{
		// Attempt to load
		const std::pair<std::string, GLenum> types[]
		{
			{ ".vert", GL_VERTEX_SHADER },
			{ ".frag", GL_FRAGMENT_SHADER },
			{ ".vert_spv", GL_VERTEX_SHADER },
			{ ".frag_spv", GL_FRAGMENT_SHADER }
		};
		
		std::vector<std::shared_ptr<GLShader>> shaders;

		for (int i = 0; i < 2; i++)
		{
			const std::string ext(std::filesystem::path(paths[i]).extension().string());

			if (ext == types[i].first)
			{
				std::string text;

				if (!loadTextFile(paths[i], text))
					return nullptr;

				shaders.push_back(std::make_shared<GLShader>(types[i].second, text));
			}
			else if (ext == types[i + 2].first)
			{
				std::vector<char> data;

				if (!loadBinaryFile(paths[i], data))
					return nullptr;

				shaders.push_back(std::make_shared<GLShader>(types[i + 2].second, data));
			}
			else
			{
				LOG(error) << "Unknown shader file extension " << ext;
				return nullptr;
			}
		}

		it = m_list.insert({ name, std::make_shared<GLProgram>(shaders) }).first;
	}

	return it->second;
}

std::shared_ptr<GLProgram> GLProgramManager::loadVertFragStrings(const std::string& name, const std::string sources[])
{
	LOG(info) << "Loading program from strings (vertex, fragment): " << name;

	std::vector<std::shared_ptr<GLShader>> shaders
	{
		std::make_shared<GLShader>(GL_VERTEX_SHADER, sources[0]),
		std::make_shared<GLShader>(GL_FRAGMENT_SHADER, sources[1])
	};

	auto it = m_list.insert({ name, std::make_shared<GLProgram>(shaders) }).first;

	return it->second;
}

std::shared_ptr<GLProgram> GLProgramManager::loadComp(const std::string& path)
{
	LOG(info) << "Loading program from a text file (compute): " << path;
	
	auto it = m_list.find(path);

	if (it == m_list.end())
	{
		// Attempt to load it
		std::vector<std::shared_ptr<GLShader>> shaders;
		const std::string ext(std::filesystem::path(path).extension().string());

		if (ext == ".comp")
		{
			std::string text;

			if (!loadTextFile(path, text))
				return nullptr;

			shaders.push_back(std::make_shared<GLShader>(GL_COMPUTE_SHADER, text));
		}
		else if (ext == ".comp_spv")
		{
			std::vector<char> data;

			if (!loadBinaryFile(path, data))
				return nullptr;

			shaders.push_back(std::make_shared<GLShader>(GL_COMPUTE_SHADER, data));
		}
		else
		{
			LOG(error) << "Unknown shader file extension " << ext;
			return nullptr;
		}

		it = m_list.insert({ path, std::make_shared<GLProgram>(shaders) }).first;
	}

	return it->second;
}

std::shared_ptr<GLProgram> GLProgramManager::loadCompString(const std::string& name, const std::string& source)
{
	LOG(info) << "Loading program from string (compute): " << name;

	std::vector<std::shared_ptr<GLShader>> shaders
	{
		std::make_shared<GLShader>(GL_COMPUTE_SHADER, source),
	};

	auto it = m_list.insert({ name, std::make_shared<GLProgram>(shaders) }).first;

	return it->second;
}

//
// File loading
//

/*
std::unique_ptr<GLShader> GLProgramManager::loadShader(const std::string& path, const std::string &macros)
{
	const std::filesystem::path filePath(path);
	const std::string ext(filePath.extension().string());
	const std::unordered_map<std::string, GLenum> types
	{
		{ "vert", GL_VERTEX_SHADER },
		{ "vert_spv", GL_VERTEX_SHADER },
		{ "frag", GL_FRAGMENT_SHADER },
		{ "frag_spv", GL_FRAGMENT_SHADER },
		{ "comp", GL_COMPUTE_SHADER },
		{ "comp_spv", GL_COMPUTE_SHADER }
	};

	if (ext == "vert_spv" || ext == "frag_spv" || ext == "comp_spv")
	{
		std::ifstream fs(path, std::ios::binary);

		if (!fs.is_open())
		{
			LOG(error) << "Can't find " << path;
			return false;
		}

		std::vector<char> data;
		data.assign(std::istreambuf_iterator<char>(fs), {});

		fs.close();

		return std::make_unique<GLShader>(types[ext].second, data);
	}
	else if (ext == "vert" || ext == "frag" || ext == "comp")
	{
		std::ifstream fs(path);

		if (!fs.is_open())
		{
			LOG(error) << "Can't find " << path;
			return false;
		}

		std::ostringstream ss;
		ss << fs.rdbuf();
		auto text = ss.str();

		fs.close();

		return std::make_unique<GLShader>(types[ext].second, macros + "\n" + text);
	}

	LOG(error) << "Unknown shader file extension " << ext;
}
*/
bool GLProgramManager::loadTextFile(const std::string& path, std::string& out)
{
	std::ifstream fs(path);

	if (!fs.is_open())
	{
		LOG(error) << "Can't find " << path;
		return false;
	}

	std::ostringstream ss;
	ss << fs.rdbuf();
	out = ss.str();

	fs.close();

	return true;
}

bool GLProgramManager::loadBinaryFile(const std::string& path, std::vector<char>& out)
{
	std::ifstream fs(path, std::ios::binary);

	if (!fs.is_open())
	{
		LOG(error) << "Can't find " << path;
		return false;
	}

	out.assign(std::istreambuf_iterator<char>(fs), {});

	fs.close();

	return true;
}
