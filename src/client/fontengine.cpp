/*
Minetest
Copyright (C) 2010-2014 sapier <sapier at gmx dot net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "fontengine.h"
#include <cmath>
#include "client/renderingengine.h"
#include "config.h"
#include "porting.h"
#include "filesys.h"
<<<<<<< HEAD:src/fontengine.cpp

#include "gettext.h"
#include "xCGUITTFont.h"
=======
#include "gettext.h"
#include "irrlicht_changes/CGUITTFont.h"
>>>>>>> 5.5.0:src/client/fontengine.cpp

/** maximum size distance for getting a "similar" font size */
#define MAX_FONT_SIZE_OFFSET 10

/** reference to access font engine, has to be initialized by main */
FontEngine* g_fontengine = NULL;

/** callback to be used on change of font size setting */
static void font_setting_changed(const std::string &name, void *userdata)
{
	g_fontengine->readSettings();
}

/******************************************************************************/
FontEngine::FontEngine(gui::IGUIEnvironment* env) :
	m_env(env)
{
	for (u32 &i : m_default_size) {
		i = FONT_SIZE_UNSPECIFIED;
	}

	assert(g_settings != NULL); // pre-condition
	assert(m_env != NULL); // pre-condition
	assert(m_env->getSkin() != NULL); // pre-condition

	readSettings();

<<<<<<< HEAD:src/fontengine.cpp
		m_default_size[FM_Standard] = m_settings->getU16("font_size");
		m_default_size[FM_Fallback] = m_settings->getU16("fallback_font_size");
		m_default_size[FM_Mono]     = m_settings->getU16("mono_font_size");

		if (is_yes(_("needs_fallback_font"))) {
			m_currentMode = FM_Fallback;
		}
		else {
			m_currentMode = FM_Standard;
		}

	// having freetype but not using it is quite a strange case so we need to do
	// special handling for it
	if (m_currentMode == FM_Simple) {
		std::stringstream fontsize;
		fontsize << DEFAULT_FONT_SIZE;
		m_settings->setDefault("font_size", fontsize.str());
		m_settings->setDefault("mono_font_size", fontsize.str());
	}

	m_default_size[FM_Simple]       = m_settings->getU16("font_size");
	m_default_size[FM_SimpleMono]   = m_settings->getU16("mono_font_size");

	updateSkin();

	if (m_currentMode == FM_Standard) {
		m_settings->registerChangedCallback("font_size", font_setting_changed, NULL);
		m_settings->registerChangedCallback("font_path", font_setting_changed, NULL);
		m_settings->registerChangedCallback("font_shadow", font_setting_changed, NULL);
		m_settings->registerChangedCallback("font_shadow_alpha", font_setting_changed, NULL);
	}
	else if (m_currentMode == FM_Fallback) {
		m_settings->registerChangedCallback("fallback_font_size", font_setting_changed, NULL);
		m_settings->registerChangedCallback("fallback_font_path", font_setting_changed, NULL);
		m_settings->registerChangedCallback("fallback_font_shadow", font_setting_changed, NULL);
		m_settings->registerChangedCallback("fallback_font_shadow_alpha", font_setting_changed, NULL);
	}

	m_settings->registerChangedCallback("mono_font_path", font_setting_changed, NULL);
	m_settings->registerChangedCallback("mono_font_size", font_setting_changed, NULL);
	m_settings->registerChangedCallback("screen_dpi", font_setting_changed, NULL);
	m_settings->registerChangedCallback("gui_scaling", font_setting_changed, NULL);
=======
	const char *settings[] = {
		"font_size", "font_bold", "font_italic", "font_size_divisible_by",
		"mono_font_size", "mono_font_size_divisible_by",
		"font_shadow", "font_shadow_alpha",
		"font_path", "font_path_bold", "font_path_italic", "font_path_bold_italic",
		"mono_font_path", "mono_font_path_bold", "mono_font_path_italic",
		"mono_font_path_bold_italic",
		"fallback_font_path",
		"screen_dpi", "gui_scaling",
	};

	for (auto name : settings)
		g_settings->registerChangedCallback(name, font_setting_changed, NULL);
>>>>>>> 5.5.0:src/client/fontengine.cpp
}

/******************************************************************************/
FontEngine::~FontEngine()
{
	cleanCache();
}

/******************************************************************************/
void FontEngine::cleanCache()
{
	RecursiveMutexAutoLock l(m_font_mutex);

	for (auto &font_cache_it : m_font_cache) {

		for (auto &font_it : font_cache_it) {
			font_it.second->drop();
			font_it.second = nullptr;
		}
		font_cache_it.clear();
	}
}

/******************************************************************************/
irr::gui::IGUIFont *FontEngine::getFont(FontSpec spec)
{
	return getFont(spec, false);
}

