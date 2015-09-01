#include "AlethZero.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	dev::aleth::AlethZero w;
	w.show();
	
	return a.exec();
}
