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
#include <QDebug>

#include "qsqldatabase.h"
#include "qgsdatasourceuri.h"

class QgsMssqlDatabase;
class QgsMssqlDatabaseConnection;

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
    struct Data
    {
      QAtomicInt ref;
      QgsMssqlDatabaseConnection *mDatabaseConnection = nullptr;
      std::weak_ptr<QSqlQuery> mSqlQueryWeakRef;
    };

    struct ValidityChecker
    {
      ValidityChecker( Data *d );
      ~ValidityChecker();
      bool isValid() const;

      Data *md;
    };

    std::shared_ptr<QSqlQuery> sqlQuery() const
    {
      return d->mSqlQueryWeakRef.lock();
    }

    Data *d = nullptr;

    bool isValidPrivate() const;

    friend class QgsMssqlDatabase;
    friend class QgsMssqlDatabaseConnectionClassic;
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
    static void invalidateConnection( QgsMssqlDatabaseConnection *mDatabaseConnection );
    static void purgeConnections();

    static QMap<QString, std::shared_ptr<QgsMssqlDatabaseConnection>> sConnectionsThread;

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
    virtual std::shared_ptr<QSqlQuery> createQuery() = 0;

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
    void invalidated() const;

  protected:
    QgsDataSourceUri mUri;
    QString mConnectionName;
    QAtomicInt mRef;
    bool mIsInvalidated = false;
    bool mIsInvalidationAsked = false;

  private:
    virtual void addBindValue( QgsMssqlQuery::ValidityChecker &checker, const QVariant &val, QSql::ParamType paramType = QSql::In ) = 0;
    virtual void clear( std::weak_ptr<QSqlQuery> queryRef ) = 0;
    virtual bool exec( std::weak_ptr<QSqlQuery> queryRef, const QString &queryString ) = 0;

    virtual bool exec( std::weak_ptr<QSqlQuery> queryRef ) = 0;
    virtual void finish( std::weak_ptr<QSqlQuery> queryRef ) = 0;
    virtual bool first( std::weak_ptr<QSqlQuery> queryRef ) = 0;
    virtual bool isActive( std::weak_ptr<QSqlQuery> queryRef ) const = 0;
    virtual bool isValid( std::weak_ptr<QSqlQuery> queryRef ) const = 0;
    virtual bool isForwardOnly( std::weak_ptr<QSqlQuery> queryRef ) const = 0;
    virtual QSqlError lastError( std::weak_ptr<QSqlQuery> queryRef ) const = 0;
    virtual QString lastQuery( std::weak_ptr<QSqlQuery> queryRef ) const = 0;
    virtual bool next( std::weak_ptr<QSqlQuery> queryRef ) = 0;
    virtual int numRowsAffected( std::weak_ptr<QSqlQuery> queryRef ) const = 0;
    virtual bool prepare( std::weak_ptr<QSqlQuery> queryRef, const QString &queryString ) = 0;
    virtual QSqlRecord record( std::weak_ptr<QSqlQuery> queryRef ) const = 0;
    virtual void setForwardOnly( std::weak_ptr<QSqlQuery> queryRef, bool forward ) = 0;
    virtual int size( std::weak_ptr<QSqlQuery> queryRef ) const = 0;

    virtual QVariant value( std::weak_ptr<QSqlQuery> queryRef, int index ) const = 0;
    virtual QVariant value( std::weak_ptr<QSqlQuery> queryRef, const QString &name ) const = 0;

    virtual void removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef ) = 0;

    friend class QgsMssqlQuery;

};

class QgsMssqlDatabaseConnectionClassic : public QgsMssqlDatabaseConnection
{
    Q_OBJECT
  public:
    QgsMssqlDatabaseConnectionClassic( QgsDataSourceUri uri, QString connectionName );
    ~QgsMssqlDatabaseConnectionClassic();
    void init() override;
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool isValid() const override;
    QSqlError lastError() const override;
    QStringList tables( QSql::TableType type = QSql::Tables ) const override;
    bool isTransaction() const override;
    void invalidate() override;

    std::shared_ptr<QSqlQuery> createQuery() override;

    bool beginTransaction() override
    {
      return mDatabase.transaction();
    }

  private slots:
    void confirmInvalidation();

  private:
    QSqlDatabase mDatabase;
    QList<std::shared_ptr<QSqlQuery>> mSqlQueries;

    void addBindValue( QgsMssqlQuery::ValidityChecker &checker, const QVariant &val, QSql::ParamType paramType = QSql::In ) override;
    void clear( std::weak_ptr<QSqlQuery> queryRef ) override;
    bool exec( std::weak_ptr<QSqlQuery> queryRef, const QString &queryString ) override;

    bool exec( std::weak_ptr<QSqlQuery> queryRef ) override;
    void finish( std::weak_ptr<QSqlQuery> queryRef ) override;
    bool first( std::weak_ptr<QSqlQuery> queryRef ) override;
    bool isActive( std::weak_ptr<QSqlQuery> queryRef ) const override;
    bool isValid( std::weak_ptr<QSqlQuery> queryRef ) const override;
    bool isForwardOnly( std::weak_ptr<QSqlQuery> queryRef ) const override;
    QSqlError lastError( std::weak_ptr<QSqlQuery> queryRef ) const override;
    QString lastQuery( std::weak_ptr<QSqlQuery> queryRef ) const override;
    bool next( std::weak_ptr<QSqlQuery> queryRef ) override;
    int numRowsAffected( std::weak_ptr<QSqlQuery> queryRef ) const override;
    bool prepare( std::weak_ptr<QSqlQuery> queryRef, const QString &queryString ) override;
    QSqlRecord record( std::weak_ptr<QSqlQuery> queryRef ) const override;
    void setForwardOnly( std::weak_ptr<QSqlQuery> queryRef, bool forward ) override;
    int size( std::weak_ptr<QSqlQuery> queryRef ) const override;

    QVariant value( std::weak_ptr<QSqlQuery> queryRef, int index ) const override;
    QVariant value( std::weak_ptr<QSqlQuery> queryRef, const QString &name ) const override;

    void removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef ) override;

};

class QgsMssqlDatabaseConnectionTransaction : public QgsMssqlDatabaseConnection
{
  public:
    bool isTransaction() const {return true;}
};


#endif // QGSMSSQLDATABASE_H
