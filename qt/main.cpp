// QSettings - save the last host:port
// QSocketNotifier* read, *write;
/*
Add the main gui elements - title bar, open, close, save settings, etc.

Add socket support. Two modes - listen, or connect?

Write a raw Vortex chat app.

Look into beep support.

OS X app enviroment

  /dev/tty
  cwd
  fd 0, 1, 2 are /dev/null, /dev/console, /dev/console


*/

#include <qapplication.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtextstream.h>
#include <qvbox.h>
#include <stdio.h>

class Chat : public QVBox
{
  Q_OBJECT

  private:
    QLineEdit* edit;
    QPushButton* quit;


  public:
    Chat(QWidget *parent=0, const char *name=0);

  public slots:
    void sendMessage()
    {
/*
      qDebug("send '%s'", edit->text().ascii());
      QTextOStream os(stdout);
      os << "send - '" << edit->text() << "'\n";
*/
    }
};

Chat::Chat(QWidget *parent, const char *name)
  : QVBox(parent, name)
{
  edit = new QLineEdit(this);
  quit = new QPushButton("Quit", this);

  this->resize(200,400);
  quit->resize(100, 50);

  QObject::connect(quit, SIGNAL(clicked()), qApp, SLOT(quit()));
  QObject::connect(edit, SIGNAL(returnPressed()), this, SLOT(sendMessage()));
}

int main( int argc, char **argv )
{
  QApplication a( argc, argv );

  Chat box;

  a.setMainWidget( &box );
  box.show();
  return a.exec();
}

#include "main.moc"

