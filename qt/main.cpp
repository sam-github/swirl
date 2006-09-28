#include <qapplication.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qvbox.h>

#include <assert.h>

class Chat : public QVBox
{
  Q_OBJECT

  public:
    Chat(QWidget *parent=0, const char *name=0);

/*
  public slots:
    void sendMessage()
    {
    }
*/
};

Chat::Chat(QWidget *parent, const char *name) : QVBox(parent, name)
{
  QLineEdit* edit = new QLineEdit(this);
  QPushButton* quit = new QPushButton("Quit", this);

  this->resize(200,400);
  quit->resize(100, 50);

  QObject::connect(quit, SIGNAL(clicked()), qApp, SLOT(quit()));
  QObject::connect(edit, SIGNAL(returnPressed()), qApp, SLOT(quit()));
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

