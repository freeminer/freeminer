// SPDX-License-Identifier: GPL-3.0-or-later
// Compatibility declarations for application initializers in Mandelbulber2.
#include "parameters.h"
#include "src/fractal_enums.h"
#include "src/object_types.hpp"
#include "src/object_node_type.h"
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#define MANDELBULBER2_SRC_FILE_MESH_HPP_
#define MANDELBULBER2_SRC_FILES_H_
#define MANDELBULBER2_SRC_FRACTPARAMS_HPP_
#define MANDELBULBER2_SRC_LIGHT_H_
#define MANDELBULBER2_SRC_MATERIAL_H_
#define MANDELBULBER2_SRC_NETRENDER_HPP_
#define MANDELBULBER2_SRC_STEREO_H_
#define MANDELBULBER2_SRC_SYSTEM_DATA_HPP_
#define MANDELBULBER2_SRC_SYSTEM_DIRECTORIES_HPP_
#define MANDELBULBER2_SRC_TEXTURE_ENUMS_HPP_
#define MANDELBULBER2_FORMULA_DEFINITION_ALL_FRACTAL_LIST_HPP_

// All non-fractal initializers are unused by Freeminer. Keep their placeholder
// types scoped here so they cannot collide with the host or formula types.
namespace mandelbulber2_initializers
{
using ::morphAkima;
using ::morphAkimaAngle;
using ::morphLinear;
using ::morphNone;
using ::paramStandard;
enum
{
	paramApp,
	paramNoSave,
	paramOnlyForNet
};
enum
{
	fractalColoring_Standard
};
static int get_cpu_count()
{
	return 1;
}
namespace parameterContainer
{
}

class QString : public std::string
{
public:
	using std::string::string;
	QString(const std::string &value) : std::string(value) {}
	QString() = default;
	template <typename T, typename... Extra>
	QString arg(const T &, Extra...) const
	{
		return *this;
	}
	int indexOf(char c) const
	{
		auto pos = find(c);
		return pos == npos ? -1 : static_cast<int>(pos);
	}
	QString left(int length) const { return substr(0, static_cast<size_t>(length)); }
};
using QStringList = std::vector<std::string>;
using QChar = char;
template <typename T>
using QList = std::vector<T>;
struct sRGB
{
	sRGB(int, int, int) {}
};
struct sPrimitiveItem
{
	QString fullName;
	QString typeName;
	int id = 0;
	fractal::enumObjectType type = fractal::objNone;
};
struct cMaterial
{
	static QString Name(const char *name, int) { return name; }
};
struct cLight
{
	enum
	{
		lightDecay1R2,
		lightDirectional,
		lightPoint
	};
	static QString Name(const char *name, int) { return name; }
};
struct cNetRender
{
	enum
	{
		netRenderModeServer
	};
};
struct cStereo
{
	enum
	{
		stereoLeftRight,
		eyeNone
	};
};
namespace params
{
enum
{
	AOModeScreenSpace,
	mapEquirectangular
};
}
namespace texture
{
enum
{
	mappingPlanar
};
}
namespace ImageFileSave
{
enum
{
	IMAGE_CHANNEL_QUALITY_8,
	IMAGE_CHANNEL_QUALITY_32
};
}
namespace MeshFileSave
{
enum
{
	MESH_BINARY
};
}
struct QDir
{
	static char separator() { return '/'; }
	static QString toNativeSeparators(const std::string &value) { return value; }
};
struct Directories
{
	QString sharedDir;
	QString GetAnimationFolder() const { return {}; }
	QString GetImagesFolder() const { return {}; }
	QString GetSettingsFolder() const { return {}; }
	QString GetSlicesFolder() const { return {}; }
	QString GetTexturesFolder() const { return {}; }
	QString GetToolbarFolder() const { return {}; }
	QString GetUserTexturesFolder() const { return {}; }
};
static Directories systemDirectories;
struct SystemData
{
	int GetPreferredFontPointSize() const { return 0; }
	int GetPreferredThumbnailSize() const { return 0; }
};
static SystemData systemData;

class cParameterContainer : public ::cParameterContainer
{
public:
	template <typename T>
	void addParam(const std::string &name, const T &value, int morph, int type,
			const QStringList &lookup = {})
	{
		if constexpr (std::is_same_v<T, QString>)
			::cParameterContainer::addParam(
					name.c_str(), std::string(value), morph, type, lookup);
		else if constexpr (!std::is_same_v<T, sRGB>)
			::cParameterContainer::addParam(name.c_str(), value, morph, type, lookup);
	}
	template <typename T>
	void addParam(const std::string &name, const T &value, const T &, const T &,
			int morph, int type)
	{
		addParam(name, value, morph, type);
	}
	template <typename T>
	void addParam(const std::string &name, int index, const T &value, int morph, int type)
	{
		if constexpr (!std::is_same_v<T, sRGB>)
			::cParameterContainer::addParam(name.c_str(), index, value, morph, type);
	}
	void DeleteParameter(const std::string &) {}
	void SetAsGradient(const std::string &) {}
	QList<QString> GetListOfParameters() const { return {}; }
};

#include "src/initparameters.cpp"
} // namespace mandelbulber2_initializers

std::shared_ptr<cParameterContainer> mandelbulber2FractalDefaults()
{
	auto parameters = std::make_shared<mandelbulber2_initializers::cParameterContainer>();
	mandelbulber2_initializers::InitFractalParams(parameters);
	return parameters;
}
