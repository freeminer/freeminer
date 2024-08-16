// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IImageWriter.h"

namespace irr
{
namespace video
{

class CImageWriterPNG : public IImageWriter
{
public:
	//! constructor
	CImageWriterPNG();

	//! return true if this writer can write a file with the given extension
	bool isAWriteableFileExtension(const io::path &filename) const override;

	//! write image to file
	bool writeImage(io::IWriteFile *file, IImage *image, u32 param) const override;
};

} // namespace video
} // namespace irr
