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
    MssqlQueryRef( QgsMssqlDatabaseConnection *databaseConnection, std::shared_ptr<QSqlQuery> query );
    ~MssqlQueryRef();
    bool isValid() const;

    Data *data = nullptr;
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


class QgsMssqlDatabaseConnection: public QObject
{
    Q_OBJECT
  public:
    QgsMssqlDatabaseConnection( QgsDataSourceUri uri, QString connectionName );
    ~QgsMssqlDatabaseConnection();

    virtual bool isTransaction() const;
    virtual void invalidate();

    bool ref()
    {
      return mRef.ref();
    }
    bool deref()
    {
      return mRef.deref();
    }

    bool isInvalidated() const;

  public slots:
    virtual void init();
    virtual bool open();
    virtual void close();
    virtual bool isOpen() const;
    virtual bool isValid() const;
    virtual QSqlError lastError() const;
    virtual QStringList tables( QSql::TableType type = QSql::Tables ) const;
    virtual bool beginTransaction();

    virtual MssqlQueryRef createQuery();

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


  protected slots:

    virtual void addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType = QSql::In );
    virtual void clear( MssqlQueryRef &queryRef );
    virtual bool exec( MssqlQueryRef &queryRef, const QString &queryString );

    virtual bool exec( MssqlQueryRef &queryRef );
    virtual void finish( MssqlQueryRef &queryRef );
    virtual bool first( MssqlQueryRef &queryRef );
    virtual bool isActive( MssqlQueryRef &queryRef ) const;
    virtual bool isValid( MssqlQueryRef &queryRef ) const;
    virtual bool isForwardOnly( MssqlQueryRef &queryRef ) const;
    virtual QSqlError lastError( MssqlQueryRef &queryRef ) const;
    virtual QString lastQuery( MssqlQueryRef &queryRef ) const;
    virtual bool next( MssqlQueryRef &queryRef );
    virtual int numRowsAffected( MssqlQueryRef &queryRef ) const;
    virtual bool prepare( MssqlQueryRef &queryRef, const QString &queryString );
    virtual QSqlRecord record( MssqlQueryRef &queryRef ) const;
    virtual void setForwardOnly( MssqlQueryRef &queryRef, bool forward );
    virtual int size( MssqlQueryRef &queryRef ) const;

    virtual QVariant value( MssqlQueryRef &queryRef, int index ) const;
    virtual QVariant value( MssqlQueryRef &queryRef, const QString &name ) const;

  private:
    QSqlDatabase mDatabase;
    QList<std::shared_ptr<QSqlQuery>> mSqlQueries;

    virtual void removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef );


    friend class QgsMssqlQuery;
    friend class MssqlQueryRef;

};

class QgsMssqlDatabaseConnectionTransaction : public QgsMssqlDatabaseConnection
{
    Q_OBJECT
  public:

    bool isTransaction() const override;
    QgsMssqlDatabaseConnectionTransaction( QgsDataSourceUri uri, QString connectionName );

  private:
    QgsMssqlDatabaseConnection *mThreadedConnection;
    QThread *mThread;

  public:
    void init() override;
    bool open() override;
    void close() override;
    bool isOpen() const override;

    bool isValid() const override;
    QSqlError lastError() const override;

    QStringList tables( QSql::TableType type ) const override;


    MssqlQueryRef createQuery() override;

    void invalidate() override;


    bool beginTransaction() override;

  private:

    virtual void addBindValue( MssqlQueryRef &queryRef, const QVariant &val, QSql::ParamType paramType = QSql::In ) override;

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


    void removeSqlQuery( std::weak_ptr<QSqlQuery> queryRef ) override;
};


#endif // QGSMSSQLDATABASE_H
