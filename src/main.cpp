#include "MainWin.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	dev::az::Main w;
	w.show();
	
	return a.exec();
}
