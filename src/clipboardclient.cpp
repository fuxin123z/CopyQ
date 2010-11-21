#include "clipboardclient.h"
#include "clipboardserver.h"
#include "client_server.h"
#include <QFile>
#include <QDebug>
#include <iostream>

ClipboardClient::ClipboardClient(int &argc, char **argv) :
        App(argc, argv)
{
    Q_INIT_RESOURCE(copyq);

    // parse arguments
    DataList args;
    for (int i = 1; i < argc; ++i)
        args.append( QByteArray(argv[i]) );

    // create message for the server
    if ( !args.isEmpty() && args.first() == QString("-") ) {
        QByteArray mime;
        if ( args.length() == 2 ) {
            mime = args.last();
        } else {
            mime = QByteArray("text/plain");
        }

        // read text from stdin
        DataList args2;
        QFile in;
        in.open(stdin, QIODevice::ReadOnly);
        args2 << QByteArray("write") << mime << in.readAll();
        serialize_args(args2, m_msg);
    } else {
        serialize_args(args, m_msg);
    }

    // client socket
    m_client = new QLocalSocket(this);
    connect( m_client, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
    connect( m_client, SIGNAL(readChannelFinished()),
             this, SLOT(readFinnished()) );
    connect( m_client, SIGNAL(error(QLocalSocket::LocalSocketError)),
             this, SLOT(error(QLocalSocket::LocalSocketError)) );
    connect( m_client, SIGNAL(connected()),
             this, SLOT(sendMessage()));

    // connect to server
    m_client->connectToServer( ClipboardServer::serverName() );
}

void ClipboardClient::sendMessage()
{
    QDataStream out(m_client);
    out << (quint32)m_msg.length() << m_msg;
}

void ClipboardClient::readyRead()
{
    QDataStream in(m_client);

    quint32 exit_code;
    while( m_client->bytesAvailable() < (qint64)sizeof(quint32) )
        m_client->waitForReadyRead();
    in >> exit_code;

    quint32 sz;
    while( m_client->bytesAvailable() < (qint64)sizeof(quint32) )
        m_client->waitForReadyRead();
    in >> sz;

    QByteArray msg;
    while( m_client->bytesAvailable() < sz )
        m_client->waitForReadyRead();
    in >> msg;

    QByteArray bytes = qUncompress(msg);
    std::cout.write( bytes.constData(), bytes.length() );

    exit(exit_code);
}

void ClipboardClient::readFinnished()
{
    exit();
}

void ClipboardClient::error(QLocalSocket::LocalSocketError socketError)
{
    switch (socketError) {
    case QLocalSocket::ServerNotFoundError:
        qDebug() << tr("ERROR: Cannot connect to server! Start CopyQ server first.");
        break;
    case QLocalSocket::ConnectionRefusedError:
        qDebug() << tr("ERROR: Connection refused by server!");
        break;
    case QLocalSocket::PeerClosedError:
        qDebug() << tr("ERROR: Connection lost!");
        break;
    default:
        qDebug() << tr("ERROR: %1.").arg(m_client->errorString());
    }
    exit(1);
}
