#include <qapplication.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qvbox.h>

int main( int argc, char **argv )
{
  QApplication a( argc, argv );

  QVBox box;
  QLineEdit edit( &box );
  QPushButton quit( "Quit", &box );

  box.resize(200,400);
  quit.resize( 100, 50 );

  QObject::connect(&quit, SIGNAL(clicked()), &a, SLOT(quit()));

  a.setMainWidget( &box );
  box.show();
  return a.exec();
}

