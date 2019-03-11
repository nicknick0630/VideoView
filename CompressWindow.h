#ifndef _COMPRESS_WINDOW_H
#define _COMPRESS_WINDOW_H

#include <QDialog>
#include <QImage>
#include <QPainter>
#include <QFileDialog>
#include <QLine>
#include <QVector>
#include <QEvent>

#include "PCX.h"
#include "Img_Function.h"
#include "VideoFunctions.h"	// in this project
#include "VideoFunction.h"	// In pcx project for jpeg

#include <vector>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <functional>

#include "ui_CompressWindow.h"

using namespace video;
using namespace std::placeholders;

class CompressWindow : public QDialog
{
	Q_OBJECT

public:
	CompressWindow(QWidget *parent = Q_NULLPTR) : QDialog(parent)
	{
		ui.setupUi(this);

		initializeCoordinateData();

		QObject::connect(this, SIGNAL(showPreviousImage_signal()), this, SLOT(showPreviousImage_slot()));
		QObject::connect(this, SIGNAL(showNowImage_signal()), this, SLOT(showNowImage_slot()));
		QObject::connect(this, SIGNAL(showPreviousBlock_signal()), this, SLOT(showPreviousBlock_slot()));
		QObject::connect(this, SIGNAL(showNowBlock_signal()), this, SLOT(showNowBlock_slot()));
		QObject::connect(this, SIGNAL(showMotionVectorImage_signal()), this, SLOT(showMotionVectorImage_slot()));

		QObject::connect(this, SIGNAL(myupdate()), this, SLOT(update()));

		pen.setStyle(Qt::PenStyle::SolidLine);
		pen.setWidth(2);
		pen.setBrush(Qt::red);
		pen.setCapStyle(Qt::PenCapStyle::RoundCap);
		pen.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
	}

	~CompressWindow() 
	{ 
		
	}

	void paintEvent(QPaintEvent *ev)
	{
		QPainter p{ this };

		if(!previous_img.isNull())
			p.drawImage(pre_img_pos, previous_img);

		if (!now_img.isNull())
			p.drawImage(now_img_pos, now_img);

		if(!motion_vector_img.isNull())
			p.drawImage(motion_vec_pos, motion_vector_img);

		if (!previous_block.isNull())
			p.drawImage(pre_block_pos, previous_block);

		if (!now_block.isNull())
			p.drawImage(now_block_pos, now_block);


		vid_mutex.lock();
		if (is_compressing)
		{
			p.setPen(pen);

			p.drawRect(matching_block_pos.x(), matching_block_pos.y(), BLOCK_SIZE, BLOCK_SIZE);
			p.drawRect(target_block_pos.x(), target_block_pos.y(), BLOCK_SIZE, BLOCK_SIZE);

			for (auto &x : line_list)
			{
				if (x.dx() == 0 && x.dy() == 0)
					p.drawPoint(x.p1());
				else
					p.drawLine(x);
			}
		}
		vid_mutex.unlock();
	}

signals: /* -------- show signal functions -------- */

	void showPreviousImage_signal(void);

	void showNowImage_signal(void);

	void showPreviousBlock_signal(void);

	void showNowBlock_signal(void);

	void showMotionVectorImage_signal(void);
	
	void myupdate();

public slots: /* -------- button slot functions --------*/

	void openVideoSequence_slot(void)
	{
		auto file_names{ QFileDialog::getOpenFileNames(this, QStringLiteral("選擇影像") , QStringLiteral("../Resource/Images/PCX") , QStringLiteral("Image (*.pcx);")) };

		if (file_names.size() != 0)
		{
			// reset vid_stream
			vid_stream.clear();
			vid_stream.reserve(file_names.size());

			for (auto cnt = 0; cnt < file_names.size(); ++cnt)
			{
				auto &path = file_names[cnt];

				if (!path.isNull())
				{
					std::string fname{ path.toStdString() };

					vid_stream.emplace_back(img::PCX{ fname }.getImgPixelData_r());
				}
			}

			// initialize motion vector
			initializeMotionVectorImage();

			// assign data
			assignPreviousData(0);
			assignNowData(1);

			// show the initial image
			showPreviousImage_slot();
			showNowImage_slot();
			showMotionVectorImage_slot();
		}
	}

