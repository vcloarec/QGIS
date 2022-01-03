/***************************************************************************
    qgsauthhmacsha256method.cpp
    --------------------------
    begin                : January 2022
    copyright            : (C) 2022 by Vincent Cloarec
    author               : Vincent Cloarec
    email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsauthhmacsha256method.h"

#include <QMessageAuthenticationCode>

#include "qgsauthmanager.h"
#include "qgslogger.h"
#include "qgsapplication.h"

#ifdef HAVE_GUI
#include "qgsauthhmacsha256edit.h"
#endif


const QString QgsAuthHmacSha256Method::AUTH_METHOD_KEY = QStringLiteral( "HmacSha256" );
const QString QgsAuthHmacSha256Method::AUTH_METHOD_DESCRIPTION = QStringLiteral( "HMAC SHA256" );
const QString QgsAuthHmacSha256Method::AUTH_METHOD_DISPLAY_DESCRIPTION = tr( "HMAC SHA256" );

QMap<QString, QgsAuthMethodConfig> QgsAuthHmacSha256Method::sAuthConfigCache = QMap<QString, QgsAuthMethodConfig>();


QgsAuthHmacSha256Method::QgsAuthHmacSha256Method()
{
  setVersion( 1 );
  setExpansions( QgsAuthMethod::NetworkRequest );
  setDataProviders( QStringList()
                    << QStringLiteral( "wms" )
                    << QStringLiteral( "vectortile" ) );

}

QString QgsAuthHmacSha256Method::key() const
{
  return AUTH_METHOD_KEY;
}

QString QgsAuthHmacSha256Method::description() const
{
  return AUTH_METHOD_DESCRIPTION;
}

QString QgsAuthHmacSha256Method::displayDescription() const
{
  return AUTH_METHOD_DISPLAY_DESCRIPTION;
}

bool QgsAuthHmacSha256Method::updateNetworkRequest( QNetworkRequest &request, const QString &authcfg,
    const QString &dataprovider )
{
  Q_UNUSED( dataprovider )
  const QgsAuthMethodConfig mconfig = getMethodConfig( authcfg );
  if ( !mconfig.isValid() )
  {
    QgsDebugMsg( QStringLiteral( "Update request config FAILED for authcfg: %1: config invalid" ).arg( authcfg ) );
    return false;
  }

  const QString key = mconfig.config( QStringLiteral( "key" ) );
  const QString token = mconfig.config( QStringLiteral( "token" ) );

  QString baseUrl = request.url().toString();

  QString keyedUrl = baseUrl + QStringLiteral( "?key=" ) + key;

  QByteArray signature = calculateSignature( token, keyedUrl );

  request.setUrl( QString( keyedUrl + QStringLiteral( "&signature=" )  + signature ) );

  return true;
}

void QgsAuthHmacSha256Method::clearCachedConfig( const QString &authcfg )
{
  removeMethodConfig( authcfg );
}

void QgsAuthHmacSha256Method::updateMethodConfig( QgsAuthMethodConfig &mconfig )
{
  if ( mconfig.hasConfig( QStringLiteral( "oldconfigstyle" ) ) )
  {
    QgsDebugMsg( QStringLiteral( "Updating old style auth method config" ) );
  }

  // NOTE: add updates as method version() increases due to config storage changes
}

QgsAuthMethodConfig QgsAuthHmacSha256Method::getMethodConfig( const QString &authcfg, bool fullconfig )
{
  const QMutexLocker locker( &mMutex );
  QgsAuthMethodConfig mconfig;

  // check if it is cached
  if ( sAuthConfigCache.contains( authcfg ) )
  {
    mconfig = sAuthConfigCache.value( authcfg );
    QgsDebugMsg( QStringLiteral( "Retrieved config for authcfg: %1" ).arg( authcfg ) );
    return mconfig;
  }

  // else build basic bundle
  if ( !QgsApplication::authManager()->loadAuthenticationConfig( authcfg, mconfig, fullconfig ) )
  {
    QgsDebugMsg( QStringLiteral( "Retrieve config FAILED for authcfg: %1" ).arg( authcfg ) );
    return QgsAuthMethodConfig();
  }

  // cache bundle
  putMethodConfig( authcfg, mconfig );

  return mconfig;
}

void QgsAuthHmacSha256Method::putMethodConfig( const QString &authcfg, const QgsAuthMethodConfig &mconfig )
{
  const QMutexLocker locker( &mMutex );
  QgsDebugMsg( QStringLiteral( "Putting token config for authcfg: %1" ).arg( authcfg ) );
  sAuthConfigCache.insert( authcfg, mconfig );
}

void QgsAuthHmacSha256Method::removeMethodConfig( const QString &authcfg )
{
  const QMutexLocker locker( &mMutex );
  if ( sAuthConfigCache.contains( authcfg ) )
  {
    sAuthConfigCache.remove( authcfg );
    QgsDebugMsg( QStringLiteral( "Removed token config for authcfg: %1" ).arg( authcfg ) );
  }
}

QByteArray QgsAuthHmacSha256Method::calculateSignature( const QString &token, const QString &keyedUrl )
{
  QByteArray decodedToken = QByteArray::fromHex( token.toStdString().c_str() );

  QMessageAuthenticationCode authCode( QCryptographicHash::Sha256, decodedToken );
  authCode.addData( keyedUrl.toUtf8() );

  return authCode.result().toBase64( QByteArray::Base64UrlEncoding );
}

#ifdef HAVE_GUI
QWidget *QgsAuthHmacSha256Method::editWidget( QWidget *parent ) const
{
  return new QgsAuthHmacSha256Edit( parent );
}
#endif

//////////////////////////////////////////////
// Plugin externals
//////////////////////////////////////////////


#ifndef HAVE_STATIC_PROVIDERS
QGISEXTERN QgsAuthMethodMetadata *authMethodMetadataFactory()
{
  return new QgsAuthHmacSha256MethodMetadata();
}
#endif
