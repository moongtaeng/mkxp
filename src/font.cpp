/*
** font.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 Jonas Kulla <Nyocurio@gmail.com>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "font.h"

#include "sharedstate.h"
#include "filesystem.h"
#include "exception.h"
#include "boost-hash.h"
#include "util.h"
#include "config.h"

#include <string>
#include <utility>

#include "../liberation.ttf.xxd"

#include <SDL_ttf.h>

#define BUNDLED_FONT liberation

#define BUNDLED_FONT_D(f) assets_## f ##_ttf
#define BUNDLED_FONT_L(f) assets_## f ##_ttf_len

// Go fuck yourself CPP
#define BNDL_F_D(f) BUNDLED_FONT_D(f)
#define BNDL_F_L(f) BUNDLED_FONT_L(f)

typedef std::pair<std::string, int> FontKey;

static SDL_RWops *openBundledFont()
{
	return SDL_RWFromConstMem(BNDL_F_D(BUNDLED_FONT), BNDL_F_L(BUNDLED_FONT));
}

struct FontSet
{
	/* 'Regular' style */
	std::string regular;

	/* Any other styles (used in case no 'Regular' exists) */
	std::string other;
};

struct SharedFontStatePrivate
{
	/* Maps: font family name, To: substituted family name,
	 * as specified via configuration file / arguments */
	BoostHash<std::string, std::string> subs;

	/* Maps: font family name, To: set of physical
	 * font filenames located in "Fonts/" */
	BoostHash<std::string, FontSet> sets;

	/* Pool of already opened fonts; once opened, they are reused
	 * and never closed until the termination of the program */
	BoostHash<FontKey, TTF_Font*> pool;
};

SharedFontState::SharedFontState(const Config &conf)
{
	p = new SharedFontStatePrivate;

	/* Parse font substitutions */
	for (size_t i = 0; i < conf.fontSubs.size(); ++i)
	{
		const std::string &raw = conf.fontSubs[i];
		size_t sepPos = raw.find_first_of('>');

		if (sepPos == std::string::npos)
			continue;

		std::string from = raw.substr(0, sepPos);
		std::string to   = raw.substr(sepPos+1);

		p->subs.insert(from, to);
	}
}

SharedFontState::~SharedFontState()
{
	BoostHash<FontKey, TTF_Font*>::const_iterator iter;
	for (iter = p->pool.cbegin(); iter != p->pool.cend(); ++iter)
		TTF_CloseFont(iter->second);

	delete p;
}

void SharedFontState::initFontSetCB(SDL_RWops &ops,
                                    const std::string &filename)
{
	TTF_Font *font = TTF_OpenFontRW(&ops, 0, 0);

	if (!font)
		return;

	std::string family = TTF_FontFaceFamilyName(font);
	std::string style = TTF_FontFaceStyleName(font);

	TTF_CloseFont(font);

	FontSet &set = p->sets[family];

	if (style == "Regular")
		set.regular = filename;
	else
		set.other = filename;
}

_TTF_Font *SharedFontState::getFont(std::string family,
                                    int size)
{
	/* Check for substitutions */
	if (p->subs.contains(family))
		family = p->subs[family];

	/* Find out if the font asset exists */
	const FontSet &req = p->sets[family];

	if (req.regular.empty() && req.other.empty())
	{
		/* Doesn't exist; use built-in font */
		family = "";
	}

	FontKey key(family, size);

	TTF_Font *font = p->pool.value(key);

	if (font)
		return font;

	/* Not in pool; open new handle */
	SDL_RWops *ops;

	if (family.empty())
	{
		/* Built-in font */
		ops = openBundledFont();
	}
	else
	{
		/* Use 'other' path as alternative in case
		 * we have no 'regular' styled font asset */
		const char *path = !req.regular.empty()
		                 ? req.regular.c_str() : req.other.c_str();

		ops = SDL_AllocRW();
		shState->fileSystem().openReadRaw(*ops, path, true);
	}

	// FIXME 0.9 is guesswork at this point
//	float gamma = (96.0/45.0)*(5.0/14.0)*(size-5);
//	font = TTF_OpenFontRW(ops, 1, gamma /** .90*/);
	font = TTF_OpenFontRW(ops, 1, size* .90);

	if (!font)
		throw Exception(Exception::SDLError, "%s", SDL_GetError());

	p->pool.insert(key, font);

	return font;
}

