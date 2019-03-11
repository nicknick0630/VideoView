#ifndef _PLAY_VIDEO_WINDOW_H
#define _PLAY_VIDEO_WINDOW_H

#include <QDialog>
#include <QImage>
#include <QPainter>
#include <QFileDialog>


#include <vector>
#include <fstream>
#include <thread>
#include <mutex>

#include "PCX.h"

#include "VideoFunctions.h"

#include "ui_PlayVideoWindow.h"

using namespace video;

class PlayVideoWindow : public QDialog
{
	Q_OBJECT

public:
	PlayVideoWindow(QWidget *parent = Q_NULLPTR) : QDialog(parent)
	{
		ui.setupUi(this);

		QObject::connect(this, SIGNAL(showImageOfIndex_signal(int)), this, SLOT(showImageOfIndex_slot(int)));
	}

	~PlayVideoWindow() {}

	void paintEvent(QPaintEvent *ev)
	{
		QPainter p{ this };

		if (!qimg.isNull())
			p.drawImage(172, 100, qimg);
	}

public slots:
	
	void openVideoSequence_slot(void)
	{
		auto file_names{ QFileDialog::getOpenFileNames(this, QStringLiteral("¿ï¾Ü¼v¹³") , QStringLiteral("../Resource/Images/PCX") , QStringLiteral("Image (*.pcx);")) };

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

			// show the first image
			showImageOfIndex_slot(0);
		}
	}

	void showImageOfIndex_slot(int idx)
	{
		if (idx >= vid_stream.size())
			throw "vid idx out of range";

		transferDataToQImage(vid_stream[idx], qimg);

		now_img_idx = idx;

		updateIndexLabel();

		update();
	}

	void playButtonAction_slot(void)
	{
		if (is_playing)
			return;

		if (vid_stream.size() == 0)
			return;

		if (now_img_idx == vid_stream.size() - 1)
			now_img_idx = 0;

		startPlaying();

		playImageForwardAndControlInterval();
	}

	void backButtonAction_slot(void)
	{
		if (is_playing)
			return;

		if (vid_stream.size() == 0)
			return;

		if (now_img_idx == vid_stream.size() - 1)
			now_img_idx = 0;

		stopPlaying();

		now_img_idx = vid_stream.size() - 1;

		startPlaying();

		playImageBackwardAndControlInterval();
	}

	void stopButtonAction_slot(void)
	{
		stopPlaying();
	}

	void nextButtonAction_slot(void)
	{
		moveToNextImage();

		showImageOfIndex_slot(now_img_idx);
	}

signals:

	void showImageOfIndex_signal(int idx);

private:
	Ui::PlayVideoWindow ui;

	int now_img_idx{ -1 };
	bool is_playing{ false };
	std::mutex vid_mutex;

	QImage qimg;
	std::vector<img::Matrix> vid_stream;

private:

	void stopPlaying(void)
	{
		vid_mutex.lock();
		is_playing = false;
		vid_mutex.unlock();
	}

	void startPlaying(void)
	{
		vid_mutex.lock();
		is_playing = true;
		vid_mutex.unlock();
	}

	void moveToNextImage(void)
	{
		vid_mutex.lock();
		++now_img_idx;
		now_img_idx %= vid_stream.size();
		vid_mutex.unlock();
	}

	void moveToPreImage(void)
	{
		vid_mutex.lock();
		--now_img_idx;
		if (now_img_idx < 0)
			now_img_idx = vid_stream.size() - 1;
		vid_mutex.unlock();
	}

	void updateIndexLabel(void)
	{
		auto str = std::to_string(now_img_idx + 1) + " / " + std::to_string(vid_stream.size());
		ui.index_label->setText(str.c_str());
	}

	void playImageForwardAndControlInterval(void)
	{
		std::thread td{ [&]() {

			// check is available to playing
			while (is_playing)
			{
				// if is the end , stop play and retrun
				if (now_img_idx == vid_stream.size() - 1)
				{
					stopPlaying();
					return;
				}

				moveToNextImage();

				showImageOfIndex_signal(now_img_idx);

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
				if (now_img_idx == 0)
				{
					stopPlaying();
					return;
				}

				moveToPreImage();

				showImageOfIndex_signal(now_img_idx);

				std::this_thread::sleep_for(std::chrono::milliseconds{ INTERVAL });
			}

		} };

		td.detach();
	}
};

#endif