/***************************************************************************
    qgsmeshrenderervectorsettingswidget.cpp
    ---------------------------------------
    begin                : June 2018
    copyright            : (C) 2018 by Peter Petrik
    email                : zilolv at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmeshrenderervectorsettingswidget.h"

#include "qgis.h"
#include "qgsmeshlayer.h"
#include "qgsmessagelog.h"

QgsMeshRendererVectorSettingsWidget::QgsMeshRendererVectorSettingsWidget( QWidget *parent )
  : QWidget( parent )

{
  setupUi( this );

  mShaftLengthComboBox->setCurrentIndex( -1 );

  //***
  connect( mColorWidget, &QgsColorButton::colorChanged, this, &QgsMeshRendererVectorSettingsWidget::widgetChanged );
  connect( mLineWidthSpinBox, qgis::overload<double>::of( &QgsDoubleSpinBox::valueChanged ),
           this, &QgsMeshRendererVectorSettingsWidget::widgetChanged );

  connect( mShaftLengthComboBox, qgis::overload<int>::of( &QComboBox::currentIndexChanged ),
           this, &QgsMeshRendererVectorSettingsWidget::widgetChanged );

  connect( mShaftLengthComboBox, qgis::overload<int>::of( &QComboBox::currentIndexChanged ),
           mShaftOptionsStackedWidget, &QStackedWidget::setCurrentIndex );

  connect( mDisplayVectorsOnGridGroupBox, &QGroupBox::toggled, this, &QgsMeshRendererVectorSettingsWidget::widgetChanged );

  //***

  QVector<QLineEdit *> widgets;
  widgets << mMinMagLineEdit << mMaxMagLineEdit
          << mHeadWidthLineEdit << mHeadLengthLineEdit
          << mMinimumShaftLineEdit << mMaximumShaftLineEdit
          << mScaleShaftByFactorOfLineEdit << mShaftLengthLineEdit;

  for ( auto widget : widgets )
  {
    connect( widget, &QLineEdit::textChanged, this, &QgsMeshRendererVectorSettingsWidget::widgetChanged );
  }

  connect( mXSpacingSpinBox, qgis::overload<int>::of( &QgsSpinBox::valueChanged ), this, &QgsMeshRendererVectorSettingsWidget::widgetChanged );
  connect( mYSpacingSpinBox, qgis::overload<int>::of( &QgsSpinBox::valueChanged ), this, &QgsMeshRendererVectorSettingsWidget::widgetChanged );

  //*****

  connect( mDisplayingVectorMethodcomboBox, qgis::overload<int>::of( &QComboBox::currentIndexChanged ),
           mDisplayingMethodStacked, &QStackedWidget::setCurrentIndex );
  mDisplayingMethodStacked->setCurrentIndex( 0 );

  connect( mStreamlinesFixedColorRadioButton, &QRadioButton::toggled, mStreamlinesFixeColorButton, &QWidget::setVisible );
  connect( mStreamlinesColorRampRadioButton, &QRadioButton::toggled, mStreamlinesColorRampShaderWidget, &QWidget::setVisible );

  connect( mStreamlinesMaxMaglineEdit, &QLineEdit::textChanged, this, &QgsMeshRendererVectorSettingsWidget::onMinMaxChange );
  connect( mStreamlinesMinMaglineEdit, &QLineEdit::textChanged, this, &QgsMeshRendererVectorSettingsWidget::onMinMaxChange );
  onMinMaxChange();

  mGroupDataSetModel = new QgsMeshDatasetGroupTreeModelSelectable( this );
  mStreamlinesScalarWeightcomboBox->setModel( mGroupDataSetModel );

}

void QgsMeshRendererVectorSettingsWidget::setLayer( QgsMeshLayer *layer )
{
  mMeshLayer = layer;
}

QgsMeshRendererVectorSettings QgsMeshRendererVectorSettingsWidget::settings() const
{
  QgsMeshRendererVectorSettings  settings;
  settings.setDisplayingMethod(
    static_cast<QgsMeshRendererVectorSettings::DisplayingMethod>( mDisplayingVectorMethodcomboBox->currentIndex() ) );

  //Arrow settings
  QgsMeshRendererVectorArrowSettings arrowSettings;

  // basic
  arrowSettings.setColor( mColorWidget->color() );
  arrowSettings.setLineWidth( mLineWidthSpinBox->value() );

  // filter by magnitude
  double val = filterValue( mMinMagLineEdit->text(), -1 );
  arrowSettings.setFilterMin( val );

  val = filterValue( mMaxMagLineEdit->text(), -1 );
  arrowSettings.setFilterMax( val );

  // arrow head
  val = filterValue( mHeadWidthLineEdit->text(), arrowSettings.arrowHeadWidthRatio() * 100.0 );
  arrowSettings.setArrowHeadWidthRatio( val / 100.0 );

  val = filterValue( mHeadLengthLineEdit->text(), arrowSettings.arrowHeadLengthRatio() * 100.0 );
  arrowSettings.setArrowHeadLengthRatio( val / 100.0 );

  // user grid
  bool enabled = mDisplayVectorsOnGridGroupBox->isChecked();
  arrowSettings.setOnUserDefinedGrid( enabled );
  arrowSettings.setUserGridCellWidth( mXSpacingSpinBox->value() );
  arrowSettings.setUserGridCellHeight( mYSpacingSpinBox->value() );

  // shaft length
  auto method = static_cast<QgsMeshRendererVectorArrowSettings::ArrowScalingMethod>( mShaftLengthComboBox->currentIndex() );
  arrowSettings.setShaftLengthMethod( method );

  val = filterValue( mMinimumShaftLineEdit->text(), arrowSettings.minShaftLength() );
  arrowSettings.setMinShaftLength( val );

  val = filterValue( mMaximumShaftLineEdit->text(), arrowSettings.maxShaftLength() );
  arrowSettings.setMaxShaftLength( val );

  val = filterValue( mScaleShaftByFactorOfLineEdit->text(), arrowSettings.scaleFactor() );
  arrowSettings.setScaleFactor( val );

  val = filterValue( mShaftLengthLineEdit->text(), arrowSettings.fixedShaftLength() );
  arrowSettings.setFixedShaftLength( val );

  settings.setArrowsSettings( arrowSettings );

  //Streamline setting
  QgsMeshRendererVectorStreamlineSettings streamlineSettings;
  streamlineSettings.setSeedingMethod(
    static_cast<QgsMeshRendererVectorStreamlineSettings::SeedingStartPointsMethod>( mStreamlinesSeedingMethodComboBox->currentIndex() ) );

  streamlineSettings.setSeedingDensity( mStreamlinesDensitySpinBox->value() / 100 );
  streamlineSettings.setLineWidth( mStreamlinesWidthSpinBox->value() );

  streamlineSettings.setIsWeightWithScalar( mStreamlinesWeightWithScalarCheckBox->isChecked() );
  streamlineSettings.setWeightDatasetGroupScalarIndex( mStreamlinesScalarWeightcomboBox->currentIndex() );
  streamlineSettings.setMinMagFilter( filterValue( mStreamlinesMinMaglineEdit->text(), -1 ) );
  streamlineSettings.setMaxMagFilter( filterValue( mStreamlinesMaxMaglineEdit->text(), -1 ) );

  QgsMeshRendererVectorStreamlineSettings::ColorMethod colorMethod;
  if ( mStreamlinesFixedColorRadioButton->isChecked() )
    colorMethod = QgsMeshRendererVectorStreamlineSettings::Fixe;
  else
    colorMethod = QgsMeshRendererVectorStreamlineSettings::ColorRamp;

  streamlineSettings.setColorMethod( colorMethod );
  streamlineSettings.setFixedColor( mStreamlinesFixeColorButton->color() );
  streamlineSettings.setColorRampShader( mStreamlinesColorRampShaderWidget->shader() );


  settings.setStreamLinesSettings( streamlineSettings );

  return settings;
}

void QgsMeshRendererVectorSettingsWidget::syncToLayer( )
{
  if ( !mMeshLayer )
    return;

  if ( mActiveDatasetGroup < 0 )
    return;

  mGroupDataSetModel->syncToLayer( mMeshLayer );

  const QgsMeshRendererSettings rendererSettings = mMeshLayer->rendererSettings();
  const QgsMeshRendererVectorSettings settings = rendererSettings.vectorSettings( mActiveDatasetGroup );

  mDisplayingVectorMethodcomboBox->setCurrentIndex( settings.displayingMethod() );

  // Arrow settings
  const QgsMeshRendererVectorArrowSettings arrowSettings = settings.arrowSettings();

  // basic
  mColorWidget->setColor( arrowSettings.color() );
  mLineWidthSpinBox->setValue( arrowSettings.lineWidth() );

  // filter by magnitude
  if ( arrowSettings.filterMin() > 0 )
  {
    mMinMagLineEdit->setText( QString::number( arrowSettings.filterMin() ) );
  }
  if ( arrowSettings.filterMax() > 0 )
  {
    mMaxMagLineEdit->setText( QString::number( arrowSettings.filterMax() ) );
  }

  // arrow head
  mHeadWidthLineEdit->setText( QString::number( arrowSettings.arrowHeadWidthRatio() * 100.0 ) );
  mHeadLengthLineEdit->setText( QString::number( arrowSettings.arrowHeadLengthRatio() * 100.0 ) );

  // user grid
  mDisplayVectorsOnGridGroupBox->setChecked( arrowSettings.isOnUserDefinedGrid() );
  mXSpacingSpinBox->setValue( arrowSettings.userGridCellWidth() );
  mYSpacingSpinBox->setValue( arrowSettings.userGridCellHeight() );

  // shaft length
  mShaftLengthComboBox->setCurrentIndex( arrowSettings.shaftLengthMethod() );

  mMinimumShaftLineEdit->setText( QString::number( arrowSettings.minShaftLength() ) );
  mMaximumShaftLineEdit->setText( QString::number( arrowSettings.maxShaftLength() ) );
  mScaleShaftByFactorOfLineEdit->setText( QString::number( arrowSettings.scaleFactor() ) );
  mShaftLengthLineEdit->setText( QString::number( arrowSettings.fixedShaftLength() ) );

  //Streamlines settings
  const QgsMeshRendererVectorStreamlineSettings streamlinesSettings = settings.streamLinesSettings();;

  mStreamlinesSeedingMethodComboBox->setCurrentIndex( streamlinesSettings.seedingMethod() );
  mStreamlinesDensitySpinBox->setValue( streamlinesSettings.seedingDensity() * 100 );
  mStreamlinesWidthSpinBox->setValue( streamlinesSettings.lineWidth() );

  mStreamlinesWeightWithScalarCheckBox->setChecked( streamlinesSettings.isWeightWithScalar() );
  mStreamlinesScalarWeightcomboBox->setCurrentIndex( streamlinesSettings.weightDatasetGroupScalarIndex() );

  if ( streamlinesSettings.minMagFilter() > 0 )
    mStreamlinesMinMaglineEdit->setText( QString::number( streamlinesSettings.minMagFilter() ) );

  if ( streamlinesSettings.maxMagFilter() > 0 )
    mStreamlinesMaxMaglineEdit->setText( QString::number( streamlinesSettings.maxMagFilter() ) );

  if ( streamlinesSettings.colorMethod() == QgsMeshRendererVectorStreamlineSettings::Fixe )
  {
    mStreamlinesFixedColorRadioButton->setChecked( true );
    mStreamlinesColorRampShaderWidget->hide();
  }

  if ( streamlinesSettings.colorMethod() == QgsMeshRendererVectorStreamlineSettings::ColorRamp )
  {
    mStreamlinesColorRampRadioButton->setChecked( true );
    mStreamlinesFixeColorButton->hide();

  }

  mStreamlinesFixeColorButton->setColor( streamlinesSettings.fixedColor() );
  mStreamlinesColorRampShaderWidget->setFromShader( streamlinesSettings.colorRampShader() );



}

void QgsMeshRendererVectorSettingsWidget::onMinMaxChange()
{
  if ( !mMeshLayer )
    return;

  if ( mActiveDatasetGroup < 0 )
    return;

  const QgsMeshDatasetGroupMetadata datasetGroupMetadata = mMeshLayer->dataProvider()->datasetGroupMetadata( mActiveDatasetGroup );
  double magMinimum = datasetGroupMetadata.minimum();
  double magMaximum = datasetGroupMetadata.maximum();

  mStreamlinesColorRampShaderWidget->setMinimumMaximum( filterValue( mStreamlinesMinMaglineEdit->text(), magMinimum ),
      filterValue( mStreamlinesMaxMaglineEdit->text(), magMaximum ) );


}

double QgsMeshRendererVectorSettingsWidget::filterValue( const QString &text, double errVal ) const
{
  if ( text.isEmpty() )
    return errVal;

  bool ok;
  double val = text.toDouble( &ok );
  if ( !ok )
    return errVal;

  if ( val < 0 )
    return errVal;

  return val;
}
