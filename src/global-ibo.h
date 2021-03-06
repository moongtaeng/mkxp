/*
** global-ibo.h
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

#ifndef GLOBALIBO_H
#define GLOBALIBO_H

#include "gl-util.h"

#include <vector>

struct GlobalIBO
{
	IBO::ID ibo;
	std::vector<uint32_t> buffer;

	GlobalIBO()
	{
		ibo = IBO::gen();
	}

	~GlobalIBO()
	{
		IBO::del(ibo);
	}

	void ensureSize(size_t quadCount)
	{
		if (buffer.size() >= quadCount*6)
			return;

		size_t startInd = buffer.size() / 6;
		buffer.reserve(quadCount*6);

		for (size_t i = startInd; i < quadCount; ++i)
		{
			static const uint32_t indTemp[] = { 0, 1, 2, 2, 3, 0 };

			for (size_t j = 0; j < 6; ++j)
				buffer.push_back(i * 4 + indTemp[j]);
		}

		IBO::bind(ibo);
		IBO::uploadData(buffer.size() * sizeof(uint32_t), &buffer[0]);
		IBO::unbind();
	}
};

#endif // GLOBALIBO_H
