// QSettings - save the last host:port
// QSocketNotifier* read, *write;
// QCustomEvent
// QApplication::postEvent()
/*

x qchat client of 'echo' service

- chat (cmdline) client of beep 'echo' service

- qchat using beep

- lchatd (lua chat server) with beep

Add the main gui elements - title bar, open, close, save settings, etc.

*/

#include <qapplication.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qsocket.h>
#include <qtextedit.h>
#include <qtextstream.h>
#include <qvbox.h>

#include <netdb.h>
#include <stdio.h>

class Chat : public QVBox
{
  Q_OBJECT

  private:
    QTextEdit* show_;
    QLineEdit* edit_;
    QPushButton* quit_;
    QSocket* socket_;

    void appendInfo(const QString& text)
    {
      show_->setItalic(TRUE);
      show_->append(QString("... %1").arg(text));
      show_->setItalic(FALSE);
    }
    void appendChat(const char* direction, const QString& text)
    {
      show_->append( QString("%1 %2").arg(direction).arg(text) );
    }

  public:
    Chat(QWidget *parent=0, const char *name=0) : QVBox(parent, name)
    {
      // UI setup:
      show_ = new QTextEdit(this);
      edit_ = new QLineEdit(this);
      quit_ = new QPushButton("Quit", this);

      this->resize(200,400);
      // Does this do anything? quit_->resize(100, 50);

      show_->setReadOnly(TRUE);
      show_->setTextFormat(LogText);

      connect(quit_, SIGNAL(clicked()), qApp, SLOT(quit()));
      connect(edit_, SIGNAL(returnPressed()), this, SLOT(sendMessage()));

      // Net setup:
      socket_ = new QSocket( this );
      connect( socket_, SIGNAL(connected()),        SLOT(socketConnected()) );
      connect( socket_, SIGNAL(readyRead()),        SLOT(socketReadyRead()) );
      connect( socket_, SIGNAL(connectionClosed()), SLOT(socketConnectionClosed()) );
      connect( socket_, SIGNAL(error(int)),         SLOT(socketError(int)) );

      int port = 7; // Standard 'echo' responder

      appendInfo( QString("connect localhost:%1").arg(port) );

      socket_->connectToHost( "localhost", port);
    }

    public slots:
      void sendMessage()
      {
        appendChat("<", edit_->text());

        QTextStream os(socket_);
        os << edit_->text() << "\n";

        edit_->setText("");
      }

      void socketConnected()
      {
        appendInfo("Connected");
      }

      void socketReadyRead()
      {
        // read from the server
        while ( socket_->canReadLine() ) {
          appendChat(">", socket_->readLine());
        }
      }

      void socketConnectionClosed()
      {
        appendInfo("Server closed");
      }

      void socketError( int e )
      {
        appendInfo( QString("Error #%1").arg(e) );
      }

/* Need this? How is resource cleanup handled?
      void closeConnection()
      {
        socket_->close();
        if ( socket_->state() == QSocket::Closing ) {
          // We have a delayed close.
          connect( socket_, SIGNAL(delayedCloseFinished()),
              SLOT(socketClosed()) );
        } else {
          // The socket is closed.
          socketClosed();
        }
      }
      void socketClosed()
      {
        infoText->append( tr("Connection closed\n") );
      }

*/
};

int main( int argc, char **argv )
{
  QApplication a( argc, argv );

  Chat box;

  a.setMainWidget( &box );
  box.show();
  return a.exec();
}

#include "main.moc"