	void startCompressButton_slot(void)
	{
		if (vid_stream.size() == 0)
			return;

		if (is_compressing)
			return;

		vid_mutex.lock();
		is_compressing = true;
		is_stopped = false;
		vid_mutex.unlock();

		getOutputFilePath();


		// reserve the buffer of output
		output_buf.reserve(vid_stream.size() * 32 * 32 * 4 + 256 * 256 * 2);

		// output the stream size
		output_buf.emplace_back((unsigned char)vid_stream.size());

		// output the GOP_SIZE
		output_buf.emplace_back((unsigned char)GOP_SIZE);



		std::thread td{ &CompressWindow::compress,this };

		td.detach();
	}

	void stopButton_slot(void)
	{
		vid_mutex.lock();
		is_stopped = true;
		vid_mutex.unlock();
	}

	void continueButton_slot(void)
	{
		vid_mutex.lock();
		is_stopped = false;
		vid_mutex.unlock();
	}

public slots: /* -------- show QImage functions -------- */

	void showPreviousImage_slot(void)
	{
		if (!previous_data)
			assignPreviousData(previous_idx);

		transferDataToQImage(*previous_data, previous_img);

		updatePreImgIdxLabel();

		update();
	}

	void showNowImage_slot(void)
	{
		if (!now_data)
			assignPreviousData(now_idx);

		transferDataToQImage(*now_data, now_img);

		updateNowImgIdxLabel();

		update();
	}


	void showPreviousBlock_slot(void)
	{
		previous_data_block = copyBlock(previous_data, matching_block_show_index.x(), matching_block_show_index.y());

		transferDataToQImage(previous_data_block, previous_block);

		update();
	}

	void showNowBlock_slot(void)
	{
		now_data_block = copyBlock(now_data, target_block_show_index.x(), target_block_show_index.y());

		transferDataToQImage(now_data_block, now_block);

		update();
	}


	void showMotionVectorImage_slot(void)
	{
		transferDataToQImage(motion_vector_matrix, motion_vector_img);

		update();
	}

private: /* -------- private data -------- */

	Ui::CompressWindow ui;

	QImage previous_img;
	QImage now_img;

	QImage previous_block;
	QImage now_block;


	img::Matrix *previous_data{ nullptr };
	img::Matrix *now_data{ nullptr };

	img::Matrix previous_data_block;
	img::Matrix now_data_block;


	img::Matrix motion_vector_matrix;
	QImage motion_vector_img;


	std::vector<img::Matrix> vid_stream;

	std::mutex vid_mutex;

	QPoint pre_img_pos;
	QPoint now_img_pos;
	QPoint pre_block_pos;
	QPoint now_block_pos;
	QPoint motion_vec_pos;

	QPoint matching_block_pos;
	QPoint target_block_pos;

	QPoint target_block_show_index;
	QPoint matching_block_show_index;

	QPen pen;

	QVector<QLine> line_list;

	std::string fpath;
	std::vector<unsigned char> output_buf;

	int previous_idx{ -1 };
	int now_idx{ -1 };

	bool is_compressing{ false };
	bool is_stopped{ true };


private: /* -------- initialization functions --------- */

	void initializeCoordinateData(void)
	{
		pre_img_pos = QPoint{ 50,100 };
		now_img_pos = QPoint{ 580,100 };

		pre_block_pos = QPoint{ 340,200 };
		now_block_pos = QPoint{ 490, 200 };

		motion_vec_pos = QPoint{ 50,450 };
	}

	void initializeMotionVectorImage(void)
	{
		motion_vector_matrix = img::Matrix(vid_stream[0].getRow(), vid_stream[0].getCol(), 3);	// set to RGB, so we can draw color

		unsigned r, c;

		for(r = BLOCK_SIZE/2-1;r<motion_vector_matrix.getRow();r+=BLOCK_SIZE)
			for (c = BLOCK_SIZE / 2 - 1; c < motion_vector_matrix.getCol(); c += BLOCK_SIZE)
			{
				motion_vector_matrix[r][c] = motion_vector_matrix[r][c + 1] = std::vector<img::BYTE>(3,(img::BYTE)0);
				motion_vector_matrix[r + 1][c] = motion_vector_matrix[r + 1][c + 1] = std::vector<img::BYTE>(3, (img::BYTE)0);
			}
	}


private: /* -------- assignment and update data -------- */