irr::gui::IGUIFont *FontEngine::getFont(FontSpec spec, bool may_fail)
{
	if (spec.mode == FM_Unspecified) {
		spec.mode = m_currentMode;
	} else if (spec.mode == _FM_Fallback) {
		// Fallback font doesn't support these
		spec.bold = false;
		spec.italic = false;
	}

	// Fallback to default size
	if (spec.size == FONT_SIZE_UNSPECIFIED)
		spec.size = m_default_size[spec.mode];

	RecursiveMutexAutoLock l(m_font_mutex);

	const auto &cache = m_font_cache[spec.getHash()];
	auto it = cache.find(spec.size);
	if (it != cache.end())
		return it->second;

	// Font does not yet exist
	gui::IGUIFont *font = initFont(spec);

	if (!font && !may_fail) {
		errorstream << "Minetest cannot continue without a valid font. "
			"Please correct the 'font_path' setting or install the font "
			"file in the proper location." << std::endl;
		abort();
	}

	m_font_cache[spec.getHash()][spec.size] = font;

	return font;
}

/******************************************************************************/
unsigned int FontEngine::getTextHeight(const FontSpec &spec)
{
	gui::IGUIFont *font = getFont(spec);

	return font->getDimension(L"Some unimportant example String").Height;
}

/******************************************************************************/
unsigned int FontEngine::getTextWidth(const std::wstring &text, const FontSpec &spec)
{
	gui::IGUIFont *font = getFont(spec);

	return font->getDimension(text.c_str()).Width;
}

/** get line height for a specific font (including empty room between lines) */
unsigned int FontEngine::getLineHeight(const FontSpec &spec)
{
	gui::IGUIFont *font = getFont(spec);

	return font->getDimension(L"Some unimportant example String").Height
			+ font->getKerningHeight();
}

/******************************************************************************/
unsigned int FontEngine::getDefaultFontSize()
{
	return m_default_size[m_currentMode];
}

unsigned int FontEngine::getFontSize(FontMode mode)
{
	if (mode == FM_Unspecified)
		return m_default_size[FM_Standard];

	return m_default_size[mode];
}

/******************************************************************************/
void FontEngine::readSettings()
{
<<<<<<< HEAD:src/fontengine.cpp
		m_default_size[FM_Standard] = m_settings->getU16("font_size");
		m_default_size[FM_Fallback] = m_settings->getU16("fallback_font_size");
		m_default_size[FM_Mono]     = m_settings->getU16("mono_font_size");

		if (is_yes(_("needs_fallback_font"))) {
			m_currentMode = FM_Fallback;
		}
		else {
			m_currentMode = FM_Standard;
		}
	m_default_size[FM_Simple]       = m_settings->getU16("font_size");
	m_default_size[FM_SimpleMono]   = m_settings->getU16("mono_font_size");
=======
	m_default_size[FM_Standard]  = g_settings->getU16("font_size");
	m_default_size[_FM_Fallback] = g_settings->getU16("font_size");
	m_default_size[FM_Mono]      = g_settings->getU16("mono_font_size");

	m_default_bold = g_settings->getBool("font_bold");
	m_default_italic = g_settings->getBool("font_italic");
>>>>>>> 5.5.0:src/client/fontengine.cpp

	cleanCache();
	updateFontCache();
	updateSkin();
}

/******************************************************************************/
void FontEngine::updateSkin()
{
	gui::IGUIFont *font = getFont();
	assert(font);

	m_env->getSkin()->setFont(font);
}

/******************************************************************************/
void FontEngine::updateFontCache()
{
	/* the only font to be initialized is default one,
	 * all others are re-initialized on demand */
	getFont(FONT_SIZE_UNSPECIFIED, FM_Unspecified);
}

