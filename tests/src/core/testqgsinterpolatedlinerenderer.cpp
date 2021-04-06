/***************************************************************************
  testqgsinterpolatedlinerenderer.cpp

 ---------------------
 begin                : 29.3.2021
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

#include "qgstest.h"
#include <QObject>

//qgis includes...
#include <qgsmapsettings.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsapplication.h>
#include <qgsproviderregistry.h>
#include <qgsproject.h>
#include <qgssymbol.h>
#include <qgsinterpolatedlinerenderer.h>
#include <qgsexpressioncontextutils.h>
#include <qgsmultirenderchecker.h>
#include <qgsstyle.h>


/**
 * \ingroup UnitTests
 * This is a unit test for 25d renderer.
 */
class TestQgsInterpolaledLineRenderer : public QObject
{
    Q_OBJECT
  public:
    TestQgsInterpolaledLineRenderer() = default;

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {} // will be called before each testfunction is executed.
    void cleanup() {} // will be called after every testfunction.

    void render();

  private:
    bool imageCheck( const QString &type );
    QgsMapSettings mMapSettings;
    std::unique_ptr<QgsVectorLayer> mLinesLayer = nullptr;
    QString mTestDataDir;
    QString mReport;
};


void TestQgsInterpolaledLineRenderer::initTestCase()
{
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();
  QgsApplication::showSettings();

  //create some objects that will be used in all tests...
  QString myDataDir( TEST_DATA_DIR ); //defined in CmakeLists.txt
  mTestDataDir = myDataDir + '/';

  //
  //create a poly layer that will be used in all tests...
  mLinesLayer.reset( new QgsVectorLayer( QStringLiteral( "LineString" ),
                                         QStringLiteral( "lines" ),
                                         QStringLiteral( "memory" ) ) );

  QList<QgsField> fields;
  fields << QgsField( QStringLiteral( "first_value" ), QVariant::Double )
         << QgsField( QStringLiteral( "second_value" ), QVariant::Double ) ;
  mLinesLayer->dataProvider()->addAttributes( fields );


  QStringList wktLines;

  wktLines << QStringLiteral( "LineString (3.0 7.0 , 6.0 19.0 , 25.0 19.0)" )
           << QStringLiteral( "LineString (23.0 7.0 , 10.0 7.0 , 23.0 16.0 , 10.0 16.0)" )
           << QStringLiteral( "LineString (6.0 5.0 , 6.0 2.0 , 23.0 2.0)" );

  QList<QPair<double, double>> values;
  values << QPair<double, double> {0, 10}
         << QPair<double, double> {3, 9}
         << QPair<double, double> {4, 7};

  QgsFeatureList flist;
  for ( int i = 0; i < wktLines.count(); ++i )
  {
    QgsFeature feat;
    feat.setGeometry( QgsGeometry::fromWkt( wktLines.at( i ) ) );
    feat.setFields( mLinesLayer->dataProvider()->fields(), true );
    feat.setAttribute( QStringLiteral( "first_value" ), values.at( i ).first );
    feat.setAttribute( QStringLiteral( "second_value" ), values.at( i ).second );
    flist << feat;
  }

  mLinesLayer->dataProvider()->addFeatures( flist );

  mLinesLayer->reload();
  QVERIFY( mLinesLayer->fields().count() == 2 );
  QVERIFY( mLinesLayer->featureCount() == 3 );

  mMapSettings.setLayers( QList<QgsMapLayer *>() << mLinesLayer.get() );
  mReport += QLatin1String( "<h1>Interpolated Line Renderer Tests</h1>\n" );

}

void TestQgsInterpolaledLineRenderer::cleanupTestCase()
{
  QString myReportFile = QDir::tempPath() + "/qgistest.html";
  QFile myFile( myReportFile );
  if ( myFile.open( QIODevice::WriteOnly | QIODevice::Append ) )
  {
    QTextStream myQTextStream( &myFile );
    myQTextStream << mReport;
    myFile.close();
  }

  mLinesLayer.release();

  QgsApplication::exitQgis();
}

void TestQgsInterpolaledLineRenderer::render()
{
  mReport += QLatin1String( "<h2>Render</h2>\n" );

  //setup 25d renderer
  QgsInterpolatedLineFeatureRenderer *renderer = new QgsInterpolatedLineFeatureRenderer();

  QgsColorRampShader colorRamp( 0, 10, QgsStyle::defaultStyle()->colorRamp( QStringLiteral( "Viridis" ) ), QgsColorRampShader::Exact );
  colorRamp.classifyColorRamp( 10, -1 );
  renderer->setColorRampShader( colorRamp );

  QgsInterpolatedLineWidth interpolatedLineWidth;
  interpolatedLineWidth.setMaximumValue( 10 );
  interpolatedLineWidth.setMinimumValue( 0 );
  interpolatedLineWidth.setMinimumWidth( 0 );
  interpolatedLineWidth.setMaximumWidth( 10 );
  interpolatedLineWidth.setIsVariableWidth( true );
  renderer->setInterpolatedWidth( interpolatedLineWidth );
  renderer->setWidthUnit( QgsUnitTypes::RenderUnit::RenderPixels );
  renderer->setExpressionsString( QStringLiteral( "first_value" ), QStringLiteral( "second_value" ) );

  mLinesLayer->setRenderer( renderer );

  QVERIFY( imageCheck( "interpolated_line_render_exact" ) );

  colorRamp.setColorRampType( QgsColorRampShader::Interpolated );
  colorRamp.classifyColorRamp( 10, -1 );
  renderer->setColorRampShader( colorRamp );
  QVERIFY( imageCheck( "interpolated_line_render_linear" ) );

  colorRamp = QgsColorRampShader( 0, 10, QgsStyle::defaultStyle()->colorRamp( QStringLiteral( "RdGy" ) ), QgsColorRampShader::Discrete );;
  colorRamp.classifyColorRamp( 5, -1 );
  renderer->setColorRampShader( colorRamp );
  QVERIFY( imageCheck( "interpolated_line_render_discret" ) );
}

bool TestQgsInterpolaledLineRenderer::imageCheck( const QString &testType )
{
  //use the QgsRenderChecker test utility class to
  //ensure the rendered output matches our control image
  mMapSettings.setExtent( mLinesLayer->extent() );
  mMapSettings.setOutputSize( QSize( 200, 200 ) );
  mMapSettings.setOutputDpi( 96 );
  QgsExpressionContext context;
  context << QgsExpressionContextUtils::mapSettingsScope( mMapSettings );
  mMapSettings.setExpressionContext( context );
  QgsMultiRenderChecker myChecker;
  myChecker.setControlPathPrefix( QStringLiteral( "interpolated_line_renderer" ) );
  myChecker.setControlName( "expected_" + testType );
  myChecker.setMapSettings( mMapSettings );
  myChecker.setColorTolerance( 20 );
  bool myResultFlag = myChecker.runTest( testType, 500 );
  mReport += myChecker.report();
  return myResultFlag;
}

QGSTEST_MAIN( TestQgsInterpolaledLineRenderer )
#include "testqgsinterpolatedlinerenderer.moc"
