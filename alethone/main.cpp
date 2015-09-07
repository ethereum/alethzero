#include "AlethOne.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	dev::aleth::one::AlethOne w;
	w.show();
	
	return a.exec();
}
