#include "VideoView.h"

#include <QtWidgets/QApplication>
#include <QLabel>
#include <QImage>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	VideoView w;
	w.show();


	return a.exec();
}