	void assignPreviousData(int idx)
	{
		if (!checkIndex(idx))
			throw "index out of range";

		previous_idx = idx;

		previous_data = &vid_stream[idx];
	}

	void assignNowData(int idx)
	{
		if (!checkIndex(idx))
			throw "index out of range";

		now_idx = idx;

		now_data = &vid_stream[idx];
	}

	void updatePreImgIdxLabel(void)
	{
		auto num = std::to_string(vid_stream.size());

		auto str = std::to_string(previous_idx + 1) + " / " + num;

		ui.pre_idx_label->setText(str.c_str());
	}

	void updateNowImgIdxLabel(void)
	{
		auto num = std::to_string(vid_stream.size());

		auto str = std::to_string(now_idx + 1) + " / " + num;

		ui.now_idx_label->setText(str.c_str());
	}


private: /* -------- helper function -------- */

	bool checkIndex(int idx) const
	{
		if (idx < 0 || idx >= vid_stream.size())
			return false;
		else
			return true;
	}

	void resizeMotionVector(MOTION_VECTOR &motion_vector) const
	{
		unsigned row = static_cast<unsigned>(ceil(1.0*vid_stream[0].getRow() / BLOCK_SIZE));
		unsigned col = static_cast<unsigned>(ceil(1.0*vid_stream[0].getCol() / BLOCK_SIZE));

		motion_vector.clear();

		motion_vector = MOTION_VECTOR(row, std::vector<M_VECTOR>(col));
	}

	img::Matrix copyBlock(img::Matrix *pdata, unsigned rt, unsigned ct) const 
	{
		auto &data = *pdata;

		img::Matrix b{ BLOCK_SIZE,BLOCK_SIZE,data.getColor() };

		for(auto r=0;r<BLOCK_SIZE;++r)
			for(auto c=0;c<BLOCK_SIZE;++c)
				for (auto color = 0; color < data.getColor(); ++color)
					b[r][c][color] = data[rt + r][ct + c][color];

		return img::ImgFunction::resize(b, 1.0* COMPRESS_SHOW_BLOCK_SIZE / BLOCK_SIZE);
	}

	void getOutputFilePath(void)
	{
		QString path = QFileDialog::getSaveFileName(this, QStringLiteral("輸出的黨名"), "../Resource/Output", "Images ( *.mymv )");

		std::string file{ path.toStdString() };

		auto idx = file.find_last_of('.');

		
		if (idx == std::string::npos)
			fpath = file + ".mymv";
		else
			fpath = file;
	}

	std::function< MOTION_VECTOR(const img::Matrix &, const img::Matrix &, const int)> getCompressionFunction(void)
	{
		std::function< MOTION_VECTOR(const img::Matrix &, const img::Matrix &, const int) > func;

		switch (ui.comboBox->currentIndex())
		{
		case 0:
			func = std::bind(&CompressWindow::exhaustedSearch, this, _1, _2, _3, true);
			break;
		case 1:
			func = std::bind(&CompressWindow::exhaustedSearchQuick, this, _1, _2, _3, true);
			break;
		case 2:
			func = std::bind(&CompressWindow::TDLSearch, this, _1, _2, _3, true);
			break;
		case 3:
			func = std::bind(&CompressWindow::TSS, this, _1, _2, _3, true);
			break;
		case 4:
			func = std::bind(&CompressWindow::CSASearch, this, _1, _2, _3, true);
			break;
		

		case 5:
			func = std::bind(&CompressWindow::exhaustedSearch, this, _1, _2, _3, false);
			break;
		case 6:
			func = std::bind(&CompressWindow::exhaustedSearchQuick, this, _1, _2, _3, false);
			break;
		case 7:
			func = std::bind(&CompressWindow::TDLSearch, this, _1, _2, _3, false);
			break;
		case 8:
			func = std::bind(&CompressWindow::TSS, this, _1, _2, _3, false);
			break;
		case 9:
			func = std::bind(&CompressWindow::CSASearch, this, _1, _2, _3, false);
			break;
		}

		return func;
	}

private: /* -------- compression method -------- */

