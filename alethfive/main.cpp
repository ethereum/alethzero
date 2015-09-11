#include "AlethFive.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	dev::aleth::five::AlethFive w;
	w.show();
	
	return a.exec();
}
