/**
* Copyright (c) 2006-2012 LOVE Development Team
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would be
*    appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
**/
#include <common/config.h>
#include "Font.h"
#include <font/GlyphData.h>
#include "Quad.h"

#include <libraries/utf8/utf8.h>

#include <common/math.h>
#include <common/Matrix.h>
#include <math.h>

#include <sstream>

#include <algorithm> // for max

namespace love
{
namespace graphics
{
namespace opengl
{

	Font::Font(love::font::Rasterizer * r, const Image::Filter& filter)
	: rasterizer(r), height(r->getHeight()), lineHeight(1), mSpacing(1), filter(filter)
	{
		r->retain();
		love::font::GlyphData * gd = r->getGlyphData(32);
		type = (gd->getFormat() == love::font::GlyphData::FORMAT_LUMINANCE_ALPHA ? FONT_TRUETYPE : FONT_IMAGE);
		delete gd;
		createTexture();
	}

	Font::~Font()
	{
		rasterizer->release();
		unloadVolatile();
	}

	void Font::createTexture()
	{
		texture_x = texture_y = rowHeight = TEXTURE_PADDING;
		GLuint t;
		glGenTextures(1, &t);
		textures.push_back(t);
		bindTexture(t);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
						(filter.mag == Image::FILTER_LINEAR) ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						(filter.min == Image::FILTER_LINEAR) ? GL_LINEAR : GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		GLint format = (type == FONT_TRUETYPE ? GL_LUMINANCE_ALPHA : GL_RGBA);
		// Initialize the texture
		glTexImage2D(GL_TEXTURE_2D,
					 0,
					 GL_RGBA,
					 (GLsizei)TEXTURE_WIDTH,
					 (GLsizei)TEXTURE_HEIGHT,
					 0,
					 format,
					 GL_UNSIGNED_BYTE,
					 NULL);
		// Fill the texture with transparent black
		std::vector<GLubyte> emptyData(TEXTURE_WIDTH * TEXTURE_HEIGHT * (type == FONT_TRUETYPE ? 2 : 4), 0);
		glTexSubImage2D(GL_TEXTURE_2D,
						0,
						0,
						0,
						(GLsizei)TEXTURE_WIDTH,
						(GLsizei)TEXTURE_HEIGHT,
						format,
						GL_UNSIGNED_BYTE,
						&emptyData[0]);
	}

	Font::Glyph * Font::addGlyph(int glyph)
	{
		Glyph * g = new Glyph;
		g->list = glGenLists(1);
		if (g->list == 0)
		{ // opengl failed to generate the list
			delete g;
			return NULL;
		}
		love::font::GlyphData *gd = rasterizer->getGlyphData(glyph);
		g->spacing = gd->getAdvance();
		int w = gd->getWidth();
		int h = gd->getHeight();
		if (texture_x + w + TEXTURE_PADDING > TEXTURE_WIDTH)
		{ // out of space - new row!
			texture_x = TEXTURE_PADDING;
			texture_y += rowHeight;
			rowHeight = TEXTURE_PADDING;
		}
		if (texture_y + h + TEXTURE_PADDING > TEXTURE_HEIGHT)
		{ // totally out of space - new texture!
			createTexture();
		}
		GLuint t = textures.back();
		bindTexture(t);
		glTexSubImage2D(GL_TEXTURE_2D, 0, texture_x, texture_y, w, h, (type == FONT_TRUETYPE ? GL_LUMINANCE_ALPHA : GL_RGBA), GL_UNSIGNED_BYTE, gd->getData());

		g->texture = t;

		Quad::Viewport v;
		v.x = (float) texture_x;
		v.y = (float) texture_y;
		v.w = (float) w;
		v.h = (float) h;
		Quad * q = new Quad(v, (const float) TEXTURE_WIDTH, (const float) TEXTURE_HEIGHT);
		const vertex * verts = q->getVertices();

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(2, GL_FLOAT, sizeof(vertex), (GLvoid *)&verts[0].x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), (GLvoid *)&verts[0].s);

		glNewList(g->list, GL_COMPILE);
		glPushMatrix();
		glTranslatef(static_cast<float>(gd->getBearingX()), static_cast<float>(-gd->getBearingY()), 0.0f);
		glDrawArrays(GL_QUADS, 0, 4);
		glPopMatrix();
		glEndList();

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		delete q;
		delete gd;

		texture_x += (w + TEXTURE_PADDING);
		rowHeight = std::max(rowHeight, h + TEXTURE_PADDING);

		glyphs[glyph] = g;
		return g;
	}

	float Font::getHeight() const
	{
		return static_cast<float>(height);
	}

