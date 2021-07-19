/***************************************************************************
  qgsmssqldatabase.h - QgsMssqlDatabase

 ---------------------
 begin                : 15.7.2021
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
#ifndef QGSMSSQLDATABASE_H
#define QGSMSSQLDATABASE_H

#include <memory>

#include <QMutex>
#include <QVariant>
#include <QSqlQuery>
#include <QPointer>

#include <QSqlError>
#include <QSqlRecord>
#include <QThread>

#include <QDebug>


#include "qsqldatabase.h"
#include "qgsdatasourceuri.h"

class QgsMssqlDatabase;
class QgsMssqlDatabaseConnection;

class MssqlQueryRef
{
  public:
    struct Data
    {
      QgsMssqlDatabaseConnection *mDatabaseConnection = nullptr;
      std::weak_ptr<QSqlQuery> mSqlQueryWeakRef;
      QAtomicInt ref;
    };

    MssqlQueryRef() {};
    MssqlQueryRef( Data *d );
    ~MssqlQueryRef();
    bool isValid() const;

    Data *md = nullptr;
};

Q_DECLARE_METATYPE( MssqlQueryRef )

/**
 * @brief The QgsMssqlQuery class
 */
class QgsMssqlQuery
{
  public :
    QgsMssqlQuery() = default;
    QgsMssqlQuery( QgsMssqlDatabase &database );
    QgsMssqlQuery( const QgsMssqlQuery &other );
    ~QgsMssqlQuery();

    QgsMssqlQuery &operator=( const QgsMssqlQuery &other );

    void addBindValue( const QVariant &val, QSql::ParamType paramType = QSql::In );
    void clear();
    bool exec( const QString &query );
    bool exec();
    void finish();
    bool first();
    bool isActive() const;
    bool isValid() const;
    bool isForwardOnly() const;
    QSqlError lastError() const;
    QString lastQuery() const;
    bool next();
    int numRowsAffected() const;
    bool prepare( const QString &query );
    QSqlRecord record() const;
    void setForwardOnly( bool forward );
    int size() const;
    QVariant value( int index ) const;
    QVariant value( const QString &name ) const;

  private:

    std::shared_ptr<QSqlQuery> sqlQuery() const
    {
      return d->mSqlQueryWeakRef.lock();
    }

    MssqlQueryRef::Data *d = nullptr;

    bool isValidPrivate() const;

    friend class QgsMssqlDatabase;
    friend class QgsMssqlDatabaseConnection;
    friend class QgsMssqlDatabaseConnectionClassic;
    friend class QgsMssqlDatabaseConnectionTransaction;
};

/**
 * @brief The QgsMssqlDatabase class provide an interface for accessing a MSSQL database through a connection.
 * An instance of QgsMssqlDatabase handles a connection that can be shared by severals instance if they are created in the same thread.
 *
 * A connection is obtains by calling the static method database(). If the wanted connection does not exist in the caller thread,
 * a new connection is created that will be handle by the new created instance of QgsMssqlDatabase.
 * If a such connection exists, the new QgsMssqlInstance will handle the existing one.
 *
 * If an instance of QgsMssqlDatabase starts a transaction, all existing connection that have the have the same uri are not valid anymore
 * and a new one is created in the starting transaction QgsMssqlDatabase instance. Then, while the transaction is open, all new instance
 * of QgsMssqlDatabase with the same uri will share the same connection even if they are created on a different thread.
 */
class QgsMssqlDatabase
{
  public:
    QgsMssqlDatabase() = default;
    QgsMssqlDatabase( const QgsMssqlDatabase &other );
    ~QgsMssqlDatabase();

    QgsMssqlDatabase &operator=( const QgsMssqlDatabase &other );

    bool open();
    void close();
    bool isOpen() const;
    bool isValid() const;

    QSqlError lastError() const;

    QStringList tables( QSql::TableType type = QSql::Tables ) const;
    bool transaction();
    QgsMssqlQuery createQuery();

