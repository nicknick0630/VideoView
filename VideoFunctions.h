#ifndef _VIDEO_FUNCTIONS_H
#define _VIDEO_FUNCTIONS_H

#include "Matrix.h"

#include <QImage>

#include <vector>
#include <mutex>

namespace video
{
	#define IMAGE_HEIGHT 256
	#define IMAGE_WIDTH 256

	#define SUB_BLOCK_SIZE 30

	#define BLOCK_SIZE 8

	#define COMPRESS_SHOW_BLOCK_SIZE 64

	#define INTERVAL 150

	#define GOP_SIZE 5

	#define PDC_THRESHOLD 8
	#define COMPRESS_THRESHOLD 15
	
	#define RED (std::vector<img::BYTE>{ (img::BYTE)255,(img::BYTE)0 ,(img::BYTE)0 })

	using M_VECTOR = std::pair<int, int>;
	using MOTION_VECTOR = std::vector<std::vector<M_VECTOR>>;

	// test the size of two matrix is equal
	bool testMatrixSizeIsEqual(const img::Matrix &a, const img::Matrix &b);

	// pel difference classification
	int PDC(const img::Matrix &a, unsigned ar, unsigned ac, const img::Matrix &b, unsigned br, unsigned bc, const int color);

	// mean absolute difference
	int MAD(const img::Matrix &a, unsigned ar, unsigned ac, const img::Matrix &b, unsigned br, unsigned bc, const int color);

	// transfer matrix data to Qimage data
	void transferDataToQImage(const img::Matrix &data, QImage &qimg);

	void transferMonoToRGB(img::Matrix &data);


	void drawLine(img::Matrix &motion_vector_matrix, unsigned block_r_start, unsigned block_c_start, unsigned block_r_end, unsigned block_c_end);

	void drawSqaure(img::Matrix &data, unsigned block_r, unsigned block_c, std::mutex &m);

	void undoSquare(img::Matrix &data, const img::Matrix &ref_data, unsigned block_r, unsigned block_c, std::mutex &m);
}


#endif
