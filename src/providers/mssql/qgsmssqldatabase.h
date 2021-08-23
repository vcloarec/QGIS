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

/**
 * @brief The QgssMssqlQueryRef class represents an reference to a QSqlQuery handle by a QgsMssqlDatabaseConnection instance.
 * This reference contains following related data to this query:
 *  - a weak reference to the query
 *  - a pointer to the QgsMssqlDatabaseConnection
 *  - whether the query has been invalidated
 *
 *  This reference is used to acces the query and to handle invalidation of the query and its related connection only when the query is not working
 */
class MssqlQueryRef
{
  public:

    //! Data related to the query
    struct Data
    {
      QgsMssqlDatabaseConnection *mDatabaseConnection = nullptr;
      std::weak_ptr<QgsMssqlDatabaseConnection> mConnectionWeakRef;
      std::weak_ptr<QSqlQuery> mSqlQueryWeakRef;
      QAtomicInt ref;
      bool isInvalidate = true;
    };

    //! Default constructor
    MssqlQueryRef() = default;

    //! Constructor with the data \a d related to the query
    MssqlQueryRef( Data *d );

    //! Constructor with the \a databaseConnection and a shared pointer to the query
    MssqlQueryRef( QgsMssqlDatabaseConnection *databaseConnection, std::shared_ptr<QSqlQuery> query );

    //! Destructor
    ~MssqlQueryRef();

    //! Returns whether the reference is still valid
    bool isValid() const;

    Data *data = nullptr;
};

Q_DECLARE_METATYPE( MssqlQueryRef )

/**
 * @brief The QgsMssqlQuery class represents a query from a QgsMssqlDatabase.
 *
 * This class provides the same method as QsqlQuery
 */
class QgsMssqlQuery
{
  public :

    //! Default constructor
    QgsMssqlQuery() = default;

    //! Constructor with the \a database
    QgsMssqlQuery( QgsMssqlDatabase &database );

    //! Copy constructor
    QgsMssqlQuery( const QgsMssqlQuery &other );

    //! Destructor
    ~QgsMssqlQuery();

    //! Assigns \a other to this object, the data related to the query is implicitly shared
    QgsMssqlQuery &operator=( const QgsMssqlQuery &other );

    //! \see QsqlQuery::addBindValue()
    void addBindValue( const QVariant &val, QSql::ParamType paramType = QSql::In );

    //! \see QsqlQuery::clear()
    void clear();

    //! \see QsqlQuery::exec()
    bool exec( const QString &query );

    //! \see QsqlQuery::exec()
    bool exec();

    //! \see QsqlQuery::finish()
    void finish();

    //! \see QsqlQuery::first()
    bool first();

    //! \see QsqlQuery::isActive()
    bool isActive() const;

    //! \see QsqlQuery::isValid()
    bool isValid() const;

    //! \see QsqlQuery::isForwardOnly()
    bool isForwardOnly() const;

    //! \see QsqlQuery::lastError()
    QSqlError lastError() const;

    //! \see QsqlQuery::lastQuery()
    QString lastQuery() const;

    //! \see QsqlQuery::next()
    bool next();

    //! \see QsqlQuery::numRowsAffected()
    int numRowsAffected() const;

    //! \see QsqlQuery::prepare()
    bool prepare( const QString &query );

    //! \see QsqlQuery::record()
    QSqlRecord record() const;

    //! \see QsqlQuery::setForwardOnly()
    void setForwardOnly( bool forward );

    //! \see QsqlQuery::size()
    int size() const;

    //! \see QsqlQuery::value()
    QVariant value( int index ) const;

    //! \see QsqlQuery::value()
    QVariant value( const QString &name ) const;

  private:

    MssqlQueryRef::Data *d = nullptr;

    friend class QgsMssqlDatabase;
    friend class QgsMssqlDatabaseConnection;
    friend class QgsMssqlDatabaseConnectionTransaction;
};

/**
 * @brief The QgsMssqlDatabase class provides an interface for accessing a MSSQL database through a connection.
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

    //! Default Constructor
    QgsMssqlDatabase() = default;

    //! Copy Constructor
    QgsMssqlDatabase( const QgsMssqlDatabase &other );

    //! Destructor
    ~QgsMssqlDatabase();

    //! Assigns \a other to this object, the data related to connection is implicitly shared
    QgsMssqlDatabase &operator=( const QgsMssqlDatabase &other );

    //! \see QSqlDataBase::open()
    bool open();

    //! \see QSqlDataBase::close()
    void close();

    //! \see QSqlDataBase::isOpen()
    bool isOpen() const;

    //! \see QSqlDataBase::isValid()
    bool isValid() const;

    //! \see QSqlDataBase::lastError()
    QSqlError lastError() const;

    //! \see QSqlDataBase::tables()
    QStringList tables( QSql::TableType type = QSql::Tables ) const;

    /**
     * \see QSqlDataBase::transaction()
     *
     * \note all existing connection with the same uri are invalidated a new unique thread safe connection is created (\see QgsMssqlDatabaseConnectionTransaction)
     */
    bool transaction();

    //! \see QSqlDataBase::commit()
    bool commit();

    //! \see QSqlDataBase::rollback()
    bool rollback();

    //! Creates and return a new query from the handled connection
    QgsMssqlQuery createQuery();

    /**
     * Returns an instance corresponding to the \a uri
     *
     * \note Except if a transaction is opened, If a existing connection exists in the same thread than the caller, this connection is returned.
     *       If such connection does not exist a nex one is created and returned.
     */
    static QgsMssqlDatabase database( const QgsDataSourceUri &uri );

    /**
     * Returns an instance corresponding connection parameter \a service, \a host, database \a db, \a username, \a password
     *
     * \note Except if a transaction is opened, If a existing connection exists in the same thread than the caller, this connection is returned.
     *       If such connection does not exist a nex one is created and returned.
     */
    static QgsMssqlDatabase database( const QString &service, const QString &host, const QString &db, const QString &username, const QString &password );

  private:
    //! Constructor
    QgsMssqlDatabase( const QgsDataSourceUri &mUri );

    QgsDataSourceUri mUri;
    std::weak_ptr<QgsMssqlDatabaseConnection> mDatabaseConnectionRef;

    static std::shared_ptr<QgsMssqlDatabaseConnection> getConnection( const QgsDataSourceUri &uri, bool transaction );

    /**
     *  Dereference and remove the connection if the the connection is not own anymore,
     *  the caller must be sure there is not shared pointer to this connection in its scope before calling this method
     */
    static void dereferenceConnection( std::weak_ptr<QgsMssqlDatabaseConnection> databaseConnectionRef );

    static QMap<QString, std::shared_ptr<QgsMssqlDatabaseConnection>> sExistingConnections;

    static QString threadConnectionName( const QgsDataSourceUri &uri, bool transaction );
    static QString connectionName( const QgsDataSourceUri &uri );

    static QString threadString();

    MssqlQueryRef::Data createQueryData();

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    static QMutex sMutex;
#else
    static QRecursiveMutex sMutex;
