/***************************************************************************
    qgsauthhmacsha256edit.cpp
    ------------------------
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

#include "qgsauthhmacsha256edit.h"
#include "ui_qgsauthhmacsha256edit.h"


QgsAuthHmacSha256Edit::QgsAuthHmacSha256Edit( QWidget *parent )
  : QgsAuthMethodEdit( parent )
{
  setupUi( this );
  connect( mKeyEdit, &QLineEdit::textChanged, this, &QgsAuthHmacSha256Edit::configChanged );
  connect( mTokenEdit, &QPlainTextEdit::textChanged, this, &QgsAuthHmacSha256Edit::configChanged );
}

bool QgsAuthHmacSha256Edit::validateConfig()
{
  const bool curvalid = !mTokenEdit->toPlainText().isEmpty() && !mKeyEdit->text().isEmpty();
  if ( mValid != curvalid )
  {
    mValid = curvalid;
    emit validityChanged( curvalid );
  }
  return curvalid;
}

QgsStringMap QgsAuthHmacSha256Edit::configMap() const
{
  QgsStringMap config;
  config.insert( QStringLiteral( "key" ), mKeyEdit->text() );
  config.insert( QStringLiteral( "token" ), mTokenEdit->toPlainText() );

  return config;
}

void QgsAuthHmacSha256Edit::loadConfig( const QgsStringMap &configmap )
{
  clearConfig();

  mConfigMap = configmap;
  mKeyEdit->setText( configmap.value( QStringLiteral( "key" ) ) );
  mTokenEdit->setPlainText( configmap.value( QStringLiteral( "token" ) ) );

  validateConfig();
}

void QgsAuthHmacSha256Edit::resetConfig()
{
  loadConfig( mConfigMap );
}

void QgsAuthHmacSha256Edit::clearConfig()
{
  mKeyEdit->clear();
  mTokenEdit->clear();
}

void QgsAuthHmacSha256Edit::configChanged()
{
  validateConfig();
}
