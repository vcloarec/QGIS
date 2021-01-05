/***************************************************************************
                    qgsmssqlnewconnection.cpp  -  description
                             -------------------
    begin                : 2011-10-08
    copyright            : (C) 2011 by Tamas Szekeres
    email                : szekerest at gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QInputDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QRegExpValidator>

#include "qgsmssqlnewconnection.h"
#include "qgsmssqlprovider.h"
#include "qgssettings.h"
#include "qgsmssqlconnection.h"
#include "qgsgui.h"

QgsMssqlNewConnection::QgsMssqlNewConnection( QWidget *parent, const QString &connName, Qt::WindowFlags fl )
  : QDialog( parent, fl )
  , mOriginalConnName( connName )
{
  setupUi( this );
  QgsGui::enableAutoGeometryRestore( this );

  connect( btnListDatabase, &QPushButton::clicked, this, &QgsMssqlNewConnection::btnListDatabase_clicked );
  connect( btnConnect, &QPushButton::clicked, this, &QgsMssqlNewConnection::btnConnect_clicked );
  connect( cb_trustedConnection, &QCheckBox::clicked, this, &QgsMssqlNewConnection::cb_trustedConnection_clicked );
  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsMssqlNewConnection::showHelp );

  buttonBox->button( QDialogButtonBox::Ok )->setDisabled( true );
  connect( txtName, &QLineEdit::textChanged, this, &QgsMssqlNewConnection::updateOkButtonState );
  connect( txtService, &QLineEdit::textChanged, this, &QgsMssqlNewConnection::updateOkButtonState );
  connect( txtHost, &QLineEdit::textChanged, this, &QgsMssqlNewConnection::updateOkButtonState );
  connect( listDatabase, &QListWidget::currentItemChanged, this, &QgsMssqlNewConnection::updateOkButtonState );
  connect( listDatabase, &QListWidget::currentItemChanged, this, &QgsMssqlNewConnection::onCurrentDataBaseChange );
  connect( cb_geometryColumns,  &QCheckBox::clicked, this, &QgsMssqlNewConnection::onCurrentDataBaseChange );
  connect( cb_allowGeometrylessTables,  &QCheckBox::clicked, this, &QgsMssqlNewConnection::onCurrentDataBaseChange );

  lblWarning->hide();

  if ( !connName.isEmpty() )
  {
    // populate the dialog with the information stored for the connection
    // populate the fields with the stored setting parameters
    QgsSettings settings;

    QString key = "/MSSQL/connections/" + connName;
    txtService->setText( settings.value( key + "/service" ).toString() );
    txtHost->setText( settings.value( key + "/host" ).toString() );
    listDatabase->addItem( settings.value( key + "/database" ).toString() );
    groupBoxSchemasFilter->setChecked( settings.value( key + "/schemasFiltering" ).toBool() );
    QVariant schemasVariant = settings.value( key + "/schemasFiltered" );
    if ( schemasVariant.isValid() && schemasVariant.type() == QVariant::Map )
      mSchemaSettings = schemasVariant.toMap();

    listDatabase->setCurrentRow( 0 );
    cb_geometryColumns->setChecked( QgsMssqlConnection::geometryColumnsOnly( connName ) );
    cb_allowGeometrylessTables->setChecked( QgsMssqlConnection::allowGeometrylessTables( connName ) );
    cb_useEstimatedMetadata->setChecked( QgsMssqlConnection::useEstimatedMetadata( connName ) );
    mCheckNoInvalidGeometryHandling->setChecked( QgsMssqlConnection::isInvalidGeometryHandlingDisabled( connName ) );

    if ( settings.value( key + "/saveUsername" ).toString() == QLatin1String( "true" ) )
    {
      txtUsername->setText( settings.value( key + "/username" ).toString() );
      chkStoreUsername->setChecked( true );
      cb_trustedConnection->setChecked( false );
    }

    if ( settings.value( key + "/savePassword" ).toString() == QLatin1String( "true" ) )
    {
      txtPassword->setText( settings.value( key + "/password" ).toString() );
      chkStorePassword->setChecked( true );
    }

    txtName->setText( connName );
  }
  txtName->setValidator( new QRegExpValidator( QRegExp( "[^\\/]+" ), txtName ) );
  cb_trustedConnection_clicked();

  schemaView->setModel( &mSchemaModel );
  if ( listDatabase->currentItem() )
  {
    QString dataBaseName = listDatabase->currentItem()->text();
    mSchemaModel.setDataBaseName( dataBaseName );
    mSchemaModel.setSchemasSetting( mSchemaSettings.value( dataBaseName ).toMap() );
  }

  onCurrentDataBaseChange();
  groupBoxSchemasFilter->setCollapsed( !groupBoxSchemasFilter->isChecked() );
}

//! Autoconnected SLOTS
void QgsMssqlNewConnection::accept()
{
  QgsSettings settings;
  QString baseKey = QStringLiteral( "/MSSQL/connections/" );
  settings.setValue( baseKey + "selected", txtName->text() );

  // warn if entry was renamed to an existing connection
  if ( ( mOriginalConnName.isNull() || mOriginalConnName.compare( txtName->text(), Qt::CaseInsensitive ) != 0 ) &&
       ( settings.contains( baseKey + txtName->text() + "/service" ) ||
         settings.contains( baseKey + txtName->text() + "/host" ) ) &&
       QMessageBox::question( this,
                              tr( "Save Connection" ),
                              tr( "Should the existing connection %1 be overwritten?" ).arg( txtName->text() ),
                              QMessageBox::Ok | QMessageBox::Cancel ) == QMessageBox::Cancel )
  {
    return;
  }

  // on rename delete the original entry first
  if ( !mOriginalConnName.isNull() && mOriginalConnName != txtName->text() )
  {
    settings.remove( baseKey + mOriginalConnName );
    settings.sync();
  }

  const QString connName = txtName->text();
  baseKey += connName;
  QString database;
  QListWidgetItem *item = listDatabase->currentItem();
  if ( item && item->text() != QLatin1String( "(from service)" ) )
  {
    database = item->text();
  }

  settings.setValue( baseKey + "/service", txtService->text() );
  settings.setValue( baseKey + "/host", txtHost->text() );
  settings.setValue( baseKey + "/database", database );
  settings.setValue( baseKey + "/username", chkStoreUsername->isChecked() ? txtUsername->text() : QString() );
  settings.setValue( baseKey + "/password", chkStorePassword->isChecked() ? txtPassword->text() : QString() );
  settings.setValue( baseKey + "/saveUsername", chkStoreUsername->isChecked() ? "true" : "false" );
  settings.setValue( baseKey + "/savePassword", chkStorePassword->isChecked() ? "true" : "false" );

  if ( groupBoxSchemasFilter->isChecked() )
  {
    if ( !mSchemaModel.dataBaseName().isEmpty() )
      mSchemaSettings.insert( mSchemaModel.dataBaseName(), mSchemaModel.schemasSettings() );
    settings.setValue( baseKey + "/schemasFiltered", mSchemaSettings );
  }

  settings.setValue( baseKey + "/schemasFiltering", groupBoxSchemasFilter->isChecked() );

  QgsMssqlConnection::setGeometryColumnsOnly( connName, cb_geometryColumns->isChecked() );
  QgsMssqlConnection::setAllowGeometrylessTables( connName, cb_allowGeometrylessTables->isChecked() );
  QgsMssqlConnection::setUseEstimatedMetadata( connName, cb_useEstimatedMetadata->isChecked() );
  QgsMssqlConnection::setInvalidGeometryHandlingDisabled( connName, mCheckNoInvalidGeometryHandling->isChecked() );

  QDialog::accept();
}

void QgsMssqlNewConnection::btnConnect_clicked()
{
  testConnection();
}

void QgsMssqlNewConnection::btnListDatabase_clicked()
{
  listDatabases();
}

void QgsMssqlNewConnection::cb_trustedConnection_clicked()
{
  if ( cb_trustedConnection->checkState() == Qt::Checked )
  {
    txtUsername->setEnabled( false );
    txtUsername->clear();
    txtPassword->setEnabled( false );
    txtPassword->clear();
  }
  else
  {
    txtUsername->setEnabled( true );
    txtPassword->setEnabled( true );
  }
}

//! End  Autoconnected SLOTS

bool QgsMssqlNewConnection::testConnection( const QString &testDatabase )
{
  bar->pushMessage( tr( "Testing connection" ), tr( "……" ) );
  // Gross but needed to show the last message.
  qApp->processEvents();

  if ( txtService->text().isEmpty() && txtHost->text().isEmpty() )
  {
    bar->clearWidgets();
    bar->pushWarning( tr( "Connection Failed" ), tr( "Host name hasn't been specified." ) );
    return false;
  }

  QString database;
  QListWidgetItem *item = listDatabase->currentItem();
  if ( !testDatabase.isEmpty() )
  {
    database = testDatabase;
  }
  else if ( item && item->text() != QLatin1String( "(from service)" ) )
  {
    database = item->text();
  }

  QSqlDatabase db = QgsMssqlConnection::getDatabase( txtService->text().trimmed(),
                    txtHost->text().trimmed(),
                    database,
                    txtUsername->text().trimmed(),
                    txtPassword->text().trimmed() );

  if ( db.isOpen() )
    db.close();

  if ( !db.open() )
  {
    bar->clearWidgets();
    bar->pushWarning( tr( "Error opening connection" ), db.lastError().text() );
    return false;
  }
  else
  {
    if ( database.isEmpty() )
    {
      database = txtService->text();
    }
    bar->clearWidgets();
  }

  return true;
}

void QgsMssqlNewConnection::listDatabases()
{
  testConnection( QStringLiteral( "master" ) );
  QString currentDataBase;
  if ( listDatabase->currentItem() )
    currentDataBase = listDatabase->currentItem()->text();
  listDatabase->clear();
  QString queryStr = QStringLiteral( "SELECT name FROM master..sysdatabases WHERE name NOT IN ('master', 'tempdb', 'model', 'msdb')" );

  QSqlDatabase db = QgsMssqlConnection::getDatabase( txtService->text().trimmed(),
                    txtHost->text().trimmed(),
                    QStringLiteral( "master" ),
                    txtUsername->text().trimmed(),
                    txtPassword->text().trimmed() );
  if ( db.open() )
  {
    QSqlQuery query = QSqlQuery( db );
    query.setForwardOnly( true );
    ( void )query.exec( queryStr );

    if ( !txtService->text().isEmpty() )
    {
      listDatabase->addItem( QStringLiteral( "(from service)" ) );
    }

    if ( query.isActive() )
    {
      while ( query.next() )
      {
        QString name = query.value( 0 ).toString();
        listDatabase->addItem( name );
      }
      listDatabase->setCurrentRow( 0 );
    }
    db.close();
  }

  for ( int i = 0; i < listDatabase->count(); ++i )
  {
    if ( listDatabase->item( i )->text() == currentDataBase )
    {
      listDatabase->setCurrentRow( i );
      break;
    }
  }
  onCurrentDataBaseChange();
}

void QgsMssqlNewConnection::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "managing_data_source/opening_data.html#connecting-to-mssql-spatial" ) );
}

void QgsMssqlNewConnection::updateOkButtonState()
{
  QListWidgetItem *item = listDatabase->currentItem();
  bool disabled = txtName->text().isEmpty() || ( txtService->text().isEmpty() && txtHost->text().isEmpty() ) || !item;
  buttonBox->button( QDialogButtonBox::Ok )->setDisabled( disabled );
}

void QgsMssqlNewConnection::onCurrentDataBaseChange()
{
  //Fisrt store the schema settings for the previous dataBase
  QVariantMap vm = mSchemaModel.schemasSettings();
  if ( !mSchemaModel.dataBaseName().isEmpty() )
    mSchemaSettings.insert( mSchemaModel.dataBaseName(), mSchemaModel.schemasSettings() );

  QString databaseName;
  if ( listDatabase->currentItem() )
    databaseName = listDatabase->currentItem()->text();

  QSqlDatabase db = QgsMssqlConnection::getDatabase( txtService->text().trimmed(),
                    txtHost->text().trimmed(),
                    databaseName,
                    txtUsername->text().trimmed(),
                    txtPassword->text().trimmed() );

  QStringList schemasList = QgsMssqlConnection::schemas( db, nullptr );

  QVariantMap newSchemaSettings = mSchemaSettings.value( databaseName ).toMap();

  for ( const QString &sch : newSchemaSettings.keys() )
  {
    if ( !schemasList.contains( sch ) )
      newSchemaSettings.remove( sch );
  }

  for ( const QString &sch : schemasList )
  {
    if ( !newSchemaSettings.contains( sch ) && !QgsMssqlConnection::isSystemSchema( sch ) )
      newSchemaSettings.insert( sch, true );
  }

  mSchemaModel.setDataBaseName( databaseName );
  mSchemaModel.setSchemasSetting( newSchemaSettings );
}

QgsMssqlNewConnection::SchemaModel::SchemaModel( QObject *parent ): QAbstractListModel( parent )
{}

int QgsMssqlNewConnection::SchemaModel::rowCount( const QModelIndex &parent ) const
{
  Q_UNUSED( parent )
  return mSchemas.count();
}

QVariant QgsMssqlNewConnection::SchemaModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() || index.row() >= mSchemas.count() )
    return QVariant();

  QList<QString> schemasName = mSchemas.keys();

  switch ( role )
  {
    case Qt::CheckStateRole:
      if ( mSchemas.value( schemasName.at( index.row() ) ).toBool() )
        return Qt::CheckState::Checked;
      else
        return Qt::CheckState::Unchecked;
      break;
    case Qt::DisplayRole:
      return schemasName.at( index.row() );
      break;
    default:
      return QVariant();
  }

  return QVariant();
}

bool QgsMssqlNewConnection::SchemaModel::setData( const QModelIndex &index, const QVariant &value, int role )
{
  if ( !index.isValid() || index.row() >= mSchemas.count() )
    return false;

  QList<QString> schemasName = mSchemas.keys();
  switch ( role )
  {
    case Qt::CheckStateRole:
      mSchemas[schemasName.at( index.row() )] = value;
      return true;
      break;
    default:
      return false;
  }

  return false;
}

Qt::ItemFlags QgsMssqlNewConnection::SchemaModel::flags( const QModelIndex &index ) const
{
  return QAbstractListModel::flags( index ) | Qt::ItemFlag::ItemIsUserCheckable;
}

void QgsMssqlNewConnection::SchemaModel::setSchemasSetting( const QVariantMap &schemas )
{
  beginResetModel();
  mSchemas = schemas;
  endResetModel();
}

QVariantMap QgsMssqlNewConnection::SchemaModel::schemasSettings() const
{
  return mSchemas;
}

QString QgsMssqlNewConnection::SchemaModel::dataBaseName() const
{
  return mDataBaseName;
}

void QgsMssqlNewConnection::SchemaModel::setDataBaseName( const QString &dataBaseName )
{
  mDataBaseName = dataBaseName;
}
