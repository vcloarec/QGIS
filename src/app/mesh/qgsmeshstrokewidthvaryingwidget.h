/***************************************************************************
    qgsmeshstrokewidthvaryingtwidget.h
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

#ifndef QGSMESHSTROKEWIDTHVARYINGWIDGET_H
#define QGSMESHSTROKEWIDTHVARYINGWIDGET_H

#include "qgis_app.h"
#include "ui_qgsmeshvaryingstrokewidthwidgetbase.h"
#include "qgspanelwidget.h"
#include "qgsmeshlayerrenderer.h"

class QgsMeshStrokeWidthVaryingButton: public QPushButton
{
  public:
    QgsMeshStrokeWidthVaryingButton( const QgsMeshStrokeWidthVarying &strokeWidthVarying, QWidget *parent = nullptr ): QPushButton( parent )
    {
      QString buttonText( "%1 - %2" );
      //buttonText.arg( QString::number( strokeWidthVarying.minimumValue(), )

    }

  private:

};

class QgsMeshStrokeWidthVaryingWidget: public QgsPanelWidget, public Ui::QgsMeshVaryingStrokeWidthWidget
{
  public:
    QgsMeshStrokeWidthVaryingWidget( const QgsMeshStrokeWidthVarying &strokeWidthVarying, QWidget *parent = nullptr );

    void setDefaultMinMaxValue( double minimum, double maximum );

    QgsMeshStrokeWidthVarying varyingStrokeWidth() const;

  private:
    double mMinimumDefaultValue;
    double mMaximumDefaultValue;
};

#endif // QGSMESHSTROKEWIDTHVARYINGWIDGET_H
