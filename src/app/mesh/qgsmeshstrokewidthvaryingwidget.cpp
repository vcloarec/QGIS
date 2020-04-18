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

#include <QDialog>
#include <QDialogButtonBox>

#include "qgssettings.h"

QgsMeshStrokeWidthVaryingWidget::QgsMeshStrokeWidthVaryingWidget( const QgsMeshStrokeWidth &strokeWidthVarying, QWidget *parent ):
  QgsPanelWidget( parent )
{
  setupUi( this );
  setPanelTitle( tr( "Varying Stroke Width" ) );

  mValueMinimumSpinBox->setValue( strokeWidthVarying.minimumValue() );
  mValueMaximumSpinBox->setValue( strokeWidthVarying.maximumValue() );
  mWidthMinimumSpinBox->setValue( strokeWidthVarying.minimumWidth() );
  mWidthMaximumSpinBox->setValue( strokeWidthVarying.maximumWidth() );
  mIgnoreOutOfRangecheckBox->setChecked( strokeWidthVarying.ignoreOutOfRange() );

  connect( mValueMinimumSpinBox, qgis::overload<double>::of( &QgsDoubleSpinBox::valueChanged ),
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mValueMaximumSpinBox, qgis::overload<double>::of( &QgsDoubleSpinBox::valueChanged ),
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mWidthMinimumSpinBox, qgis::overload<double>::of( &QgsDoubleSpinBox::valueChanged ),
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mWidthMaximumSpinBox, qgis::overload<double>::of( &QgsDoubleSpinBox::valueChanged ),
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mIgnoreOutOfRangecheckBox, &QCheckBox::toggled,
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
}

void QgsMeshStrokeWidthVaryingButton::setDefaultMinMaxValue( double minimum, double maximum )
{
  mMinimumDefaultValue = minimum;
  mMaximumDefaultValue = maximum;
}

QgsMeshStrokeWidth QgsMeshStrokeWidthVaryingWidget::varyingStrokeWidth() const
{
  QgsMeshStrokeWidth strokeWidth;
  strokeWidth.setMinimumValue( mValueMinimumSpinBox->value() );
  strokeWidth.setMaximumValue( mValueMaximumSpinBox->value() );
  strokeWidth.setMinimumWidth( mWidthMinimumSpinBox->value() );
  strokeWidth.setMaximumWidth( mWidthMaximumSpinBox->value() );
  strokeWidth.setIgnoreOutOfRange( mIgnoreOutOfRangecheckBox->isChecked() );

  return strokeWidth;
}

QgsMeshStrokeWidthVaryingButton::QgsMeshStrokeWidthVaryingButton( QWidget *parent ): QPushButton( parent )
{
  updateText();
  connect( this, &QPushButton::clicked, this, &QgsMeshStrokeWidthVaryingButton::openWidget );
}

QgsMeshStrokeWidth QgsMeshStrokeWidthVaryingButton::strokeWidthVarying() const
{
  return mStrokeWidthVarying;
}

void QgsMeshStrokeWidthVaryingButton::setStrokeWidthVarying( const QgsMeshStrokeWidth &strokeWidthVarying )
{
  mStrokeWidthVarying = strokeWidthVarying;
  updateText();
}

void QgsMeshStrokeWidthVaryingButton::openWidget()
{
  QgsPanelWidget *panel = QgsPanelWidget::findParentPanel( this );
  QgsMeshStrokeWidthVaryingWidget *widget = new QgsMeshStrokeWidthVaryingWidget( mStrokeWidthVarying, panel );

  if ( panel && panel->dockMode() )
  {
    connect( widget, &QgsMeshStrokeWidthVaryingWidget::widgetChanged, this, [this, widget]
    {
      //update strokeWidth
      this->setStrokeWidthVarying( widget->varyingStrokeWidth() );
      this->emit widgetChanged();
    } );

    panel->openPanel( widget );
    return;
  }
  else
  {
    // Show the dialog version if not in a panel
    QDialog *dlg = new QDialog( this );
    QString key = QStringLiteral( "/UI/paneldialog/%1" ).arg( widget->panelTitle() );
    QgsSettings settings;
    dlg->restoreGeometry( settings.value( key ).toByteArray() );
    dlg->setWindowTitle( widget->panelTitle() );
    dlg->setLayout( new QVBoxLayout() );
    dlg->layout()->addWidget( widget );
    QDialogButtonBox *buttonBox = new QDialogButtonBox( QDialogButtonBox::Cancel | QDialogButtonBox::Ok );
    connect( buttonBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept );
    connect( buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject );
    dlg->layout()->addWidget( buttonBox );

    if ( dlg->exec() == QDialog::Accepted )
    {
      this->setStrokeWidthVarying( widget->varyingStrokeWidth() );
      emit widgetChanged();
    }
    settings.setValue( key, dlg->saveGeometry() );
  }
}

void QgsMeshStrokeWidthVaryingButton::updateText()
{
  setText( QString( "%1 - %2" ).
           arg( QString::number( mStrokeWidthVarying.minimumWidth(), 'f', 1 ) ).
           arg( QString::number( mStrokeWidthVarying.maximumWidth(), 'f', 1 ) ) );
}
