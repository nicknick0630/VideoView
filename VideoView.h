#ifndef _VIDEO_VIEW_H
#define _VIDEO_VIEW_H

#include <QtWidgets/QMainWindow>
#include "ui_VideoView.h"

#include "PlayVideoWindow.h"
#include "CompressWindow.h"
#include "DecodingWindow.h"


class VideoView : public QMainWindow
{
	Q_OBJECT

public:
	VideoView(QWidget *parent = Q_NULLPTR) : QMainWindow(parent)
	{
		ui.setupUi(this);
	}

public slots:

	void openPlayVideoWindow_slot(void)
	{
		PlayVideoWindow *w = new PlayVideoWindow{ this };

		w->show();
	}

	void openCompressingWindow_slot(void)
	{
		CompressWindow *w = new CompressWindow{ this };

		w->show();
	}

	void openDecodeWindow_slot(void)
	{
		DecodingWindow *w = new DecodingWindow{ this };

		w->show();
	}

private:
	Ui::VideoViewClass ui;
};
#endif