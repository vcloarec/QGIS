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
#include <QSqlRecord>
#include <QSqlDriverPlugin>
#include <QThread>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QMutexLocker>
#include <QPointer>

#include "qgsdatasourceuri.h"

class QgsSqlODBCDatabaseTransactionConnection;
class QgsSqlDatabaseTransaction;
class QgsSqlODBCDatabaseConnection;

/**
 *  Class that represent a proxy for ODBC database driver
 *  This class handle connection throught an ODBC driver and
 *  support transaction from multiple QsqlDatabase instances to the same database
 *  When a transation begin from a QsqlDatabase instance,
 *  all the next QsqlDatabase instances will share a unique connection embeded in its proper thread.
 */
class QgsSqlOdbcProxyDriver : public QSqlDriver
{
  public:

    QgsSqlOdbcProxyDriver( QObject *parent = nullptr );
    ~QgsSqlOdbcProxyDriver();

    bool hasFeature( DriverFeature f ) const override;
    void close() override;
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

//! Simple wrapper of a database with a ODBC driver
class QgsSqlODBCDatabaseConnection: public QObject
{
// ************** PARTIALLY IMPLEMENTED ***************
    Q_OBJECT
  public:
    QgsSqlODBCDatabaseConnection( const QgsDataSourceUri &uri, const QString &connectionOptions );

    QSqlError lastError() const;
    QSqlResult *createResult();
    QSqlDriver *driver() const;

  public slots:
    bool open();
    bool isOpen() const;
    bool hasFeature( QSqlDriver::DriverFeature f ) const;
    void close();

  protected:
    QSqlDatabase mODBCDatabase;

  private:
    QgsDataSourceUri mUri;
    QString mConnectionOptions;
    static QString threadedConnectionName( const QString &name );
};

class QgsSqlOdbcTransactionResult: public QSqlResult
{
  public:
    /**
     * Contructor of result class that will work on the caller thread with the \a connection created in another thread
     *
     * Instance of this class will communicate with the \a connection to create and handle queries in this database
     * As this derived QsqlResult instance is create in the caller thread,
     * it needs to have acces to the driver \a callerDriver created in the caller thread
     *
     * All interaction between this instance and the connection is done by invoking slots method of the connection
     *
     */
    QgsSqlOdbcTransactionResult( QSqlDriver *callerDriver, QgsSqlODBCDatabaseTransactionConnection *connection, QThread *thread );
    ~QgsSqlOdbcTransactionResult();

    QVariant data( int i ) override;
    void setForwardOnly( bool forward );
    bool isNull( int i ) override;
    bool reset( const QString &sqlquery ) override;
    bool fetch( int i ) override;
    bool fetchFirst() override;
    bool fetchLast() override;
    int size() override;
    int numRowsAffected() override;
    QSqlRecord record() const override;

  private:
    QString mUuid;
    QPointer<QgsSqlODBCDatabaseTransactionConnection> mConnection;
    QPointer<QThread> mThread;

    bool connectionIsValid() const;
    bool isSelectPrivate() const;
    bool isActivePrivate() const;
    int atPrivate() const;
    QSqlError lastErrorPrivate() const;

};


//! Thread safe wrapper of a database with a ODBC driver
class QgsSqlODBCDatabaseTransactionConnection : public QgsSqlODBCDatabaseConnection
{
    Q_OBJECT
  public:

    QgsSqlODBCDatabaseTransactionConnection( const QgsDataSourceUri &uri, const QString &connectionOptions ):
      QgsSqlODBCDatabaseConnection( uri, connectionOptions )
    {}

  public slots:
    //! Creates a QsqlResult stored in the instance and associated with an UUId. Returns the UUId
    QString createTransactionResult();
    void removeTransactionResult( const QString &uuid );

    QVariant data( const QString &uuid, int i ) const;
    bool reset( const  QString &uuid, const QString &stringQuery );
    void setForwardOnly( const  QString &uuid, bool forward );
    bool fetch( const QString &uuid, int index );
    bool fetchFirst( const QString &uuid );
    bool fetchLast( const QString &uuid );
    bool isSelect( const QString &uuid ) const;
    bool isActive( const QString &uuid ) const;
    bool isNull( const QString &uuid, int i ) const;
    int at( const QString &uuid ) const;
    int size( const QString &uuid ) const;
    int numRowsAffected( const QString &uuid ) const;
    QSqlRecord record( const QString &uuid ) const;
    QSqlError lastError( const QString &uuid ) const;

    QSqlError lastError() const;

    bool beginTransaction();
    bool commit();
    bool rollBack();

  private:
    QMap<QString, QSharedPointer<QSqlQuery>> mQueries;
};


//! Class used to wrap database connection with transaction.
class QgsSqlDatabaseTransaction : public QObject
{
    Q_OBJECT
  public:
// ************** PARTIALLY IMPLEMENTED ***************

    ~QgsSqlDatabaseTransaction();

    bool hasFeature( QSqlDriver::DriverFeature f ) const;
    bool isOpen();

    void close();
    void closeAll();

    bool commit();
    bool rollBack();

    QSqlResult *createResult();

    QSqlError lastError() const;

    static bool isTransactionExist( const QgsDataSourceUri &uri );
    static QgsSqlDatabaseTransaction *getTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions );

  private:
    QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, const QgsDataSourceUri &uri, const QString &connectionOptions );
    QgsSqlDatabaseTransaction( QgsSqlOdbcProxyDriver *driver, QgsSqlDatabaseTransaction *other );

    void beginTransaction();
    bool connectionIsValid() const;
    void stopConnection();

    QgsSqlOdbcProxyDriver *mDriver = nullptr;
    QString mConnectionId;

    struct Data
    {
      QPointer<QThread> thread = nullptr;
      QAtomicInt ref = 0;
      QPointer<QgsSqlODBCDatabaseTransactionConnection> connection = nullptr;
      bool transactionIsStarted = false;
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