	void compress(void)
	{
		auto func = getCompressionFunction();

		for (unsigned idx = 0; idx < vid_stream.size(); ++idx)
		{
			if (idx % GOP_SIZE == 0)
			{
				auto seqs = my_video::VidFunction::jpegEncoding(vid_stream[idx]);
				my_video::VidFunction::outputJpegCode(seqs, output_buf);
			}
			else
			{
				line_list.clear();

				assignPreviousData(idx - 1);
				assignNowData(idx);
				showPreviousImage_signal();
				showNowImage_slot();

				auto motion_vector = func(vid_stream[idx - 1], vid_stream[idx], 0);

				outputMotion(motion_vector);

				generateImageWithMotionVector(motion_vector, vid_stream[idx - 1], vid_stream[idx]);
			}
		}


		std::ofstream fout{ fpath,std::ios::binary };

		if (!fout)
		{
			throw "output file error";
		}

		fout.write((const char*)output_buf.data(), output_buf.size());

		fout.close();
	}

	void waitToContinue(void)
	{
		while (is_stopped)
			std::this_thread::sleep_for(std::chrono::microseconds(100));
	}

	void storeAndDrawMotionVector(MOTION_VECTOR &motion_vector, unsigned r, unsigned c, unsigned r_dif, unsigned c_dif)
	{
		motion_vector[r / BLOCK_SIZE][c / BLOCK_SIZE] = M_VECTOR(r_dif, c_dif);

		int start_y = motion_vec_pos.y() + BLOCK_SIZE / 2;
		int start_x = motion_vec_pos.x() + BLOCK_SIZE / 2;

		line_list.push_back(QLine(start_x + c, start_y + r, start_x + c + c_dif, start_y + r + r_dif));

		myupdate();
	}

	void printBlockAndUpdate(void)
	{
		showNowBlock_signal();
		showPreviousBlock_signal();
		myupdate();
	}

private: /* -------- exhausted search --------*/

	MOTION_VECTOR exhaustedSearch(const img::Matrix &ref_data, const img::Matrix &data, const int color, bool display)
	{
		MOTION_VECTOR motion_vector;
		resizeMotionVector(motion_vector);

		unsigned r, c;
		unsigned ref_r, ref_c;

		unsigned min_r = -1, min_c = -1;

		int difference;
		int dif;

		for (r = 0; r < data.getRow(); r += BLOCK_SIZE)
			for (c = 0; c < data.getCol(); c += BLOCK_SIZE)
			{
				// draw
				drawTargetBlock(display, r, c);


				// using MAD to find less difference, which is the answer
				min_r = 0; min_c = 0;
				difference = 1000000;

				for (ref_r = 0; ref_r <= ref_data.getRow() - BLOCK_SIZE; ++ref_r)
					for (ref_c = 0; ref_c <= ref_data.getCol() - BLOCK_SIZE; ++ref_c)
					{
						// draw
						drawMatchingBlock(display, ref_r, ref_c);


						dif = MAD(ref_data, ref_r, ref_c, data, r, c, color);

						if (dif < difference)
						{
							min_r = ref_r;
							min_c = ref_c;
							difference = dif;
						}

						if (is_stopped)
							waitToContinue();
					}

				storeAndDrawMotionVector(motion_vector, r, c, min_r - r, min_c - c);
			}

		return motion_vector;
	}

