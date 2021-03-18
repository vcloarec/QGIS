/***************************************************************************
  qgsmssqltransaction.h - QgsMssqlTransaction

 ---------------------
 begin                : 11.3.2021
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
#ifndef QGSMSSQLTRANSACTION_H
#define QGSMSSQLTRANSACTION_H

#include <QSqlDatabase>
#include <QEventLoop>
#include <QThread>
#include <QSqlQuery>
#include <QMutex>
#include <QWaitCondition>
#include <QPointer>

#include "qgstransaction.h"
#include "qgsdatasourceuri.h"
#include "qgis_sip.h"

///@cond PRIVATE
#define SIP_NO_FILE

class QgsMssqlTransaction : public QgsTransaction
{
    Q_OBJECT

  public:
    explicit QgsMssqlTransaction( const QString &connString );
    ~ QgsMssqlTransaction();

    /**
     * Executes the SQL query in database.
     *
     * \param sql The SQL query to execute
     * \param error The error or an empty string if none
     * \param isDirty True to add an undo/redo command in the edition buffer, false otherwise
     * \param name Name of the operation ( only used if `isDirty` is true)
     */
    bool executeSql( const QString &sql, QString &error, bool isDirty = false, const QString &name = QString() ) override;
    QString createSavepoint( const QString &savePointId, QString &error ) override;

    QSqlQuery createQuery();

  private:
    bool beginTransaction( QString &error, int ) override;
    bool commitTransaction( QString &error ) override;
    bool rollbackTransaction( QString &error ) override;

    QSqlDatabase mDataBase;
    bool transactionCommand( const QString command, QString &error );

};

class QgsMssqlConnectionWrapper : public QObject
{
    Q_OBJECT
  public:
    QgsMssqlConnectionWrapper( const QgsDataSourceUri &uri );

  public slots:
    //! Creates the connection with the database
    void createConnection();

    //! Begins a transaction, return true if successful, \see QSqlDataBase::transaction()
    bool beginTransaction();

    //! Commits the active transasction, return the value returned by QSqlDataBase::commit()
    bool commitTransaction();

    //! Rollbacks the active transasction, return the value returned by QSqlDataBase::rollback()
    bool rollbackTransaction();

    //! Create a query and return an unique id associated to this query, this unique id can be used for operation on this query
    QString createQuery();

    //! Remove the query associated with the unique \a uid
    void removeQuery( const QString &uid );

    //! Executes the query associated with the unique id \a uid, returns the same value than QsqlQuery::exec()
    bool executeQuery( const QString &uid, const QString &query );

    /**
     *  Returns the value of field index of the current record the query associated with the unique id \a uid
     *  An invalid QVariant is returned if the query does not exist anymore or, if it exists have the same behavior than QSqlQuery::value()
     */
    QVariant queryValue( const QString &uid, int index ) const;

    //! Returns whether the query is active, same behavior than QSqlQuery::isActive() except if the query does not exist anymore, return false
    bool isQueryActive( const QString &uid ) const;

  private:
    QSqlDatabase mDatabaseConnection;
    QgsDataSourceUri mUri;
    QMap<QString, QSharedPointer<QSqlQuery>> mQueries;
    QMap<QString, bool> mQueriesSuccesful;
    bool mIsTransactionActive;
};


class QgsMssqlThreadSafeConnection : public QObject
{
  public:
    QgsMssqlThreadSafeConnection( const QgsDataSourceUri &uri );
    ~QgsMssqlThreadSafeConnection();

    class Query
    {
      public:
        Query( QgsMssqlThreadSafeConnection *connection );
        ~Query();

        bool exec( const QString query );
        bool isActive();

        QVariant value( int index );

      private:
        QString mUid;
        QPointer<QgsMssqlThreadSafeConnection> mConnection = nullptr;

        friend class QgsMssqlThreadSafeConnection;
    };

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    //! Creates a query associated to the connection
    Query createQuery();

  private:
    QgsMssqlConnectionWrapper *mConnection = nullptr;
    QThread *mTransactionThread;

    //! Ask to the connection trhead to create a query, return the unique id of the query
    QString createQueryPrivate();
    void removeQuery( const QString &queryId );
    bool executeQuery( const QString &query, const QString &uid );
    bool queryIsActive( const QString &queryId ) const;
    QVariant queryValue( const QString &queryId, int index ) const;

    friend class Query;
};



///@endcond

#endif // QGSMSSQLTRANSACTION_H
