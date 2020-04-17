/***************************************************************************
    qgsmeshstrokewidthvaryingtwidget.cpp
    -------------------------------------
    begin                : April 2020
    copyright            : (C) 2020 by Vincent Cloarec
    email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmeshstrokewidthvaryingwidget.h"

QgsMeshStrokeWidthVaryingWidget::QgsMeshStrokeWidthVaryingWidget( const QgsMeshStrokeWidthVarying &strokeWidthVarying, QWidget *parent )
{
  setupUi( this );
  setPanelTitle( tr( "Varying Stroke Width" ) );
}
