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
#include <QSharedPointer>

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
      QSqlQuery *mSqlQuery;
    };

    Data *d = nullptr;

    bool isValidPrivate() const;

    friend class QgsMssqlDatabase;
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
    QgsMssqlDatabase( QgsMssqlDatabaseConnection *connection );

    QgsMssqlDatabaseConnection *mDatabaseConnection = nullptr;

    static QgsMssqlDatabaseConnection *getConnection( const QgsDataSourceUri &uri );
    static void dereferenceConnection( QgsMssqlDatabaseConnection *mDatabaseConnection );

    static QMap < QString, QgsMssqlDatabaseConnection *> sConnections;

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    static QMutex sMutex;
#else
    static QRecursiveMutex sMutex;
#endif

    friend class QgsMssqlQuery;
};


class QgsMssqlDatabaseConnection
{
  public:
    QgsMssqlDatabaseConnection( QgsDataSourceUri uri, QString connectionName )
      : mUri( uri )
      , mConnectionName( connectionName )
    {}

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

    bool ref()
    {
      return mRef.ref();
    }
    bool deref()
    {
      return mRef.deref();
    }

  protected:
    QgsDataSourceUri mUri;
    QString mConnectionName;
    QAtomicInt mRef;

  private:
    virtual void addBindValue( QSqlQuery *query, const QVariant &val, QSql::ParamType paramType = QSql::In ) = 0;
    virtual void clear( QSqlQuery *query ) = 0;
    virtual bool exec( QSqlQuery *query, const QString &queryString ) = 0;

    virtual bool exec( QSqlQuery *query ) = 0;
    virtual void finish( QSqlQuery *query ) = 0;
    virtual bool first( QSqlQuery *query ) = 0;
    virtual bool isActive( QSqlQuery *query ) const = 0;
    virtual bool isValid( QSqlQuery *query ) const = 0;
    virtual QSqlError lastError( QSqlQuery *query ) const = 0;
    virtual QString lastQuery( QSqlQuery *query ) const = 0;
    virtual bool next( QSqlQuery *query ) = 0;
    virtual int numRowsAffected( QSqlQuery *query ) const = 0;
    virtual bool prepare( QSqlQuery *query, const QString &queryString ) = 0;
    virtual QSqlRecord record( QSqlQuery *query ) const = 0;
    virtual void setForwardOnly( QSqlQuery *query, bool forward ) = 0;
    virtual int size( QSqlQuery *query ) const = 0;

    virtual QVariant value( QSqlQuery *query, int index ) const = 0;
    virtual QVariant value( QSqlQuery *query, const QString &name ) const = 0;

    virtual void removeSqlQuery( QSqlQuery *sqlQuery ) = 0;

    friend class QgsMssqlQuery;

};

class QgsMssqlDatabaseConnectionClassic : public QgsMssqlDatabaseConnection
{
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

    std::shared_ptr<QSqlQuery> createQuery() override;

  private:
    QSqlDatabase mDatabase;
    QList<std::shared_ptr<QSqlQuery>> mSqlQueries;

    void addBindValue( QSqlQuery *query, const QVariant &val, QSql::ParamType paramType = QSql::In ) override;
    void clear( QSqlQuery *query ) override;
    bool exec( QSqlQuery *query, const QString &queryString ) override;

    bool exec( QSqlQuery *query ) override;
    void finish( QSqlQuery *query ) override;
    bool first( QSqlQuery *query ) override;
    bool isActive( QSqlQuery *query ) const override;
    bool isValid( QSqlQuery *query ) const override;
    QSqlError lastError( QSqlQuery *query ) const override;
    QString lastQuery( QSqlQuery *query ) const override;
    bool next( QSqlQuery *query ) override;
    int numRowsAffected( QSqlQuery *query ) const override;
    bool prepare( QSqlQuery *query, const QString &queryString ) override;
    QSqlRecord record( QSqlQuery *query ) const override;
    void setForwardOnly( QSqlQuery *query, bool forward ) override;
    int size( QSqlQuery *query ) const override;

    QVariant value( QSqlQuery *query, int index ) const override;
    QVariant value( QSqlQuery *query, const QString &name ) const override;

    void removeSqlQuery( QSqlQuery *sqlQuery ) override;
};

class QgsMssqlDatabaseConnectionTransaction : public QgsMssqlDatabaseConnection
{
  public:
    bool isTransaction() const {return true;}
};


#endif // QGSMSSQLDATABASE_H