#endif

    friend class QgsMssqlQuery;
    friend class MssqlQueryRef;
};

/**
 * @brief The QgsMssqlDatabaseConnection represents a connection to a database. This class wrap a QSqlDatabase instance
 */
class QgsMssqlDatabaseConnection: public QObject
{
    Q_OBJECT
  public:

    /**
     * Constructor of the connection with the \a uri and a connection name.
     * The parameter \a useMultipleActiveResultSets is used to activate or not the Multiple Active Result Sets (MARS) option
     */
    QgsMssqlDatabaseConnection( const QgsDataSourceUri &uri, const QString &connectionName, bool useMultipleActiveResultSets = false );

    //! Destructor
    ~QgsMssqlDatabaseConnection();

    //! Returns whether this connection is a transaction, default implementation return false
    virtual bool isTransaction() const;

    //! Increments the count of existing instance of QgsMssqlDatabase or QgsMssqlQuery that use this object
    bool ref();

    //! Decrements the count of existing instance of QgsMssqlDatabase or QgsMssqlQuery that use this object, returns TRUE is this count is non-zero
    bool deref();

    //! Returns whether this connection has been invalidated
    bool isInvalidated() const;

    //! Returns Whether this connection is finished after its invalidation
    bool isFinished() const;

  public slots:
    //! Initialize the connection
    virtual void init();

    //! \see QSqlDataBase::open()
    virtual bool open();

    //! \see QSqlDataBase::close()
    virtual void close();

    //! \see QSqlDataBase::isOpen()
    virtual bool isOpen() const;

    //! \see QSqlDataBase::isValid()
    virtual bool isValid() const;

    //! \see QSqlDataBase::lastError()
    virtual QSqlError lastError() const;

    //! \see QSqlDataBase::tables()
    virtual QStringList tables( QSql::TableType type = QSql::Tables ) const;

    //! \see QSqlDataBase::beginTransaction()
    virtual bool beginTransaction();

    //! \see QSqlDataBase::commit()
    virtual bool commit();

    //! \see QSqlDataBase::rollback()
    virtual bool rollback();

    //! Invalidates the connection
    virtual void invalidate();

    //! Creates a new query and return a weak pointer to it
    virtual MssqlQueryRef createQuery();

    //! Returns the name of the connection
    QString connectionName() const;

  signals:
    //! Emitted after the connetion is invaldated and the work is finished
    void finished() const;

  protected:
    QgsDataSourceUri mUri;
    QString mConnectionName;
    QAtomicInt mRef;
    std::atomic<bool> mIsInvalidated = false;
    std::atomic<bool> mIsWorking = false;
    std::atomic<bool> mIsFinished = false;

    void onQueryFinish();

  protected slots:

    /**
     * Following methods execute methods of a QsqlQuery instance linked to QgsMssqlQueryRef instance
     */
    virtual void removeSqlQuery( MssqlQueryRef &queryRef );
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
    std::unique_ptr<QSqlDatabase> mDatabase;
    QList<std::shared_ptr<QSqlQuery>> mSqlQueries;
    bool mUseMultipleActiveResultSets = false;


#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    mutable QMutex mMutex;
#else
    mutable QRecursiveMutex mMutex;
#endif
    friend class QgsMssqlQuery;
    friend class MssqlQueryRef;
    friend class QgsMssqlDatabase;
};

/**
 * @brief The QgsMssqlDatabaseConnectionTransaction is derived from QgsMssqlDatabaseConnection and embed a QgsMssqlDatabaseConnection instance in a independant thread.
 *
 *  The query from this class can be created and called from different threads.
 */
class QgsMssqlDatabaseConnectionTransaction : public QgsMssqlDatabaseConnection
{
    Q_OBJECT
  public:

    bool isTransaction() const override;
    QgsMssqlDatabaseConnectionTransaction( const QgsDataSourceUri &uri, const QString &connectionName );
    ~QgsMssqlDatabaseConnectionTransaction();

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
    bool commit() override;
    bool rollback() override;

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

    void removeSqlQuery( MssqlQueryRef &queryRef ) override;
};


#endif // QGSMSSQLDATABASE_H
