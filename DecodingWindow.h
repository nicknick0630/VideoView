#ifndef _DECODEING_WINDOW_H
#define _DECODEING_WINDOW_H

#include <QDialog>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QVector>
#include <QLine>
#include <QPen>
#include <QChart>
#include <QScatterSeries>
#include <QChartView>
#include <QValueAxis>

#include <vector>
#include <mutex>
#include <thread>

#include "VideoFunctions.h"	// in this project
#include "VideoFunction.h"	// In pcx project for jpeg
#include "PCX.h"

#include "ui_DecodingWindow.h"

#include <QDebug> ///////////////////////////////////////////////

QT_CHARTS_USE_NAMESPACE

using namespace video;

class DecodingWindow : public QDialog
{
	Q_OBJECT

public:
	DecodingWindow(QWidget *parent = Q_NULLPTR) : QDialog(parent)
	{
		ui.setupUi(this);

		QObject::connect(this, SIGNAL(update_signal()), this, SLOT(update()));
		QObject::connect(this, SIGNAL(moveToNextImage_signal()), this, SLOT(moveToNextImage_slot()));
		QObject::connect(this, SIGNAL(moveToPreImage_signal()), this, SLOT(moveToPreImage_slot()));

		pen.setStyle(Qt::PenStyle::SolidLine);
		pen.setWidth(2);
		pen.setBrush(Qt::red);
		pen.setCapStyle(Qt::PenCapStyle::RoundCap);
		pen.setJoinStyle(Qt::PenJoinStyle::RoundJoin);


		initializeQchartData();
		initializePositionData();
		initializeMotionImageData();
	}

	~DecodingWindow() {}

	void paintEvent(QPaintEvent *ev)
	{
		QPainter p{ this };

		if (!decoded_img.isNull())
			p.drawImage(decoded_img_pos, decoded_img);

		if (!org_img.isNull())
			p.drawImage(org_img_pos, org_img);

		if (!motion_img.isNull())
			p.drawImage(motion_img_pos, motion_img);

		if (!decoded_img.isNull() && !org_img.isNull())
			chart_view->show();
		else
			chart_view->hide();

		if (motion_vector_lines.size() != 0)
		{
			p.setPen(pen);


			for (auto &x : motion_vector_lines)
			{
				if (x.dx() == 0 && x.dy() == 0)
					p.drawPoint(x.p1());
				else
					p.drawLine(x);
			}
		}
	}

signals:

	void update_signal(void);

	void moveToNextImage_signal(void);

	void moveToPreImage_signal(void);

public slots: /* -------- buntton slots -------- */

	void openCodeButton_slot(void)
	{
		auto file_name{ QFileDialog::getOpenFileName(this, QStringLiteral("選擇壓縮檔案") , QStringLiteral("../Resource/Demo_Output") , QStringLiteral("Image (*.mymv);")) };

		if (!file_name.isNull())
		{
			// reset vid_stream
			decoded_img_stream.clear();

			std::string fname{ file_name.toStdString() };


			read(fname);

			now_stream_index = 0;

			calculateMotionVectorLines();

			calculatePSNRs();
			buildChartByPSNRseries();

			// show the initial image
			showDecodedImg_slot();
			showMotionImg_slot();
			showIndexLabel_slot();

			if (org_img_stream.size() != 0)
				updateWindowStatus();

			update_signal();
		}
	}

	void openOrgImgButton_slot(void)
	{
		auto file_names{ QFileDialog::getOpenFileNames(this, QStringLiteral("選擇影像") , QStringLiteral("../Resource/Images/PCX") , QStringLiteral("Image (*.pcx);")) };

		if (file_names.size() != 0)
		{
			// reset vid_stream
			org_img_stream.clear();
			org_img_stream.reserve(file_names.size());

			for (auto cnt = 0; cnt < file_names.size(); ++cnt)
			{
				auto &path = file_names[cnt];

				if (!path.isNull())
				{
					std::string fname{ path.toStdString() };

					org_img_stream.emplace_back(img::PCX{ fname }.getImgPixelData_r());
				}
			}

			calculatePSNRs();
			buildChartByPSNRseries();

			now_stream_index = 0;
			
			// show the initial image
			showOrgImg_slot();
			showIndexLabel_slot();


			if (decoded_img_stream.size() != 0)
				updateWindowStatus();


			update_signal();
		}
	}

	void nextButton_slot(void)
	{
		moveToNextImage_signal();
	}

	void playButton_slot(void)
	{
		if (is_playing)
			return;

		if (decoded_img_stream.size() == 0 || org_img_stream.size() == 0)
			return;

		if (now_stream_index == decoded_img_stream.size() - 1)
			now_stream_index = -1;

		changePlayStatus(true);

		playImageForwardAndControlInterval();
	}

	void stopButton_slot(void)
	{
		changePlayStatus(false);
	}