    //! Create a database object
    static QgsMssqlDatabase database( const QgsDataSourceUri &uri );
    static QgsMssqlDatabase database( const QString &service, const QString &host, const QString &db, const QString &username, const QString &password );

  private:
    //! Constructor
    QgsMssqlDatabase( const QgsDataSourceUri &mUri );

    QgsDataSourceUri mUri;
    std::weak_ptr<QgsMssqlDatabaseConnection> mDatabaseConnectionRef;

    static std::shared_ptr<QgsMssqlDatabaseConnection> getConnection( const QgsDataSourceUri &uri, bool transaction );
    static void dereferenceConnection( std::weak_ptr<QgsMssqlDatabaseConnection> mDatabaseConnectionRef );

    static QMap<QString, std::shared_ptr<QgsMssqlDatabaseConnection>> sExistingConnections;

    static QString threadConnectionName( const QgsDataSourceUri &uri, bool transaction );
    static QString threadString();
    static QString connectionName( const QgsDataSourceUri &uri );

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    static QMutex sMutex;
#else
    static QRecursiveMutex sMutex;
#endif

    friend class QgsMssqlQuery;
};


class QgsMssqlDatabaseConnection : public QObject
{
    Q_OBJECT
  public:
    QgsMssqlDatabaseConnection( QgsDataSourceUri uri, QString connectionName )
      : mUri( uri )
      , mConnectionName( connectionName )
    { }

    virtual ~QgsMssqlDatabaseConnection() = default;
    virtual void init() = 0;
    virtual bool isTransaction() const = 0;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool isValid() const = 0;
    virtual QSqlError lastError() const = 0;
    virtual QStringList tables( QSql::TableType type = QSql::Tables ) const = 0;
    virtual MssqlQueryRef createQuery() = 0;

    virtual void invalidate() = 0;

    virtual bool beginTransaction() = 0;

    bool ref()
    {
      return mRef.ref();
    }
    bool deref()
    {
      return mRef.deref();
    }

    bool isInvalidated() const;

    QString connectionName() const;

  signals:
    void isStopped() const;

  protected:
    QgsDataSourceUri mUri;
    QString mConnectionName;
    QAtomicInt mRef;
    std::atomic<bool> mIsInvalidated = false;
    std::atomic<bool> mIsWorking = false;
    std::atomic<bool> mIsStopped = true;

    void confirmInvalidation()
    {
      mIsStopped = true;
      emit isStopped();
    }

  private:

    virtual void addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType = QSql::In ) = 0;
    virtual void clear( MssqlQueryRef &queryRef ) = 0;
    virtual bool exec( MssqlQueryRef &queryRef, const QString &queryString ) = 0;

    virtual bool exec( MssqlQueryRef &queryRef ) = 0;
    virtual void finish( MssqlQueryRef &queryRef ) = 0;
    virtual bool first( MssqlQueryRef &queryRef ) = 0;
    virtual bool isActive( MssqlQueryRef &queryRef ) const = 0;
    virtual bool isValid( MssqlQueryRef &queryRef ) const = 0;
    virtual bool isForwardOnly( MssqlQueryRef &queryRef ) const = 0;
    virtual QSqlError lastError( MssqlQueryRef &queryRef ) const = 0;
    virtual QString lastQuery( MssqlQueryRef &queryRef ) const = 0;
    virtual bool next( MssqlQueryRef &queryRef ) = 0;
    virtual int numRowsAffected( MssqlQueryRef &queryRef ) const = 0;
    virtual bool prepare( MssqlQueryRef &queryRef, const QString &queryString ) = 0;
    virtual QSqlRecord record( MssqlQueryRef &queryRef ) const = 0;
    virtual void setForwardOnly( MssqlQueryRef &queryRef, bool forward ) = 0;
    virtual int size( MssqlQueryRef &queryRef ) const = 0;

    virtual QVariant value( MssqlQueryRef &queryRef, int index ) const = 0;
    virtual QVariant value( MssqlQueryRef &queryRef, const QString &name ) const = 0;

    virtual void removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef ) = 0;


    friend class QgsMssqlQuery;
    friend class MssqlQueryRef;

};

