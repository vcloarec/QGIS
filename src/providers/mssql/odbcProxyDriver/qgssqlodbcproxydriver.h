/***************************************************************************
  qgssqlodbcproxydriver.h - QgsSqlOdbcProxyDriver

 ---------------------
 begin                : 3.5.2021
 copyright            : (C) 2021 by Vincent Cloarec
 email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSSQLODBCPROXYDRIVER_H
#define QGSSQLODBCPROXYDRIVER_H

#include <QSqlDriver>
#include <QSqlResult>
#include <QSqlDriverPlugin>
#include <QThread>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QMutexLocker>
#include <QPointer>

#include "qgsdatasourceuri.h"

class QgsSqlODBCDatabaseThreadedConnection;
class QgsSqlDatabaseTransaction;
class QgsSqlODBCDatabaseConnection;


class QgsSqlOdbcProxyDriver : public QSqlDriver
{
  public:
// ************** PARTIALLY IMPLEMENTED ***************
    QgsSqlOdbcProxyDriver( QObject *parent = nullptr );
    ~QgsSqlOdbcProxyDriver();
    bool hasFeature( DriverFeature f ) const override;
    void close() {}
    QSqlResult *createResult() const override;

    bool open( const QString &db, const QString &user, const QString &password, const QString &host, int port, const QString &connOpts ) override;
    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;

  private:
    QgsDataSourceUri mUri;
    QString mConnectionOption;
    std::unique_ptr<QgsSqlDatabaseTransaction> mTransaction;
    std::unique_ptr<QgsSqlODBCDatabaseConnection> mConnection;
    void initOdbcDatabase();
};

class QgsSqlOdbcThreadSafeResult: public QSqlResult
{
  public:
// ************** NOT IMPLEMENTED ***************

// Here we need to find a way to path the information from the QsqlResult created by the connection in the other trhead to this instance
// The problem is that all method of QsqlResult are protected, only QsqlQuery can have access (friend class)

    /**
     * Contructor of result class that will work on the caller thread with the \a connection created in another thread
     *
     * Instance of this class will communicate with the \a connection to create and handle queries in this database
     * As this derived QsqlResult instance is create in the caller thread,
     * it needs to have acces to the driver \a callerDriver created in the caller therad
     *
     * All interaction between this instance and the connection is done by invoking slots method of the connection
     *
     */
    QgsSqlOdbcThreadSafeResult( QSqlDriver *callerDriver, QgsSqlODBCDatabaseThreadedConnection *connection );

    QVariant data( int i );
    bool isNull( int i ) {return false;}
    bool reset( const QString &sqlquery ) {return false;}
    bool fetch( int i ) {return false;}
    bool fetchFirst() {return false;}
    bool fetchLast() {return false;}
    int size() {return 0;}
    int numRowsAffected() {return 0;}

  private:
    QString mUuid;
    QPointer<QgsSqlODBCDatabaseThreadedConnection> mConnection;
};


//! Simple wrapper of a database with a ODBC driver
class QgsSqlODBCDatabaseConnection: public QObject
{
// ************** PARTIALLY IMPLEMENTED ***************
    Q_OBJECT
  public:
    QgsSqlODBCDatabaseConnection( const QgsDataSourceUri &uri, const QString &connectionOptions ):
      mUri( uri ), mConnectionOptions( connectionOptions )
    {}

    QSqlError lastError() const
    {
      return mODBCDatabase.lastError();
    }

    QSqlResult *createResult()
    {
      if ( mODBCDatabase.driver() )
        return mODBCDatabase.driver()->createResult();

      return nullptr;
    }

    QSqlDriver *driver() const
    {
      return mODBCDatabase.driver();
    }

  public slots:

    bool open();

  private:
    QgsDataSourceUri mUri;
    QString mConnectionOptions;
    QSqlDatabase mODBCDatabase;

    static QString threadedConnectionName( const QString &name )
    {
      return QStringLiteral( "%1:0x%2" ).arg( name ).arg( reinterpret_cast<quintptr>( QThread::currentThread() ), 2 * QT_POINTER_SIZE, 16, QLatin1Char( '0' ) );
    }
};

//! Thread safe wrapper of a database with a ODBC driver
class QgsSqlODBCDatabaseThreadedConnection : public QgsSqlODBCDatabaseConnection
{
    Q_OBJECT
  public:
    QgsSqlODBCDatabaseThreadedConnection( const QgsDataSourceUri &uri, const QString &connectionOptions ):
      QgsSqlODBCDatabaseConnection( uri, connectionOptions )
    {}

  private slots:
    QMap<QString, QSharedPointer<QSqlResult>> mResults;

    //! Creates a QsqlResult stored in the instance and associated with an UUId. Returns the UUId
    QString createResultPrivate();

    //! Returns the data for field \i in the current row for the result associated with \a uuid
    QVariant data( QString uuid, int i );
};


//! Class used to wrap database connection with transaction.
class QgsSqlDatabaseTransaction : public QObject
{
    Q_OBJECT
  public:
// ************** PARTIALLY IMPLEMENTED ***************

    ~QgsSqlDatabaseTransaction();

    bool hasFeature( QSqlDriver::DriverFeature f ) const
    {
      return false;
    }

    bool isOpen() {return mIsOpen;}

    bool beginTransaction();
    bool commit()  {return true;}
    bool rollBack() {return true;}

    QSqlResult *createResult();

    static bool transactionIsOpen( const QgsDataSourceUri &uri );
    static QgsSqlDatabaseTransaction *getTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions );

  private:
    QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions );
    QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, QgsSqlDatabaseTransaction *other );
    bool mIsOpen;
    QgsSqlOdbcProxyDriver *mDriver = nullptr;
    QString mConnectionId;
    struct Data
    {
      QThread *thread;
      QAtomicInt ref;
      QgsSqlODBCDatabaseThreadedConnection *connection;
    };

    Data *d = nullptr;

    static QMap<QString, QgsSqlDatabaseTransaction *> sOpenedTransaction;

};


class QgsSqlOdbcProxyPlugin : public QSqlDriverPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA( IID "org.qt-project.Qt.QSqlDriverFactoryInterface" FILE "qgsodbcproxy.json" )

  public:
    QgsSqlOdbcProxyPlugin( QObject *parent = nullptr );
    QSqlDriver *create( const QString &key ) override;
};

#endif // QGSSQLODBCPROXYDRIVER_H