	void backButton_slot(void)
	{
		if (is_playing)
			return;

		if (decoded_img_stream.size() == 0 || org_img_stream.size() == 0)
			return;

		if (now_stream_index == 0)
			now_stream_index = decoded_img_stream.size();

		changePlayStatus(true);

		playImageBackwardAndControlInterval();
	}

public slots:

	void showDecodedImg_slot(void)
	{
		transferDataToQImage(decoded_img_stream[now_stream_index], decoded_img);
	}

	void showOrgImg_slot(void)
	{
		transferDataToQImage(org_img_stream[now_stream_index], org_img);
	}

	void showMotionImg_slot(void)
	{
		transferDataToQImage(motion_img_data, motion_img);
	}

	void showIndexLabel_slot(void)
	{
		std::string str = std::to_string(decoded_img_stream.size());
		std::string n = std::to_string(now_stream_index + 1);
		std::string s = n + " / " + str;

		ui.idx_label->setText(s.c_str());
	}

	void moveToNextImage_slot(void)
	{
		now_stream_index += 1;

		now_stream_index %= decoded_img_stream.size();

		updateWindowStatus();

		update_signal();
	}

	void moveToPreImage_slot(void)
	{
		now_stream_index -= 1;

		if (now_stream_index < 0)
			now_stream_index = decoded_img_stream.size();

		updateWindowStatus();

		update_signal();
	}


private: /* -------- inner data -------- */

	Ui::DecodingWindow ui;

	// Qimage data
	QImage decoded_img;
	QImage org_img;
	QImage motion_img;

	// matrix data
	img::Matrix motion_img_data;

	// position
	QPoint decoded_img_pos;
	QPoint org_img_pos;
	QPoint motion_img_pos;
	QPoint chart_pos;

	// motion vector lines
	QVector<QLine> motion_vector_lines;

	// streams
	std::vector<img::Matrix> decoded_img_stream;
	std::vector<img::Matrix> org_img_stream;
	std::vector<MOTION_VECTOR> motion_stream;
	std::vector<double> psnr_stream;

	// record data
	int now_stream_index{ -1 };
	bool is_playing{ false };

	// charts data
	QScatterSeries *series{ nullptr };
	QChart *chart{ nullptr };
	QChartView *chart_view{ nullptr };

	// other
	QPen pen;
	std::mutex mutx;

private: /* -------- initialization functions -------- */

	void initializePositionData(void)
	{
		decoded_img_pos = QPoint{ 50,90 };
		org_img_pos = QPoint{ 580,90 };

		motion_img_pos = QPoint{ 50,480 };

		chart_pos = QPoint{ 500,480 };

		chart_view->setGeometry(chart_pos.x(), chart_pos.y(), 400, 280);
	}

	void initializeMotionImageData(void)
	{
		motion_img_data = img::Matrix(IMAGE_HEIGHT, IMAGE_WIDTH, 3);	// set to RGB, so we can draw color

		unsigned r, c;

		for (r = BLOCK_SIZE / 2 - 1; r < motion_img_data.getRow(); r += BLOCK_SIZE)
			for (c = BLOCK_SIZE / 2 - 1; c < motion_img_data.getCol(); c += BLOCK_SIZE)
			{
				motion_img_data[r][c] = motion_img_data[r][c + 1] = std::vector<img::BYTE>(3, (img::BYTE)0);
				motion_img_data[r + 1][c] = motion_img_data[r + 1][c + 1] = std::vector<img::BYTE>(3, (img::BYTE)0);
			}
	}

	void initializeQchartData(void)
	{
		series = new QScatterSeries{ this };
		series->setMarkerShape(QScatterSeries::MarkerShapeCircle);
		series->setMarkerSize(10.0);

		chart = new QChart{};

		chart_view = new QChartView{ chart, this };
	}

private: /* -------- helper functions -------- */

	void calculatePSNRs(void)
	{
		psnr_stream.clear();

		if (decoded_img_stream.size() == 0 || org_img_stream.size() == 0 || decoded_img_stream.size() != org_img_stream.size())
			return;

		for (auto idx = 0; idx < decoded_img_stream.size(); ++idx)
			psnr_stream.push_back(img::ImgFunction::getPSNR(decoded_img_stream[idx], org_img_stream[idx]));
	}

	void buildChartByPSNRseries(void)
	{
		if (decoded_img_stream.size() == 0 || org_img_stream.size() == 0 || decoded_img_stream.size() != org_img_stream.size())
			return;

		chart->legend()->hide();
		chart->addSeries(series);

		QValueAxis *axisX = new QValueAxis{ this };
		axisX->setRange(1, psnr_stream.size());
		axisX->setTickCount(psnr_stream.size());
		axisX->setLabelFormat("%d");

		QFont font{ "Consolas" };
		font.setPixelSize(10);
		axisX->setLabelsFont(font);
		
		QValueAxis *axisY = new QValueAxis{ this };
		axisY->setRange(0, 50);
		axisY->setTickCount(6);


		chart->setAxisX(axisX, series);
		chart->setAxisY(axisY, series);



		chart->setTitle("PSNR");
	}