	MOTION_VECTOR exhaustedSearchQuick(const img::Matrix &ref_data, const img::Matrix &data, const int color, bool display)
	{
		MOTION_VECTOR motion_vector;
		resizeMotionVector(motion_vector);

		auto func = [&](unsigned r_start, unsigned r_end)
		{
			unsigned r, c;
			unsigned ref_r, ref_c;

			unsigned min_r = -1, min_c = -1;

			int difference;
			int dif;

			for (r = r_start; r < r_end; r += BLOCK_SIZE)
				for (c = 0; c < data.getCol(); c += BLOCK_SIZE)
				{
					// draw
					drawTargetBlock(display, r, c);


					// check the position of itself
					dif = PDC(ref_data, r, c, data, r, c, color);
					if (dif >= BLOCK_SIZE * BLOCK_SIZE - COMPRESS_THRESHOLD)
					{
						storeAndDrawMotionVector(motion_vector, r, c, 0, 0);

						continue;
					}

					// using MAD to find less difference, which is the answer
					min_r = 0; min_c = 0;
					difference = 1000000;

					for (ref_r = 0; ref_r <= ref_data.getRow() - BLOCK_SIZE; ++ref_r)
						for (ref_c = 0; ref_c <= ref_data.getCol() - BLOCK_SIZE; ++ref_c)
						{
							// draw
							drawMatchingBlock(display, ref_r, ref_c);


							dif = MAD(ref_data, ref_r, ref_c, data, r, c, color);

							if (dif < difference)
							{
								min_r = ref_r;
								min_c = ref_c;
								difference = dif;
							}

							if (is_stopped)
								waitToContinue();
						}

					storeAndDrawMotionVector(motion_vector, r, c, min_r - r, min_c - c);
				}
		};

		std::thread td1{ func,0,data.getRow() };
		//std::thread td2{ func,data.getRow() / 2,data.getRow() };

		td1.join();
		//td2.join();

		return motion_vector;
	}


private: /* -------- Two Dimensional Logarithmic Search -------- */

	MOTION_VECTOR TDLSearch(const img::Matrix &ref_data, const img::Matrix &data, const int color, bool display)
	{
		MOTION_VECTOR motion_vector;
		resizeMotionVector(motion_vector);

		int r, c;
		int min_r, min_c;

		int step = 4;

		for (r = 0; r < data.getRow(); r += BLOCK_SIZE)
			for (c = 0; c < data.getCol(); c += BLOCK_SIZE)
			{
				// draw
				drawTargetBlock(display, r, c);


				step = 4;
				min_r = r;
				min_c = c;

				while (step != 1)
				{
					if (is_stopped)
						waitToContinue();

					auto point = TDL_stage1(ref_data, data, color, min_r, min_c, step, display);

					min_r = point.x();
					min_c = point.y();

					step /= 2;
				}

				auto point = TDL_stage3(ref_data, data, color, min_r, min_c, display);

				std::this_thread::sleep_for(std::chrono::microseconds(10));
				

				storeAndDrawMotionVector(motion_vector, r, c, min_r - r, min_c - c);
			}

		return motion_vector;
	}

	QPoint TDL_stage1(const img::Matrix &ref_data, const img::Matrix &data, const int color, int r, int c, int step, bool display)
	{
		int dif;
		int min = 1000000000;
		int min_r = -1, min_c = -1;

		int r_t, c_t;

		std::vector<M_VECTOR> arr{ {0,0},{0,-step},{-step,0},{0,step},{step,0} };

		for (auto cnt = 0; cnt < arr.size(); ++cnt)
		{
			r_t = r + arr[cnt].first;
			c_t = c + arr[cnt].second;
			if (tdl_checkBoarder(r_t, c_t))
			{
				// draw
				drawMatchingBlock(display, r_t, c_t);


				dif = MAD(ref_data, r_t, c_t, data, r, c, color);

				if (dif < min)
				{
					min = dif;
					min_r = r_t;
					min_c = c_t;
				}
			}
		}

		return QPoint{ min_r,min_c };
	}

	QPoint TDL_stage3(const img::Matrix &ref_data, const img::Matrix &data, const int color, int r, int c, bool display)
	{
		int dif;
		int min = 1000000;
		int min_r = -1, min_c = -1;

		int r_t, c_t;

		std::vector<M_VECTOR> arr{ {-1,-1},{-1,0},{-1,1},{0,-1},{0,0},{0,1},{1,-1},{1,0},{1,1} };

		for (auto cnt = 0; cnt < arr.size(); ++cnt)
		{
			r_t = r + arr[cnt].first;
			c_t = c + arr[cnt].second;
			if (tdl_checkBoarder(r_t, c_t))
			{
				// draw
				drawMatchingBlock(display, r_t, c_t);


				dif = MAD(ref_data, r_t, c_t, data, r, c, color);

				if (dif < min)
				{
					min = dif;
					min_r = r_t;
					min_c = c_t;
				}
			}
		}

		return QPoint{ min_r,min_c };
	}

