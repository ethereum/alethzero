#include "AlethZero.h"
#include <QtWidgets/QApplication>
#include <libdevcore/Log.h>

int main(int argc, char** argv)
{
	dev::aleth::initChromiumDebugTools(argc, argv);
	dev::superFunctionAddedByLefteris();
	QApplication a(argc, argv);
	dev::aleth::zero::AlethZero w;
	w.show();
	
	return a.exec();
}