class QgsMssqlDatabaseConnectionClassic : public QgsMssqlDatabaseConnection
{
    Q_OBJECT
  public:
    QgsMssqlDatabaseConnectionClassic( QgsDataSourceUri uri, QString connectionName );
    ~QgsMssqlDatabaseConnectionClassic();

    bool isTransaction() const override;
    void invalidate() override;



  public slots:
    void init() override;
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool isValid() const override;
    QSqlError lastError() const override;
    QStringList tables( QSql::TableType type = QSql::Tables ) const override;
    bool beginTransaction() override;

  protected slots:

    void addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType = QSql::In ) override;
    void clear( MssqlQueryRef &queryRef ) override;
    bool exec( MssqlQueryRef &queryRef, const QString &queryString ) override;

    bool exec( MssqlQueryRef &queryRef ) override;
    void finish( MssqlQueryRef &queryRef ) override;
    bool first( MssqlQueryRef &queryRef ) override;
    bool isActive( MssqlQueryRef &queryRef ) const override;
    bool isValid( MssqlQueryRef &queryRef ) const override;
    bool isForwardOnly( MssqlQueryRef &queryRef ) const override;
    QSqlError lastError( MssqlQueryRef &queryRef ) const override;
    QString lastQuery( MssqlQueryRef &queryRef ) const override;
    bool next( MssqlQueryRef &queryRef ) override;
    int numRowsAffected( MssqlQueryRef &queryRef ) const override;
    bool prepare( MssqlQueryRef &queryRef, const QString &queryString ) override;
    QSqlRecord record( MssqlQueryRef &queryRef ) const override;
    void setForwardOnly( MssqlQueryRef &queryRef, bool forward ) override;
    int size( MssqlQueryRef &queryRef ) const override;

    QVariant value( MssqlQueryRef &queryRef, int index ) const override;
    QVariant value( MssqlQueryRef &queryRef, const QString &name ) const override;

    MssqlQueryRef createQuery() override;

  private:
    QSqlDatabase mDatabase;
    QList<std::shared_ptr<QSqlQuery>> mSqlQueries;



    void removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef ) override;

};

class QgsMssqlDatabaseConnectionTransaction : public QgsMssqlDatabaseConnection
{
    Q_OBJECT
  public:

    bool isTransaction() const override;
    QgsMssqlDatabaseConnectionTransaction( QgsDataSourceUri uri, QString connectionName ): QgsMssqlDatabaseConnection( uri, connectionName )
    {
      mThread = new QThread;
      mThreadedConnection = new QgsMssqlDatabaseConnectionClassic( uri, connectionName );
      mThreadedConnection->moveToThread( mThread );
      connect( mThread, &QThread::started, this, &QgsMssqlDatabaseConnectionTransaction::init );
      connect( mThread, &QThread::finished, mThreadedConnection, &QgsMssqlDatabaseConnectionClassic::deleteLater );
      mThread->start();
    }

  private:
    QgsMssqlDatabaseConnectionClassic *mThreadedConnection;
    QThread *mThread;