bool SharedFontState::fontPresent(std::string family)
{
	/* Check for substitutions */
	if (p->subs.contains(family))
		family = p->subs[family];

	const FontSet &set = p->sets[family];

	return !(set.regular.empty() && set.other.empty());
}


struct FontPrivate
{
	std::string name;
	int size;
	bool bold;
	bool italic;
	Color *color;

	Color colorTmp;

	static std::string defaultName;
	static int defaultSize;
	static bool defaultBold;
	static bool defaultItalic;
	static Color *defaultColor;

	static Color defaultColorTmp;

	/* The actual font is opened as late as possible
	 * (when it is queried by a Bitmap), prior it is
	 * set to null */
	TTF_Font *sdlFont;

	FontPrivate(const char *name = 0,
	            int size = 0)
	    : name(name ? std::string(name) : defaultName),
	      size(size ? size : defaultSize),
	      bold(defaultBold),
	      italic(defaultItalic),
	      color(&colorTmp),
	      colorTmp(*defaultColor),
	      sdlFont(0)
	{}

	FontPrivate(const FontPrivate &other)
	    : name(other.name),
	      size(other.size),
	      bold(other.bold),
	      italic(other.italic),
	      color(&colorTmp),
	      colorTmp(*other.color),
	      sdlFont(other.sdlFont)
	{}
};

std::string FontPrivate::defaultName   = "Arial";
int         FontPrivate::defaultSize   = 22;
bool        FontPrivate::defaultBold   = false;
bool        FontPrivate::defaultItalic = false;
Color      *FontPrivate::defaultColor  = &FontPrivate::defaultColorTmp;

Color FontPrivate::defaultColorTmp(255, 255, 255, 255);

bool Font::doesExist(const char *name)
{
	return shState->fontState().fontPresent(name);
}

Font::Font(const char *name,
           int size)
{
	p = new FontPrivate(name, size);
}

Font::Font(const Font &other)
{
	p = new FontPrivate(*other.p);
}

Font::~Font()
{
	delete p;
}

const char *Font::getName() const
{
	return p->name.c_str();
}

void Font::setName(const char *value)
{
	if (p->name == value)
		return;

	p->name = value;
	p->sdlFont = 0;
}

void Font::setSize(int value)
{
	if (p->size == value)
		return;

	/* Catch illegal values (according to RMXP) */
	if (value < 6 || value > 96)
		throw Exception(Exception::ArgumentError, "%s", "bad value for size");

	p->size = value;
	p->sdlFont = 0;
}

#undef CHK_DISP
#define CHK_DISP

DEF_ATTR_RD_SIMPLE(Font, Size, int, p->size)
DEF_ATTR_SIMPLE(Font, Bold, bool, p->bold)
DEF_ATTR_SIMPLE(Font, Italic, bool, p->italic)
DEF_ATTR_SIMPLE(Font, Color, Color*, p->color)

DEF_ATTR_SIMPLE_STATIC(Font, DefaultSize, int, FontPrivate::defaultSize)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultBold, bool, FontPrivate::defaultBold)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultItalic, bool, FontPrivate::defaultItalic)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultColor, Color*, FontPrivate::defaultColor)

const char *Font::getDefaultName()
{
	return FontPrivate::defaultName.c_str();
}

void Font::setDefaultName(const char *value)
{
	FontPrivate::defaultName = value;
}

_TTF_Font *Font::getSdlFont()
{
	if (!p->sdlFont)
		p->sdlFont = shState->fontState().getFont(p->name.c_str(),
		                                          p->size);

	int style = TTF_STYLE_NORMAL;

	if (p->bold)
		style |= TTF_STYLE_BOLD;

	if (p->italic)
		style |= TTF_STYLE_ITALIC;

	TTF_SetFontStyle(p->sdlFont, style);

	return p->sdlFont;
}
