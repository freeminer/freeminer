#pragma once

#include <png.h>
#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#include <optional>
#include <cstdint>
//#include <iostream>
#include "SColor.h"

//struct RGBA {uint8_t r, g, b, a;};
using RGBA = video::SColor;

class PngImage
{
public:
	// open and read PNG, convert to 8-bit RGBA
	explicit PngImage(const std::string &filename)
	{
		fp_ = std::fopen(filename.c_str(), "rb");
		if (!fp_)
			throw std::runtime_error("Failed to open file: " + filename);

		png_ptr_ =
				png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		if (!png_ptr_) {
			std::fclose(fp_);
			fp_ = nullptr;
			throw std::runtime_error("png_create_read_struct failed");
		}

		info_ptr_ = png_create_info_struct(png_ptr_);
		if (!info_ptr_) {
			png_destroy_read_struct(&png_ptr_, nullptr, nullptr);
			std::fclose(fp_);
			fp_ = nullptr;
			throw std::runtime_error("png_create_info_struct failed");
		}

		if (setjmp(png_jmpbuf(png_ptr_))) {
			// libpng error
			cleanup();
			throw std::runtime_error("libpng error during init/read");
		}

		png_init_io(png_ptr_, fp_);
		png_read_info(png_ptr_, info_ptr_);

		png_uint_32 w, h;
		int bit_depth, color_type, interlace_type, compression_type, filter_method;
		png_get_IHDR(png_ptr_, info_ptr_, &w, &h, &bit_depth, &color_type,
				&interlace_type, &compression_type, &filter_method);

		width_ = static_cast<int>(w);
		height_ = static_cast<int>(h);

		// Transformations to 8-bit RGBA:
		// Expand palette, grayscale < 8, and tRNS to RGB(A)
		png_set_expand(png_ptr_);

		// Convert 16-bit to 8-bit
		if (bit_depth == 16)
			png_set_strip_16(png_ptr_);

		// Convert gray/gray+alpha to RGB(A)
		if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(png_ptr_);

		// Ensure we have alpha: if image has no alpha, add an opaque alpha channel
		if (!(color_type & PNG_COLOR_MASK_ALPHA)) {
			// Put alpha after RGB (i.e., RGBA)
			png_set_filler(png_ptr_, 0xFF, PNG_FILLER_AFTER);
		}

		// Handle interlace
		png_set_interlace_handling(png_ptr_);

		// Update info after transformations
		png_read_update_info(png_ptr_, info_ptr_);

		// Get bytes per row (after transforms)
		png_uint_32 rowbytes = png_get_rowbytes(png_ptr_, info_ptr_);
		if (rowbytes == 0) {
			cleanup();
			throw std::runtime_error("Invalid rowbytes");
		}

		rowbytes_ = static_cast<size_t>(rowbytes);
		// allocate one contiguous buffer for all image data
		image_data_.resize(rowbytes_ * static_cast<size_t>(height_));
		row_pointers_.resize(height_);

		for (int i = 0; i < height_; ++i) {
			row_pointers_[i] = image_data_.data() + (size_t)i * rowbytes_;
		}

		// Read the image into our buffer
		png_read_image(png_ptr_, row_pointers_.data());

		// After read, we expect 4 bytes per pixel (RGBA)
		// But verify: rowbytes must be width * 4 (or at least enough)
		if (rowbytes_ < static_cast<size_t>(width_) * 4) {
			cleanup();
			throw std::runtime_error("Unexpected row size after transformation");
		}
	}

	// Disable copy
	PngImage(const PngImage &) = delete;
	PngImage &operator=(const PngImage &) = delete;

	// Enable move
	PngImage(PngImage &&other) noexcept { steal_from(std::move(other)); }
	PngImage &operator=(PngImage &&other) noexcept
	{
		if (this != &other) {
			cleanup();
			steal_from(std::move(other));
		}
		return *this;
	}

	~PngImage() { cleanup(); }

	int width() const noexcept { return width_; }
	int height() const noexcept { return height_; }

	// Return RGBA for given coords, or std::nullopt if out of bounds.
	std::optional<RGBA> get_pixel(int x, int y) const noexcept
	{
		if (x < 0 || y < 0 || x >= width_ || y >= height_)
			return std::nullopt;
		const uint8_t *row = row_pointers_[y];
		const uint8_t *px =
				row + static_cast<size_t>(x) * 4; // 4 bytes per pixel (R,G,B,A)
		RGBA c{px[3], px[0], px[1], px[2]};
		return c;
	}

private:
	png_structp png_ptr_ = nullptr;
	png_infop info_ptr_ = nullptr;
	FILE *fp_ = nullptr;

	int width_ = 0;
	int height_ = 0;
	size_t rowbytes_ = 0;

	std::vector<uint8_t> image_data_;
	std::vector<png_bytep> row_pointers_;

	void cleanup() noexcept
	{
		if (png_ptr_ || info_ptr_) {
			// png_destroy_read_struct frees info_ptr_ as well
			png_destroy_read_struct(&png_ptr_, &info_ptr_, nullptr);
			png_ptr_ = nullptr;
			info_ptr_ = nullptr;
		}
		if (fp_) {
			std::fclose(fp_);
			fp_ = nullptr;
		}
		image_data_.clear();
		row_pointers_.clear();
	}

	void steal_from(PngImage &&other) noexcept
	{
		png_ptr_ = other.png_ptr_;
		info_ptr_ = other.info_ptr_;
		fp_ = other.fp_;
		width_ = other.width_;
		height_ = other.height_;
		rowbytes_ = other.rowbytes_;
		image_data_ = std::move(other.image_data_);
		row_pointers_ = std::move(other.row_pointers_);

		other.png_ptr_ = nullptr;
		other.info_ptr_ = nullptr;
		other.fp_ = nullptr;
		other.width_ = other.height_ = 0;
		other.rowbytes_ = 0;
	}
};
