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

QgsMeshStrokeWidthVaryingWidget::QgsMeshStrokeWidthVaryingWidget(
  const QgsMeshStrokeWidth &strokeWidthVarying,
  double defaultMinimumvalue,
  double defaultMaximumValue,
  QWidget *parent ):
  QgsPanelWidget( parent ),
  mDefaultMinimumValue( defaultMinimumvalue ),
  mDefaultMaximumValue( defaultMaximumValue )
{
  setupUi( this );
  setPanelTitle( tr( "Varying Stroke Width" ) );

  setVaryingStrokeWidth( strokeWidthVarying );

  connect( mDefaultMinMaxButton, &QPushButton::clicked, this, &QgsMeshStrokeWidthVaryingWidget::defaultMinMax );

  connect( mValueMinimumLineEdit, &QLineEdit::textEdited, this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mValueMaximumLineEdit, &QLineEdit::textEdited, this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mWidthMinimumSpinBox, qgis::overload<double>::of( &QgsDoubleSpinBox::valueChanged ),
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mWidthMaximumSpinBox, qgis::overload<double>::of( &QgsDoubleSpinBox::valueChanged ),
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
  connect( mIgnoreOutOfRangecheckBox, &QCheckBox::toggled,
           this, &QgsMeshStrokeWidthVaryingWidget::widgetChanged );
}

void QgsMeshStrokeWidthVaryingWidget::setVaryingStrokeWidth( const QgsMeshStrokeWidth &strokeWidthVarying )
{
  whileBlocking( mValueMinimumLineEdit )->setText( QString::number( strokeWidthVarying.minimumValue() ) );
  whileBlocking( mValueMaximumLineEdit )->setText( QString::number( strokeWidthVarying.maximumValue() ) );
  whileBlocking( mWidthMinimumSpinBox )->setValue( strokeWidthVarying.minimumWidth() );
  whileBlocking( mWidthMaximumSpinBox )->setValue( strokeWidthVarying.maximumWidth() );
  whileBlocking( mIgnoreOutOfRangecheckBox )->setChecked( strokeWidthVarying.ignoreOutOfRange() );
}

void QgsMeshStrokeWidthVaryingButton::setDefaultMinMaxValue( double minimum, double maximum )
{
  mMinimumDefaultValue = minimum;
  mMaximumDefaultValue = maximum;
}

QgsMeshStrokeWidth QgsMeshStrokeWidthVaryingWidget::varyingStrokeWidth() const
{
  QgsMeshStrokeWidth strokeWidth;
  strokeWidth.setMinimumValue( lineEditValue( mValueMinimumLineEdit ) );
  strokeWidth.setMaximumValue( lineEditValue( mValueMaximumLineEdit ) );
  strokeWidth.setMinimumWidth( mWidthMinimumSpinBox->value() );
  strokeWidth.setMaximumWidth( mWidthMaximumSpinBox->value() );
  strokeWidth.setIgnoreOutOfRange( mIgnoreOutOfRangecheckBox->isChecked() );

  return strokeWidth;
}

void QgsMeshStrokeWidthVaryingWidget::defaultMinMax()
{
  whileBlocking( mValueMinimumLineEdit )->setText( QString::number( mDefaultMinimumValue ) );
  whileBlocking( mValueMaximumLineEdit )->setText( QString::number( mDefaultMaximumValue ) );
  emit widgetChanged();
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
  QgsMeshStrokeWidthVaryingWidget *widget =
    new QgsMeshStrokeWidthVaryingWidget( mStrokeWidthVarying,
                                         mMinimumDefaultValue,
                                         mMaximumDefaultValue,
                                         panel );

  if ( panel && panel->dockMode() )
  {
    connect( widget, &QgsMeshStrokeWidthVaryingWidget::widgetChanged, this, [this, widget]
    {
      //update strokeWidth toward button
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
           arg( QString::number( mStrokeWidthVarying.minimumWidth(), 'g', 3 ) ).
           arg( QString::number( mStrokeWidthVarying.maximumWidth(), 'g', 3 ) ) );
}

double QgsMeshStrokeWidthVaryingWidget::lineEditValue( const QLineEdit *lineEdit ) const
{
  if ( lineEdit->text().isEmpty() )
  {
    return std::numeric_limits<double>::quiet_NaN();
  }

  return lineEdit->text().toDouble();
}