  public:
    void init()
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "init", Qt::BlockingQueuedConnection );
    }
    bool open() override
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "open", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
      return ret;
    }
    void close() override
    {
      QMetaObject::invokeMethod( mThreadedConnection, "close", Qt::BlockingQueuedConnection );
    }
    bool isOpen() const override
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "isOpen", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
      return ret;
    }

    bool isValid() const override
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "isValid", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
      return ret;
    }
    QSqlError lastError() const
    {
      QSqlError ret;
      QMetaObject::invokeMethod( mThreadedConnection, "lastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlError, ret ) );
      return ret;
    }

    QStringList tables( QSql::TableType type ) const
    {
      QStringList ret;
      QMetaObject::invokeMethod( mThreadedConnection, "tables", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QStringList, ret ), Q_ARG( QSql::TableType, type ) );
      return ret;
    }


    MssqlQueryRef createQuery()
    {
      MssqlQueryRef ret;
      QMetaObject::invokeMethod( mThreadedConnection, "createQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( MssqlQueryRef, ret ) );
      return ret;
    }

    void invalidate()
    {
      QMetaObject::invokeMethod( mThreadedConnection, "invalidate", Qt::BlockingQueuedConnection );
    }


    bool beginTransaction()
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "beginTransaction", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ) );
      return ret;
    }

  private:

    virtual void addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType = QSql::In )
    {
      QMetaObject::invokeMethod( mThreadedConnection, "addBindValue", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QVariant, val ), Q_ARG( QSql::ParamType, paramType ) );
    }

    void clear( MssqlQueryRef &queryRef )
    {
      QMetaObject::invokeMethod( mThreadedConnection, "clear", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ) );
    }
    bool exec( MssqlQueryRef &queryRef, const QString &queryString )
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "exec", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QString, queryString ) );
      return ret;
    }
    bool exec( MssqlQueryRef &queryRef )
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "exec", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    void finish( MssqlQueryRef &queryRef )
    {
      QMetaObject::invokeMethod( mThreadedConnection, "finish", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ) );
    }
    bool first( MssqlQueryRef &queryRef )
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "first", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    bool isActive( MssqlQueryRef &queryRef ) const
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "isActive", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    bool isValid( MssqlQueryRef &queryRef ) const
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "isValid", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    bool isForwardOnly( MssqlQueryRef &queryRef ) const
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "isForwardOnly", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    QSqlError lastError( MssqlQueryRef &queryRef ) const
    {
      QSqlError ret;
      QMetaObject::invokeMethod( mThreadedConnection, "lastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlError, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    QString lastQuery( MssqlQueryRef &queryRef ) const
    {
      QString ret;
      QMetaObject::invokeMethod( mThreadedConnection, "lastQuery", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QString, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    bool next( MssqlQueryRef &queryRef )
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "next", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    int numRowsAffected( MssqlQueryRef &queryRef ) const
    {
      int ret;
      QMetaObject::invokeMethod( mThreadedConnection, "numRowsAffected", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    bool prepare( MssqlQueryRef &queryRef, const QString &queryString )
    {
      bool ret;
      QMetaObject::invokeMethod( mThreadedConnection, "next", Qt::BlockingQueuedConnection, Q_RETURN_ARG( bool, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QString, queryString ) );
      return ret;
    }
    QSqlRecord record( MssqlQueryRef &queryRef ) const
    {
      QSqlRecord ret;
      QMetaObject::invokeMethod( mThreadedConnection, "record", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QSqlRecord, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    void setForwardOnly( MssqlQueryRef &queryRef, bool forward )
    {
      QMetaObject::invokeMethod( mThreadedConnection, "setForwardOnly", Qt::BlockingQueuedConnection, Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( bool, forward ) );
    }
    int size( MssqlQueryRef &queryRef ) const
    {
      int ret;
      QMetaObject::invokeMethod( mThreadedConnection, "size", Qt::BlockingQueuedConnection, Q_RETURN_ARG( int, ret ), Q_ARG( MssqlQueryRef &, queryRef ) );
      return ret;
    }
    QVariant value( MssqlQueryRef &queryRef, int index ) const
    {
      QVariant ret;
      QMetaObject::invokeMethod( mThreadedConnection, "value", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( int, index ) );
      return ret;
    }
    QVariant value( MssqlQueryRef &queryRef, const QString &name ) const
    {
      QVariant ret;
      QMetaObject::invokeMethod( mThreadedConnection, "value", Qt::BlockingQueuedConnection, Q_RETURN_ARG( QVariant, ret ), Q_ARG( MssqlQueryRef &, queryRef ), Q_ARG( QString, name ) );
      return ret;
    }


    void removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef )
    {

    }
};


#endif // QGSMSSQLDATABASE_H
