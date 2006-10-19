// QSettings - save the last host:port
// QSocketNotifier* read, *write;
// QCustomEvent
// QApplication::postEvent()

#define QT_FATAL_ASSERT

#include <qapplication.h>
#include <qevent.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qsocket.h>
#include <qtextedit.h>
#include <qtextstream.h>
#include <qvbox.h>

#include <vortex.h>

class Chat : public QVBox
{
  Q_OBJECT

  private:
    QTextEdit* show_;
    QLineEdit* edit_;
    QPushButton* quit_;

    void appendInfo(const QString& text)
    {
      show_->setItalic(TRUE);
      show_->append(QString("... %1").arg(text));
      show_->setItalic(FALSE);
    }
    void appendChat(const QString& direction, const QString& text)
    {
      show_->append( QString("%1 %2").arg(direction).arg(text) );
    }

    // Tcp support
    QSocket* socket_;

    // Vortex support
    VortexConnection * connection_;
    VortexChannel    * channel_;
    VortexFrame      * frame_;
    gint               msg_no;

  public:
    Chat(QWidget *parent=0, const char *name=0) : QVBox(parent, name)
    {
      // UI setup:
      show_ = new QTextEdit(this);
      edit_ = new QLineEdit(this);
      quit_ = new QPushButton("Quit", this);

      this->resize(600,400);
      // Does this do anything? quit_->resize(100, 50);

      show_->setReadOnly(TRUE);
      show_->setTextFormat(LogText);

      connect(quit_, SIGNAL(clicked()), qApp, SLOT(quit()));
      connect(edit_, SIGNAL(returnPressed()), this, SLOT(sendMessage()));

      // Tcp setup:
      socket_ = new QSocket( this );
      connect( socket_, SIGNAL(connected()),        SLOT(socketConnected()) );
      connect( socket_, SIGNAL(readyRead()),        SLOT(socketReadyRead()) );
      connect( socket_, SIGNAL(connectionClosed()), SLOT(socketConnectionClosed()) );
      connect( socket_, SIGNAL(error(int)),         SLOT(socketError(int)) );

      int port = 7; // Standard 'echo' responder

      appendInfo( QString("connect localhost:%1").arg(port) );

      socket_->connectToHost( "localhost", port);


      // Vortex setup:

      connection_ = vortex_connection_new (
          "localhost", "44000",
          onConnectionNew, this
          );

      Q_ASSERT(!connection_);
    }

    ~Chat()
    {
	vortex_connection_close (connection_);
    }

  private:

    enum Event
    {
      CONNECTION_NEW,    // QCustomEvent, data = connection
      CHANNEL_CREATED,   // QCustomEvent, data = channel_num
      CHANNEL_CLOSED,    // QCustomEvent, data = channel_num
      FRAME_RECEIVED,    // QCustomEvent, data = frame
      UNUSED
    };

    static QEvent::Type E(Event event) { return (QEvent::Type) (QEvent::User+event); }
    static Event E(QEvent::Type event) { return (Event) (event - QEvent::User); }

    void customEvent(QCustomEvent* e)
    {
      qDebug("customEvent %u data %p", (int) (e->type() - QEvent::User), e->data());

      switch(E(e->type()))
      {
        case CONNECTION_NEW:
          connection_ = (VortexConnection*) e->data();

          if (!vortex_connection_is_ok (connection_, false)) {
            appendInfo ( QString("Vortex - connection failed, %1")
                .arg(vortex_connection_get_message(connection_))
                );
            return;
          }
          appendInfo("Vortex - connected");

          // FIXME - need a common header
#define PLAIN_PROFILE "http://fact.aspl.es/profiles/plain_profile"

          channel_ = vortex_channel_new (connection_, 0,
              PLAIN_PROFILE,
              onChannelClosed, this,
              onFrameReceive, this,
              onChannelCreated, this
              );

          Q_ASSERT(!channel_);

          break;

        case CHANNEL_CREATED:
          channel_ = vortex_connection_get_channel(connection_, (gint) e->data());
          appendInfo("Vortex - channel created");
          break;

        case CHANNEL_CLOSED:
          appendInfo("Vortex - channel closed");
          break;

        case FRAME_RECEIVED:
          {
            VortexFrame* frame = (VortexFrame*) e->data();

            appendChat(
                QString("%1/%2>")
                  .arg(vortex_frame_get_type(frame))
                  .arg(vortex_frame_get_content_type(frame)),
                vortex_frame_get_payload(frame));
          }
          break;

        case UNUSED: break;
      }
    }

    static void onConnectionNew(VortexConnection* connection, gpointer vdata)
    {
      Chat* self = (Chat*) vdata;

      qDebug("onConnectionNew %p", connection);

      QApplication::postEvent(self, new QCustomEvent(E(CONNECTION_NEW), connection));
    }

    static void onChannelCreated(gint channel_num, VortexChannel* channel, gpointer vdata)
    {
      Chat* self = (Chat*) vdata;

      (void) channel;

      qDebug("onChannelCreated %d", channel_num);

      QApplication::postEvent(self, new QCustomEvent(E(CHANNEL_CREATED), (void*)channel_num));
    }

    static gboolean onChannelClosed(gint channel_num, VortexConnection* connection, gpointer vdata)
    {
      Chat* self = (Chat*) vdata;

      (void) connection;

      QApplication::postEvent(self, new QCustomEvent(E(CHANNEL_CLOSED), (void*)channel_num));

      return true;
    }

    static void onFrameReceive(
        VortexChannel* channel, VortexConnection* connection, VortexFrame* frame, gpointer vdata
        )
    {
      Chat* self = (Chat*) vdata;

      (void) connection;
      (void) channel;

      frame = vortex_frame_copy(frame);

      Q_ASSERT(frame);

      QApplication::postEvent(self, new QCustomEvent(E(FRAME_RECEIVED), frame));
    }

    public slots:
      void sendMessage()
      {
        appendChat("<", edit_->text());

        // TCP send
        QTextStream os(socket_);
        os << edit_->text() << "\n";

        // Beep send
        if(channel_) {
          gint ok = vortex_channel_send_msgv(channel_, NULL, "%s", edit_->text().latin1());
          Q_ASSERT(ok);
        }

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
  vortex_init ();

  QApplication a( argc, argv );

  qDebug("-- qchat start");

  Chat box;

  a.setMainWidget( &box );
  box.show();
  return a.exec();
}

#include "main.moc"