/******************************************************************************/
gui::IGUIFont *FontEngine::initFont(const FontSpec &spec)
{
	assert(spec.mode != FM_Unspecified);
	assert(spec.size != FONT_SIZE_UNSPECIFIED);

	std::string setting_prefix = "";
	if (spec.mode == FM_Mono)
		setting_prefix = "mono_";

	std::string setting_suffix = "";
	if (spec.bold)
		setting_suffix.append("_bold");
	if (spec.italic)
		setting_suffix.append("_italic");

	u32 size = std::max<u32>(spec.size * RenderingEngine::getDisplayDensity() *
			g_settings->getFloat("gui_scaling"), 1);

	// Constrain the font size to a certain multiple, if necessary
	u16 divisible_by = g_settings->getU16(setting_prefix + "font_size_divisible_by");
	if (divisible_by > 1) {
		size = std::max<u32>(
				std::round((double)size / divisible_by) * divisible_by, divisible_by);
	}

	sanity_check(size != 0);

	u16 font_shadow       = 0;
	u16 font_shadow_alpha = 0;
	g_settings->getU16NoEx(setting_prefix + "font_shadow", font_shadow);
	g_settings->getU16NoEx(setting_prefix + "font_shadow_alpha",
			font_shadow_alpha);

	std::string path_setting;
	if (spec.mode == _FM_Fallback)
		path_setting = "fallback_font_path";
	else
		path_setting = setting_prefix + "font_path" + setting_suffix;

	std::string fallback_settings[] = {
		g_settings->get(path_setting),
		Settings::getLayer(SL_DEFAULTS)->get(path_setting)
	};

<<<<<<< HEAD:src/fontengine.cpp
		case FM_Simple: /* Fallthrough */
		case FM_SimpleMono: /* Fallthrough */
		default:
			font_config_prefix = "";

	}

	if (m_font_cache[mode].find(basesize) != m_font_cache[mode].end())
		return;

	if ((mode == FM_Simple) || (mode == FM_SimpleMono)) {
		initSimpleFont(basesize, mode);
		return;
	}

	else {
		unsigned int size = floor(
				porting::getDisplayDensity() *
				m_settings->getFloat("gui_scaling") *
				basesize);
		u32 font_shadow       = 0;
		u32 font_shadow_alpha = 0;

		try {
			font_shadow =
					g_settings->getU16(font_config_prefix + "font_shadow");
		} catch (SettingNotFoundException&) {}
		try {
			font_shadow_alpha =
					g_settings->getU16(font_config_prefix + "font_shadow_alpha");
		} catch (SettingNotFoundException&) {}

		std::string font_path = g_settings->get(font_config_prefix + "font_path");

		irr::gui::IGUIFont* font = gui::CGUITTFont::createTTFont(m_env,
=======
	for (const std::string &font_path : fallback_settings) {
		gui::CGUITTFont *font = gui::CGUITTFont::createTTFont(m_env,
>>>>>>> 5.5.0:src/client/fontengine.cpp
				font_path.c_str(), size, true, true, font_shadow,
				font_shadow_alpha);

		if (!font) {
			errorstream << "FontEngine: Cannot load '" << font_path <<
				"'. Trying to fall back to another path." << std::endl;
			continue;
		}

		if (spec.mode != _FM_Fallback) {
			FontSpec spec2(spec);
			spec2.mode = _FM_Fallback;
			font->setFallback(getFont(spec2, true));
		}
		return font;
	}
<<<<<<< HEAD:src/fontengine.cpp
}

/** initialize a font without freetype */
void FontEngine::initSimpleFont(unsigned int basesize, FontMode mode)
{
	assert(mode == FM_Simple || mode == FM_SimpleMono); // pre-condition

	std::string font_path = "";
	if (mode == FM_Simple) {
		font_path = m_settings->get("font_path");
	} else {
		font_path = m_settings->get("mono_font_path");
	}
	std::string basename = font_path;
	std::string ending = font_path.substr(font_path.length() -4);

	if (ending == ".ttf") {
		errorstream << "FontEngine: Not trying to open \"" << font_path
				<< "\" which seems to be a truetype font." << std::endl;
		return;
	}

	if ((ending == ".xml") || (ending == ".png")) {
		basename = font_path.substr(0,font_path.length()-4);
	}

	if (basesize == FONT_SIZE_UNSPECIFIED)
		basesize = DEFAULT_FONT_SIZE;

	unsigned int size = floor(
			porting::getDisplayDensity() *
			m_settings->getFloat("gui_scaling") *
			basesize);

	irr::gui::IGUIFont* font = NULL;

	for(unsigned int offset = 0; offset < MAX_FONT_SIZE_OFFSET; offset++) {

		// try opening positive offset
		std::stringstream fontsize_plus_png;
		fontsize_plus_png << basename << "_" << (size + offset) << ".png";

		if (fs::PathExists(fontsize_plus_png.str())) {
			font = m_env->getFont(fontsize_plus_png.str().c_str());

			if (font) {
				verbosestream << "FontEngine: found font: " << fontsize_plus_png.str() << std::endl;
				break;
			}
		}

		std::stringstream fontsize_plus_xml;
		fontsize_plus_xml << basename << "_" << (size + offset) << ".xml";

		if (fs::PathExists(fontsize_plus_xml.str())) {
			font = m_env->getFont(fontsize_plus_xml.str().c_str());

			if (font) {
				verbosestream << "FontEngine: found font: " << fontsize_plus_xml.str() << std::endl;
				break;
			}
		}

		// try negative offset
		std::stringstream fontsize_minus_png;
		fontsize_minus_png << basename << "_" << (size - offset) << ".png";

		if (fs::PathExists(fontsize_minus_png.str())) {
			font = m_env->getFont(fontsize_minus_png.str().c_str());

			if (font) {
				verbosestream << "FontEngine: found font: " << fontsize_minus_png.str() << std::endl;
				break;
			}
		}

		std::stringstream fontsize_minus_xml;
		fontsize_minus_xml << basename << "_" << (size - offset) << ".xml";

		if (fs::PathExists(fontsize_minus_xml.str())) {
			font = m_env->getFont(fontsize_minus_xml.str().c_str());

			if (font) {
				verbosestream << "FontEngine: found font: " << fontsize_minus_xml.str() << std::endl;
				break;
			}
		}
	}

	// try name direct
	if (font == NULL) {
		if (fs::PathExists(font_path)) {
			font = m_env->getFont(font_path.c_str());
			if (font)
				verbosestream << "FontEngine: found font: " << font_path << std::endl;
		}
	}

	if (font != NULL) {
		font->grab();
		m_font_cache[mode][basesize] = font;
	}
=======
	return nullptr;
>>>>>>> 5.5.0:src/client/fontengine.cpp
}
