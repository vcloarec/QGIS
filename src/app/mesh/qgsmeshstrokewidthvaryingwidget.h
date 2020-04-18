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
    Q_OBJECT
  public:
    QgsMeshStrokeWidthVaryingButton( QWidget *parent = nullptr );

    QgsMeshStrokeWidth strokeWidthVarying() const;
    void setStrokeWidthVarying( const QgsMeshStrokeWidth &strokeWidthVarying );
    void setDefaultMinMaxValue( double minimum, double maximum );

  signals:
    void widgetChanged();

  private slots:
    void openWidget();

  private:
    void updateText();

    QgsMeshStrokeWidth mStrokeWidthVarying;
    double mMinimumDefaultValue;
    double mMaximumDefaultValue;
};

class QgsMeshStrokeWidthVaryingWidget: public QgsPanelWidget, public Ui::QgsMeshVaryingStrokeWidthWidget
{
  public:
    QgsMeshStrokeWidthVaryingWidget( const QgsMeshStrokeWidth &strokeWidthVarying, QWidget *parent = nullptr );
    QgsMeshStrokeWidth varyingStrokeWidth() const;
};

#endif // QGSMESHSTROKEWIDTHVARYINGWIDGET_H