	void Font::print(std::string text, float x, float y, float angle, float sx, float sy, float ox, float oy, float kx, float ky)
	{
		float dx = 0.0f; // spacing counter for newline handling
		glPushMatrix();

		Matrix t;
		t.setTransformation(ceil(x), ceil(y), angle, sx, sy, ox, oy, kx, ky);
		glMultMatrixf((const GLfloat*)t.getElements());
		try
		{
			utf8::iterator<std::string::iterator> i (text.begin(), text.begin(), text.end());
			utf8::iterator<std::string::iterator> end (text.end(), text.begin(), text.end());
			while (i != end)
			{
				int g = *i++;
				if (g == '\n')
				{ // wrap newline, but do not print it
					glTranslatef(-dx, floor(getHeight() * getLineHeight() + 0.5f), 0);
					dx = 0.0f;
					continue;
				}
				Glyph * glyph = glyphs[g];
				if (!glyph) glyph = addGlyph(g);
				glPushMatrix();
				// 1.25 is magic line height for true type fonts
				if (type == FONT_TRUETYPE) glTranslatef(0, floor(getHeight() / 1.25f + 0.5f), 0);
				bindTexture(glyph->texture);
				glCallList(glyph->list);
				glPopMatrix();
				glTranslatef(static_cast<GLfloat>(glyph->spacing), 0, 0);
				dx += glyph->spacing;
			}
		}
		catch (utf8::exception & e)
		{
			glPopMatrix();
			throw love::Exception("%s", e.what());
		}
		glPopMatrix();
	}

	void Font::print(char character, float x, float y)
	{
		Glyph * glyph = glyphs[character];
		if (!glyph) glyph = addGlyph(character);
		glPushMatrix();
		glTranslatef(x, floor(y+getHeight() + 0.5f), 0.0f);
		bindTexture(glyph->texture);
		glCallList(glyph->list);
		glPopMatrix();
	}

	int Font::getWidth(const std::string & line)
	{
		if (line.size() == 0) return 0;
		int temp = 0;

		Glyph * g;

		try
		{
			utf8::iterator<std::string::const_iterator> i (line.begin(), line.begin(), line.end());
			utf8::iterator<std::string::const_iterator> end (line.end(), line.begin(), line.end());
			while (i != end)
			{
				int c = *i++;
				g = glyphs[c];
				if (!g) g = addGlyph(c);
				temp += static_cast<int>(g->spacing * mSpacing);
			}
		}
		catch (utf8::exception & e)
		{
			throw love::Exception("%s", e.what());
		}

		return temp;
	}

	int Font::getWidth(const char * line)
	{
		return this->getWidth(std::string(line));
	}

	int Font::getWidth(const char character)
	{
		Glyph * g = glyphs[character];
		if (!g) g = addGlyph(character);
		return g->spacing;
	}

	std::vector<std::string> Font::getWrap(const std::string text, float wrap, int * max_width)
	{
		using namespace std;
		const float width_space = static_cast<float>(getWidth(' '));
		vector<string> lines_to_draw;
		int maxw = 0;

		//split text at newlines
		istringstream iss( text );
		string line;
		ostringstream string_builder;
		while (getline(iss, line, '\n'))
		{
			// split line into words
			vector<string> words;
			istringstream word_iss(line);
			copy(istream_iterator<string>(word_iss), istream_iterator<string>(),
					back_inserter< vector<string> >(words));

			// put words back together until a wrap occurs
			float width = 0.0f;
			float oldwidth = 0.0f;
			string_builder.str("");
			vector<string>::const_iterator word_iter, wend = words.end();
			for (word_iter = words.begin(); word_iter != wend; ++word_iter)
			{
				const string& word = *word_iter;
				width += getWidth( word );

				// on wordwrap, push line to line buffer and clear string builder
				if (width >= wrap && oldwidth > 0)
				{
					int realw = (int) width;

					// remove trailing space
					string tmp = string_builder.str();
					lines_to_draw.push_back( tmp.substr(0,tmp.size()-1) );
					string_builder.str("");
					width = static_cast<float>(getWidth( word ));
					realw -= (int) width;
					if (realw > maxw)
						maxw = realw;
				}
				string_builder << word << " ";
				width += width_space;
				oldwidth = width;
			}
			// push last line
			if (width > maxw)
				maxw = (int) width;
			string tmp = string_builder.str();
			lines_to_draw.push_back( tmp.substr(0,tmp.size()-1) );
		}

		if (max_width)
			*max_width = maxw;

		return lines_to_draw;
	}

	void Font::setLineHeight(float height)
	{
		this->lineHeight = height;
	}

	float Font::getLineHeight() const
	{
		return lineHeight;
	}

	void Font::setSpacing(float amount)
	{
		mSpacing = amount;
	}

	float Font::getSpacing() const
	{
		return mSpacing;
	}

	bool Font::loadVolatile()
	{
		createTexture();
		return true;
	}

	void Font::unloadVolatile()
	{
		// nuke everything from orbit
		std::map<int, Glyph *>::iterator it = glyphs.begin();
		Glyph * g;
		while (it != glyphs.end())
		{
			g = it->second;
			glDeleteLists(g->list, 1);
			delete g;
			glyphs.erase(it++);
		}
		std::vector<GLuint>::iterator iter = textures.begin();
		while (iter != textures.end())
		{
			deleteTexture(*iter);
			iter++;
		}
		textures.clear();
	}

} // opengl
} // graphics
} // love