	void updateChartSeriesAndLabel(void)
	{
		if (psnr_stream.size() == 0)
		{
			ui.psnr_v_label->setText("");
			return;
		}

		ui.psnr_v_label->setText((std::string{ "PSNR : " } + std::to_string(psnr_stream[now_stream_index])).c_str());

		series->clear();

		for (auto x = 0; x <= now_stream_index; ++x)
			series->append(x + 1, psnr_stream[x]);
	}


	void calculateMotionVectorLines(void)
	{
		if (motion_stream.size() == 0)
			return;

		motion_vector_lines.clear();

		auto &motions = motion_stream[now_stream_index];

		if (motions.size() == 0)
			return;

		for(auto r=0;r<IMAGE_HEIGHT/BLOCK_SIZE;++r)
			for (auto c = 0; c < IMAGE_WIDTH / BLOCK_SIZE; ++c)
			{
				auto &motion = motions[r][c];

				int start_y = motion_img_pos.y() + BLOCK_SIZE / 2;
				int start_x = motion_img_pos.x() + BLOCK_SIZE / 2;

				motion_vector_lines.push_back(QLine(start_x + c * BLOCK_SIZE, start_y + r * BLOCK_SIZE, start_x + c * BLOCK_SIZE + motion.second, start_y + r * BLOCK_SIZE + motion.first));
			}
	}

	void updateWindowStatus(void)
	{
		showDecodedImg_slot();

		showOrgImg_slot();

		calculateMotionVectorLines();

		showIndexLabel_slot();

		updateChartSeriesAndLabel();
	}

	void changePlayStatus(bool flag)
	{
		mutx.lock();
		is_playing = flag;
		mutx.unlock();
	}


	void playImageForwardAndControlInterval(void)
	{
		std::thread td{ [&]() {

			// check is available to playing
			while (is_playing)
			{
				// if is the end , stop play and retrun
				if (now_stream_index == decoded_img_stream.size() - 1)
				{
					changePlayStatus(false);
					return;
				}

				moveToNextImage_signal();

				std::this_thread::sleep_for(std::chrono::milliseconds{ INTERVAL });
			}

		} };

		td.detach();
	}

	void playImageBackwardAndControlInterval(void)
	{
		std::thread td{ [&]() {

			// check is available to playing
			while (is_playing)
			{
				// if is the end , stop play and retrun
				if (now_stream_index == 0)
				{
					changePlayStatus(false);
					return;
				}

				moveToPreImage_signal();

				std::this_thread::sleep_for(std::chrono::milliseconds{ INTERVAL });
			}

		} };

		td.detach();
	}

private: /* -------- read encoded data functions -------- */

	void read(const std::string &fpath)
	{
		std::ifstream fin{ fpath,std::ios::binary };

		if (!fin)
			return;

		fin.seekg(0, std::ios::end);
		auto len = fin.tellg();
		fin.seekg(0, std::ios::beg);

		std::vector<unsigned char> buf(len);

		fin.read((char*)buf.data(), len);

		int idx = 0;

		int frame_number = buf[idx++];
		int gop = buf[idx++];

		img::Matrix pdata;
		img::Matrix ndata;

		for (auto cnt = 0; cnt < frame_number; ++cnt)
		{
			if (cnt%gop == 0)
			{
				pdata = ndata = _readJpeg(buf, idx);

				motion_stream.push_back(MOTION_VECTOR{});
			}
			else
			{
				auto motion = _readMotion(buf, idx);

				motion_stream.push_back(motion);

				_generateImageWithMotionVector(motion, pdata, ndata);
			}

			decoded_img_stream.push_back(ndata);

			pdata = ndata;
		}
	}

	img::Matrix _readJpeg(std::vector<unsigned char> &buf, int &idx)
	{
		auto seqs = my_video::VidFunction::readJpegCode(buf, idx);


		return my_video::VidFunction::jpegDecoding(seqs, 32, 32);
	}

	MOTION_VECTOR _readMotion(std::vector<unsigned char> &buf, int &idx)
	{
		MOTION_VECTOR motion(256 / 8, std::vector<M_VECTOR>(256 / 8));

		unsigned r, c;
		int tem;

		for (r = 0; r < 256 / 8; ++r)
			for (c = 0; c < 256 / 8; ++c)
			{
				motion[r][c].first = _read16bits(buf, idx);
				motion[r][c].second = _read16bits(buf, idx);
			}

		return motion;
	}

	int _read16bits(std::vector<unsigned char> &buf, int &idx)
	{
		int16_t tem = 0;

		tem += buf[idx++];
		tem += (buf[idx++] << 8);

		return tem;
	}

	void _generateImageWithMotionVector(const MOTION_VECTOR &motion, const img::Matrix &ref_data, img::Matrix &ndata) const
	{
		unsigned r, c;
		unsigned rr, cc;
		unsigned i, k;

		for (r = 0; r < 256; r += 8)
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


};

#endif