	bool tdl_checkBoarder(int r, int c) const
	{
		if (r < 0 || c < 0)
			return false;
		if (r + BLOCK_SIZE > 256 || c + BLOCK_SIZE > 256)
			return false;
		return true;
	}


private: /* -------- Three Step Search -------- */

	MOTION_VECTOR TSS(const img::Matrix &ref_data, const img::Matrix &data, const int color, bool display)
	{
		MOTION_VECTOR motion_vector;
		resizeMotionVector(motion_vector);

		int r, c;
		int min_r, min_c;

		int step;

		for (r = 0; r < data.getRow(); r += BLOCK_SIZE)
			for (c = 0; c < data.getCol(); c += BLOCK_SIZE)
			{
				// draw
				drawTargetBlock(display, r, c);

				step = 3;
				min_r = r;
				min_c = c;

				while (step != 0)
				{
					if (is_stopped)
						waitToContinue();
	
					auto point = TSS_stage(ref_data, data, color, min_r, min_c, step, display);

					min_r = point.x();
					min_c = point.y();

					step -= 1;
				}

				std::this_thread::sleep_for(std::chrono::microseconds(10));

				storeAndDrawMotionVector(motion_vector, r, c, min_r - r, min_c - c);
			}

		return motion_vector;
	}

	QPoint TSS_stage(const img::Matrix &ref_data, const img::Matrix &data, const int color, int r, int c, int step, bool display)
	{
		int dif;
		int min = 1000000000;
		int min_r = -1, min_c = -1;

		int ref_r, ref_c;

		std::vector<M_VECTOR> arr{ {0,0},{0,-step},{-step,0},{0,step},{step,0},{-step,-step}, {step,-step},{-step,step},{step,step} };

		for (auto cnt = 0; cnt < arr.size(); ++cnt)
		{
			ref_r = r + arr[cnt].first;
			ref_c = c + arr[cnt].second;
		
			// draw
			drawMatchingBlock(display, ref_r, ref_c);
			

			if (tdl_checkBoarder(ref_r, ref_c))
			{
				dif = MAD(ref_data, ref_r, ref_c, data, r, c, color);

				if (dif < min)
				{
					min = dif;
					min_r = ref_r;
					min_c = ref_c;
				}
			}
		}

		return QPoint{ min_r,min_c };
	}


private: /* -------- Cross Search Algorithm -------- */

	MOTION_VECTOR CSASearch(const img::Matrix &ref_data, const img::Matrix &data, const int color, bool display)
	{
		MOTION_VECTOR motion_vector;
		resizeMotionVector(motion_vector);

		int r, c;
		int min_r, min_c;
		int pre_r, pre_c;

		int step = 4;

		for (r = 0; r < data.getRow(); r += BLOCK_SIZE)
			for (c = 0; c < data.getCol(); c += BLOCK_SIZE)
			{
				// draw
				drawTargetBlock(display, r, c);


				step = 4;
				min_r = r;
				min_c = c;

				while (step != 1)
				{
					if (is_stopped)
						waitToContinue();

					auto point = CSA_stage1(ref_data, data, color, min_r, min_c, step, display);

					pre_r = min_r;
					pre_c = min_c;

					min_r = point.x();
					min_c = point.y();

					step /= 2;
				}

				auto point = CSA_stage2(ref_data, data, color, min_r, min_c, pre_r, pre_c, display);

				std::this_thread::sleep_for(std::chrono::microseconds(10));


				storeAndDrawMotionVector(motion_vector, r, c, min_r - r, min_c - c);
			}

		return motion_vector;
	}

