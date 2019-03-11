
#include "VideoFunctions.h"

#include <cmath>

namespace video
{
	// test the size of two matrix is equal
	bool testMatrixSizeIsEqual(const img::Matrix &a, const img::Matrix &b)
	{
		if (a.getRow() != b.getRow() || a.getCol() != b.getCol())
			return false;

		return true;
	}

	// pel difference classification
	int PDC(const img::Matrix &a, unsigned ar, unsigned ac, const img::Matrix &b, unsigned br, unsigned bc, const int color)
	{
		if (!testMatrixSizeIsEqual(a, b))
			return -1;

		int sum = 0;
		int tem;

		for (unsigned r = 0; r < BLOCK_SIZE; ++r)
			for (unsigned c = 0; c < BLOCK_SIZE; ++c)
			{
				tem = abs(a[r + ar][c + ac][color] - b[r + br][c + bc][color]);

				if (tem <= PDC_THRESHOLD)
					sum++;
			}

		return sum;
	}

	// mean absolute difference
	int MAD(const img::Matrix &a, unsigned ar, unsigned ac, const img::Matrix &b, unsigned br, unsigned bc, const int color)
	{
		if (!testMatrixSizeIsEqual(a, b))
			return -1;

		int sum = 0;
		int tem;

		for (unsigned r = 0; r < BLOCK_SIZE; ++r)
			for (unsigned c = 0; c < BLOCK_SIZE; ++c)
			{
				tem = abs(a[r + ar][c + ac][color] - b[r + br][c + bc][color]);
				sum += tem;
			}

		return sum;
	}

	// transfer matrix data to Qimage data
	void transferDataToQImage(const img::Matrix &data, QImage &qimg)
	{
		QImage::Format format{ ((data.getColor() == 1) ? QImage::Format_Grayscale8 : QImage::Format_RGB888) };

		qimg = QImage(data.getCol(), data.getRow(), format);

		unsigned r, c, color;


		for (r = 0; r < data.getRow(); ++r)
		{
			// use ScanLine , because QImage inner data will be set alignment by 4 and prone to error
			uchar *ptr = qimg.scanLine(r);

			for (c = 0; c < data.getCol(); ++c)
				for (color = 0; color < data.getColor(); ++color)
					*(ptr++) = data[r][c][color];
		}
	}

	void transferMonoToRGB(img::Matrix &data)
	{
		if (data.getColor() == 3)
			return;

		img::Matrix tem(data.getRow(), data.getCol(), 3);

		unsigned r, c, color;

		for (r = 0; r < tem.getRow(); ++r)
			for (c = 0; c < tem.getCol(); ++c)
				for (color = 0; color < 3; ++color)
					tem[r][c][color] = data[r][c][0];

		data = std::move(tem);
	}


	void drawLine(img::Matrix &motion_vector_matrix, unsigned block_r_start, unsigned block_c_start, unsigned block_r_end, unsigned block_c_end)
	{
		unsigned start = BLOCK_SIZE / 2 - 1;

		unsigned r_start = start + block_r_start * BLOCK_SIZE;
		unsigned c_start = start + block_c_start * BLOCK_SIZE;
		unsigned r_end = start + block_r_end * BLOCK_SIZE;
		unsigned c_end = start + block_c_end * BLOCK_SIZE;

		if (c_end < c_start)
		{
			std::swap(r_start, r_end);
			std::swap(c_start, c_end);
		}

		const double r_dif = 1.0 * (r_end - r_start) / (c_end - c_start);

		unsigned x;
		double y;

		for (x = c_start, y = r_start; x <= c_end; ++x, y += r_dif)
		{
			// draw a bold line
			motion_vector_matrix[(unsigned)round(y)][x] = RED;
			motion_vector_matrix[(unsigned)round(y) + 1][x] = RED;
		}
	}
}