	QPoint CSA_stage1(const img::Matrix &ref_data, const img::Matrix &data, const int color, int r, int c, int step, bool display)
	{
		int dif;
		int min = 1000000000;
		int min_r = -1, min_c = -1;

		int r_t, c_t;

		std::vector<M_VECTOR> arr{ {-step,-step}, {step,-step},{-step,step},{step,step} };

		for (auto cnt = 0; cnt < arr.size(); ++cnt)
		{
			r_t = r + arr[cnt].first;
			c_t = c + arr[cnt].second;
			if (tdl_checkBoarder(r_t, c_t))
			{
				// draw
				drawMatchingBlock(display, r_t, c_t);


				dif = MAD(ref_data, r_t, c_t, data, r, c, color);

				if (dif < min)
				{
					min = dif;
					min_r = r_t;
					min_c = c_t;
				}
			}
		}

		return QPoint{ min_r,min_c };
	}

	QPoint CSA_stage2(const img::Matrix &ref_data, const img::Matrix &data, const int color, int r, int c,int pr, int pc, bool display)
	{
		int dif;
		int min = 1000000;
		int min_r = -1, min_c = -1;

		int r_t, c_t;

		std::vector<M_VECTOR> arr1{ {-1,-1},{-1,1},{1,-1},{1,1} };
		std::vector<M_VECTOR> arr2{ {-1,0},{0,-1},{0,1},{1,0} };

		std::vector<M_VECTOR> *parr;

		int dr = r - pr;
		int dc = c - pc;

		if (dr*dc > 0)
			parr = &arr1;
		else
			parr = &arr2;

		auto &arr = *parr;

		for (auto cnt = 0; cnt < arr.size(); ++cnt)
		{
			r_t = r + arr[cnt].first;
			c_t = c + arr[cnt].second;
			if (tdl_checkBoarder(r_t, c_t))
			{
				// draw
				drawMatchingBlock(display, r_t, c_t);


				dif = MAD(ref_data, r_t, c_t, data, r, c, color);

				if (dif < min)
				{
					min = dif;
					min_r = r_t;
					min_c = c_t;
				}
			}
		}

		return QPoint{ min_r,min_c };
	}


private: /* -------- output --------*/

	void outputMatrix(const img::Matrix &data)
	{
		unsigned r, c;

		for (r = 0; r < data.getRow(); ++r)
			for (c = 0; c < data.getCol(); ++c)
				output_buf.emplace_back(data[r][c][0]);
	}

	void outputMotion(const MOTION_VECTOR &data)
	{
		unsigned r, c;

		for(r=0;r<data.size();++r)
			for (c = 0; c < data[0].size(); ++c)
			{
				output16bits(data[r][c].first);
				output16bits(data[r][c].second);
			}
	}

	void output16bits(int num)
	{
		unsigned char *ptr = (unsigned char*)&num;

		output_buf.push_back(*ptr++);
		output_buf.push_back(*ptr);
	}

private:

	void generateImageWithMotionVector(const MOTION_VECTOR &motion, const img::Matrix &ref_data, img::Matrix &ndata) const
	{
		unsigned r, c;
		unsigned rr, cc;
		unsigned i, k;

		for(r=0;r<256;r+=8)
			for (c = 0; c < 256; c += 8)
			{
				auto &p = motion[r / 8][c / 8];

				rr = r + p.first;
				cc = c + p.second;

				for (i = 0; i < 8; ++i)
					for (k = 0; k < 8; ++k)
						ndata[r + i][c + k][0] = ref_data[rr + i][cc + k][0];
			}
	}

	void drawTargetBlock(bool should_display, int r, int c)
	{
		if (should_display)
		{
			// draw
			target_block_pos = QPoint(now_img_pos.x() + c, now_img_pos.y() + r);
			target_block_show_index = QPoint(r, c);
			printBlockAndUpdate();
			std::this_thread::sleep_for(std::chrono::microseconds(15));
		}
	}

	void drawMatchingBlock(bool should_display, int ref_r, int ref_c)
	{
		if (should_display)
		{
			// draw
			matching_block_pos = QPoint(pre_img_pos.x() + ref_c, pre_img_pos.y() + ref_r);
			matching_block_show_index = QPoint(ref_r, ref_c);
			printBlockAndUpdate();
			std::this_thread::sleep_for(std::chrono::microseconds(15));
		}
	}
};

#endif