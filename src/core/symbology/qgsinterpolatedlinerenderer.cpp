/***************************************************************************
  qgsinterpolatedlinerenderer.cpp
  --------------------------------------
  Date                 : April 2020
  Copyright            : (C) 2020 by Vincent Cloarec
  Email                : vcloarec at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QPainter>

#include "qgsinterpolatedlinerenderer.h"
#include "qgscolorramplegendnode.h"
#include "qgssymbollayerutils.h"


void QgsInterpolatedLineRenderer::setInterpolatedWidth( const QgsInterpolatedLineWidth &strokeWidth )
{
  mStrokeWidth = strokeWidth;
}

QgsInterpolatedLineWidth QgsInterpolatedLineRenderer::interpolatedLineWidth() const
{
  return mStrokeWidth;
}

void QgsInterpolatedLineRenderer::setInterpolatedColor( const QgsInterpolatedLineColor &strokeColoring )
{
  mStrokeColoring = strokeColoring;
}

QgsInterpolatedLineColor QgsInterpolatedLineRenderer::interpolatedColor() const
{
  return mStrokeColoring;
}

void QgsInterpolatedLineRenderer::setWidthUnit( const QgsUnitTypes::RenderUnit &strokeWidthUnit )
{
  mStrokeWidthUnit = strokeWidthUnit;
}

QgsUnitTypes::RenderUnit QgsInterpolatedLineRenderer::widthUnit() const
{
  return mStrokeWidthUnit;
}

void QgsInterpolatedLineRenderer::render( double value1, double value2, const QgsPointXY &pt1, const QgsPointXY &pt2, QgsRenderContext &context ) const
{
  QPainter *painter = context.painter();
  QgsScopedQPainterState painterState( painter );
  context.setPainterFlagsUsingContext( painter );

  const QgsMapToPixel &mapToPixel = context.mapToPixel();

  QgsPointXY point1 = pt1;
  QgsPointXY point2 = pt2;

  if ( value1 > value2 )
  {
    std::swap( value1, value2 );
    std::swap( point1, point2 );
  }

  QPointF p1 = mapToPixel.transform( point1 ).toQPointF();
  QPointF p2 = mapToPixel.transform( point2 ).toQPointF();
  QPointF dir = p2 - p1;
  double length = sqrt( pow( dir.x(), 2 ) + pow( dir.y(), 2 ) );
  QPointF diru = dir / length;
  QPointF orthu = QPointF( -diru.y(), diru.x() );

  QList<double> breakValues;
  QList<QColor> breakColors;
  QList<QLinearGradient> gradients;

  mStrokeColoring.graduatedColors( value1, value2, breakValues, breakColors, gradients );
  QColor selelectedColor = context.selectionColor();;

  if ( gradients.isEmpty() && !breakValues.empty() && !breakColors.isEmpty() ) //exact colors to render
  {
    Q_ASSERT( breakColors.count() == breakValues.count() );
    for ( int i = 0; i < breakValues.count(); ++i )
    {
      double value = breakValues.at( i );
      double width = context.convertToPainterUnits( mStrokeWidth.strokeWidth( value ), mStrokeWidthUnit );
      QPen pen( mSelected ? selelectedColor : breakColors.at( i ) );
      pen.setWidthF( width );
      pen.setCapStyle( Qt::PenCapStyle::RoundCap );
      painter->setPen( pen );
      QPointF point = p1 + dir * ( value - value1 ) / ( value2 - value1 );
      painter->drawPoint( point );
    }
  }
  else
  {
    double width1 = mStrokeWidth.strokeWidth( value1 );
    double width2 = mStrokeWidth.strokeWidth( value2 );

    if ( !std::isnan( width1 ) || !std::isnan( width2 ) ) // the two widths on extremity are not out of range and ignored
    {
      //Draw line cap
      QBrush brush( Qt::SolidPattern );
      QPen pen;
      int startAngle;
      startAngle = ( acos( -orthu.x() ) / M_PI ) * 180;
      if ( orthu.y() < 0 )
        startAngle = 360 - startAngle;

      bool outOfRange1 = std::isnan( width1 );
      bool outOfRange2 = std::isnan( width2 );

      if ( !outOfRange1 )
      {
        width1 = context.convertToPainterUnits( width1, mStrokeWidthUnit );
        QRectF capBox1( p1.x() - width1 / 2, p1.y() - width1 / 2, width1, width1 );
        brush.setColor( mSelected ? selelectedColor : mStrokeColoring.color( value1 ) );
        painter->setBrush( brush );
        pen.setBrush( brush );
        painter->setPen( pen );
        painter->drawPie( capBox1, ( startAngle - 1 ) * 16, 182 * 16 );
      }

      if ( !outOfRange2 )
      {
        width2 = context.convertToPainterUnits( width2, mStrokeWidthUnit ) ;
        QRectF capBox2( p2.x() - width2 / 2, p2.y() - width2 / 2, width2, width2 );
        brush.setColor( mSelected ? selelectedColor : mStrokeColoring.color( value2 ) );
        pen.setBrush( brush );
        painter->setBrush( brush );
        painter->setPen( pen );
        painter->drawPie( capBox2, ( startAngle + 179 ) * 16, 182 * 16 );
      }

      if ( ( gradients.isEmpty() && breakValues.empty() && breakColors.count() == 1 ) || mSelected ) //only one color to render
      {
        double startAdjusting = 0;
        if ( outOfRange1 )
          adjustLine( value1, value1, value2, width1, startAdjusting );


        double endAdjusting = 0;
        if ( outOfRange2 )
          adjustLine( value2, value1, value2, width2, endAdjusting );

        QPointF pointStartAdjusted = p1 + dir * startAdjusting;
        QPointF pointEndAdjusted  = p2 - dir * endAdjusting;

        QPolygonF varLine;
        double semiWidth1 = width1 / 2;
        double semiWidth2 = width2 / 2;

        varLine.append( pointStartAdjusted + orthu * semiWidth1 );
        varLine.append( pointEndAdjusted + orthu * semiWidth2 );
        varLine.append( pointEndAdjusted - orthu * semiWidth2 );
        varLine.append( pointStartAdjusted - orthu * semiWidth1 );

        QBrush brush( Qt::SolidPattern );
        brush.setColor( mSelected ? selelectedColor : breakColors.first() );
        painter->setBrush( brush );
        painter->setPen( pen );

        QPen pen;
        pen.setBrush( brush );
        pen.setWidthF( 0 );
        painter->setPen( pen );

        painter->drawPolygon( varLine );
      }
      else if ( !gradients.isEmpty() && !breakValues.isEmpty() && !breakColors.isEmpty() )
      {
        Q_ASSERT( breakColors.count() == breakValues.count() );
        Q_ASSERT( breakColors.count() == gradients.count() + 1 );

        for ( int i = 0; i < gradients.count(); ++i )
        {
          double firstValue = breakValues.at( i );
          double secondValue = breakValues.at( i + 1 );
          double w1 =  mStrokeWidth.strokeWidth( firstValue );
          double w2 =  mStrokeWidth.strokeWidth( secondValue );

          if ( std::isnan( w1 ) && std::isnan( w2 ) )
            continue;

          double firstAdjusting = 0;
          if ( std::isnan( w1 ) )
            adjustLine( firstValue, value1, value2, w1, firstAdjusting );


          double secondAdjusting = 0;
          if ( std::isnan( w2 ) )
            adjustLine( secondValue, value1, value2, w2, secondAdjusting );

          w1 = context.convertToPainterUnits( w1, mStrokeWidthUnit );
          w2 = context.convertToPainterUnits( w2, mStrokeWidthUnit ) ;

          QPointF pointStart = p1 + dir * ( firstValue - value1 ) / ( value2 - value1 );
          QPointF pointEnd = p1 + dir * ( secondValue - value1 ) / ( value2 - value1 );

          QPointF pointStartAdjusted = pointStart + dir * firstAdjusting;
          QPointF pointEndAdjusted  = pointEnd - dir * secondAdjusting;

          QPolygonF varLine;
          double sw1 = w1 / 2;
          double sw2 = w2 / 2;

          varLine.append( pointStartAdjusted + orthu * sw1 );
          varLine.append( pointEndAdjusted + orthu * sw2 );
          varLine.append( pointEndAdjusted - orthu * sw2 );
          varLine.append( pointStartAdjusted - orthu * sw1 );

          QLinearGradient gradient = gradients.at( i );
          gradient.setStart( pointStart );
          gradient.setFinalStop( pointEnd );
          QBrush brush( gradient );
          painter->setBrush( brush );

          QPen pen;
          pen.setBrush( brush );
          pen.setWidthF( 0 );
          painter->setPen( pen );

          painter->drawPolygon( varLine );
        }
      }
    }
  }
}

void QgsInterpolatedLineRenderer::setSelected( bool selected )
{
  mSelected = selected;
}

void QgsInterpolatedLineRenderer::adjustLine( const double &value, const double &value1, const double &value2, double &width, double &adjusting ) const
{
  if ( value > mStrokeWidth.maximumValue() )
  {
    adjusting = fabs( ( value - mStrokeWidth.maximumValue() ) / ( value2 - value1 ) );
    width = mStrokeWidth.maximumWidth();
  }
  else
  {
    adjusting = fabs( ( value - mStrokeWidth.minimumValue() ) / ( value2 - value1 ) );
    width = mStrokeWidth.minimumWidth();
  }
}

double QgsInterpolatedLineWidth::minimumValue() const
{
  return mMinimumValue;
}

void QgsInterpolatedLineWidth::setMinimumValue( double minimumValue )
{
  mMinimumValue = minimumValue;
  mNeedUpdateFormula = true;
}

double QgsInterpolatedLineWidth::maximumValue() const
{
  return mMaximumValue;
}

void QgsInterpolatedLineWidth::setMaximumValue( double maximumValue )
{
  mMaximumValue = maximumValue;
  mNeedUpdateFormula = true;
}

double QgsInterpolatedLineWidth::minimumWidth() const
{
  return mMinimumWidth;
}

void QgsInterpolatedLineWidth::setMinimumWidth( double minimumWidth )
{
  mMinimumWidth = minimumWidth;
  mNeedUpdateFormula = true;
}

double QgsInterpolatedLineWidth::maximumWidth() const
{
  return mMaximumWidth;
}

void QgsInterpolatedLineWidth::setMaximumWidth( double maximumWidth )
{
  mMaximumWidth = maximumWidth;
  mNeedUpdateFormula = true;
}

double QgsInterpolatedLineWidth::strokeWidth( double value ) const
{
  if ( mIsWidthVariable )
  {
    if ( mNeedUpdateFormula )
      updateLinearFormula();

    if ( mUseAbsoluteValue )
      value = std::fabs( value );

    if ( value > mMaximumValue )
    {
      if ( mIgnoreOutOfRange )
        return std::numeric_limits<double>::quiet_NaN();
      else
        return mMaximumWidth;
    }

    if ( value < mMinimumValue )
    {
      if ( mIgnoreOutOfRange )
        return std::numeric_limits<double>::quiet_NaN();
      else
        return mMinimumWidth;
    }

    return ( value - mMinimumValue ) * mLinearCoef + mMinimumWidth;
  }
  else
    return fixedStrokeWidth();
}

QDomElement QgsInterpolatedLineWidth::writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( context );

  QDomElement elem = doc.createElement( QStringLiteral( "mesh-stroke-width" ) );

  elem.setAttribute( QStringLiteral( "width-varying" ), mIsWidthVariable ? 1 : 0 );
  elem.setAttribute( QStringLiteral( "fixed-width" ), mFixedWidth );
  elem.setAttribute( QStringLiteral( "minimum-value" ), mMinimumValue );
  elem.setAttribute( QStringLiteral( "maximum-value" ), mMaximumValue );
  elem.setAttribute( QStringLiteral( "minimum-width" ), mMinimumWidth );
  elem.setAttribute( QStringLiteral( "maximum-width" ), mMaximumWidth );
  elem.setAttribute( QStringLiteral( "ignore-out-of-range" ), mIgnoreOutOfRange ? 1 : 0 );
  elem.setAttribute( QStringLiteral( "use-absolute-value" ), mUseAbsoluteValue ? 1 : 0 );

  return elem;
}

void QgsInterpolatedLineWidth::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  Q_UNUSED( context );

  mIsWidthVariable = elem.attribute( QStringLiteral( "width-varying" ) ).toInt();
  mFixedWidth = elem.attribute( QStringLiteral( "fixed-width" ) ).toDouble();
  mMinimumValue = elem.attribute( QStringLiteral( "minimum-value" ) ).toDouble();
  mMaximumValue = elem.attribute( QStringLiteral( "maximum-value" ) ).toDouble();
  mMinimumWidth = elem.attribute( QStringLiteral( "minimum-width" ) ).toDouble();
  mMaximumWidth = elem.attribute( QStringLiteral( "maximum-width" ) ).toDouble();
  mIgnoreOutOfRange = elem.attribute( QStringLiteral( "ignore-out-of-range" ) ).toInt();
  mUseAbsoluteValue = elem.attribute( QStringLiteral( "use-absolute-value" ) ).toInt();
}

bool QgsInterpolatedLineWidth::useAbsoluteValue() const
{
  return mUseAbsoluteValue;
}

void QgsInterpolatedLineWidth::setUseAbsoluteValue( bool useAbsoluteValue )
{
  mUseAbsoluteValue = useAbsoluteValue;
}

double QgsInterpolatedLineWidth::fixedStrokeWidth() const
{
  return mFixedWidth;
}

bool QgsInterpolatedLineWidth::ignoreOutOfRange() const
{
  return mIgnoreOutOfRange;
}

void QgsInterpolatedLineWidth::setIgnoreOutOfRange( bool ignoreOutOfRange )
{
  mIgnoreOutOfRange = ignoreOutOfRange;
}

bool QgsInterpolatedLineWidth::isVariableWidth() const
{
  return mIsWidthVariable;
}

void QgsInterpolatedLineWidth::setIsVariableWidth( bool isWidthVarying )
{
  mIsWidthVariable = isWidthVarying;
}

void QgsInterpolatedLineWidth::setFixedStrokeWidth( double fixedWidth )
{
  mFixedWidth = fixedWidth;
}

void QgsInterpolatedLineWidth::updateLinearFormula() const
{
  if ( mMaximumWidth - mMinimumWidth != 0 )
    mLinearCoef = ( mMaximumWidth - mMinimumWidth ) / ( mMaximumValue - mMinimumValue ) ;
  else
    mLinearCoef = 0;
  mNeedUpdateFormula = false;
}

QgsInterpolatedLineColor::QgsInterpolatedLineColor()
{
  mColorRampShader.setMinimumValue( std::numeric_limits<double>::quiet_NaN() );
  mColorRampShader.setMaximumValue( std::numeric_limits<double>::quiet_NaN() );
}

QgsInterpolatedLineColor::QgsInterpolatedLineColor( const QgsColorRampShader &colorRampShader )
{
  setColor( colorRampShader );
}

QgsInterpolatedLineColor::QgsInterpolatedLineColor( const QColor &color )
{
  setColor( color );
  mColoringMethod = SingleColor;
  mColorRampShader.setMinimumValue( std::numeric_limits<double>::quiet_NaN() );
  mColorRampShader.setMaximumValue( std::numeric_limits<double>::quiet_NaN() );
}

void QgsInterpolatedLineColor::setColor( const QgsColorRampShader &colorRampShader )
{
  mColorRampShader = colorRampShader;
  if ( ( mColorRampShader.sourceColorRamp() ) )
    mColoringMethod = ColorRamp;
  else
    mColoringMethod = SingleColor;
}

void QgsInterpolatedLineColor::setColor( const QColor &color )
{
  mColorRampShader = QgsColorRampShader();
  mSingleColor = color;
}

QColor QgsInterpolatedLineColor::color( double magnitude ) const
{
  if ( auto *lSourceColorRamp = mColorRampShader.sourceColorRamp() )
  {
    if ( mColorRampShader.isEmpty() )
      return lSourceColorRamp->color( 0 );

    int r, g, b, a;
    if ( mColorRampShader.shade( magnitude, &r, &g, &b, &a ) )
      return QColor( r, g, b, a );
    else
      return QColor( 0, 0, 0, 0 );
  }
  else
  {
    return mSingleColor;
  }
}

QgsInterpolatedLineColor::ColoringMethod QgsInterpolatedLineColor::coloringMethod() const
{
  return mColoringMethod;
}

QgsColorRampShader QgsInterpolatedLineColor::colorRampShader() const
{
  return mColorRampShader;
}

QDomElement QgsInterpolatedLineColor::writeXml( QDomDocument &doc, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( context );

  QDomElement elem = doc.createElement( QStringLiteral( "mesh-stroke-color" ) );

  elem.setAttribute( QStringLiteral( "single-color" ), QgsSymbolLayerUtils::encodeColor( mSingleColor ) );
  elem.setAttribute( QStringLiteral( "coloring-method" ), mColoringMethod );
  elem.appendChild( mColorRampShader.writeXml( doc ) );

  return elem;
}

void QgsInterpolatedLineColor::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  Q_UNUSED( context );

  QDomElement shaderElem = elem.firstChildElement( QStringLiteral( "colorrampshader" ) );
  mColorRampShader.readXml( shaderElem );

  mSingleColor = QgsSymbolLayerUtils::decodeColor( elem.attribute( QStringLiteral( "single-color" ) ) );
  mColoringMethod = static_cast<QgsInterpolatedLineColor::ColoringMethod>(
                      elem.attribute( QStringLiteral( "coloring-method" ) ).toInt() );
}

void QgsInterpolatedLineColor::graduatedColors( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients ) const
{
  breakValues.clear();
  breakColors.clear();
  gradients.clear();
  if ( mColoringMethod == SingleColor )
  {
    breakValues.append( value1 );
    breakColors.append( mSingleColor );
    breakValues.append( value2 );
    breakColors.append( mSingleColor );
    gradients.append( makeSimpleLinearGradient( mSingleColor, mSingleColor ) );
    return;
  }

  switch ( mColorRampShader.colorRampType() )
  {
    case QgsColorRampShader::Interpolated:
      graduatedColorsInterpolated( value1, value2, breakValues, breakColors, gradients );
      break;
    case QgsColorRampShader::Discrete:
      graduatedColorsDiscrete( value1, value2, breakValues, breakColors, gradients );
      break;
    case QgsColorRampShader::Exact:
      graduatedColorsExact( value1, value2, breakValues, breakColors, gradients );
      break;
  }

}

QLinearGradient QgsInterpolatedLineColor::makeSimpleLinearGradient( const QColor &color1, const QColor &color2 ) const
{
  QLinearGradient gradient;
  gradient.setColorAt( 0, color1 );
  gradient.setColorAt( 1, color2 );

  return gradient;
}

int QgsInterpolatedLineColor::itemColorIndexInf( double value ) const
{
  QList<QgsColorRampShader::ColorRampItem> itemList = mColorRampShader.colorRampItemList();

  if ( itemList.isEmpty() || itemList.first().value > value )
    return -1;

  if ( mColorRampShader.colorRampType() == QgsColorRampShader::Discrete )
    itemList.removeLast(); //remove the inf value

  if ( value > itemList.last().value )
    return itemList.count() - 1;

  int indSup = itemList.count() - 1;
  int indInf = 0;

  while ( true )
  {
    if ( abs( indSup - indInf ) <= 1 ) //always indSup>indInf, but abs to prevent infinity loop
      return indInf;

    int newInd = ( indInf + indSup ) / 2;

    if ( itemList.at( newInd ).value == std::numeric_limits<double>::quiet_NaN() )
      return -1;

    if ( itemList.at( newInd ).value <= value )
      indInf = newInd;
    else
      indSup = newInd;
  }
}

void QgsInterpolatedLineColor::graduatedColorsExact( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() == QgsColorRampShader::Exact );
  Q_ASSERT( breakValues.isEmpty() );
  Q_ASSERT( breakColors.isEmpty() );
  Q_ASSERT( gradients.isEmpty() );

  const QList<QgsColorRampShader::ColorRampItem> &itemList = mColorRampShader.colorRampItemList();
  if ( itemList.isEmpty() )
    return;

  int index = itemColorIndexInf( value1 );
  if ( index < 0 || !qgsDoubleNear( value1, itemList.at( index ).value ) )
    index++;

  if ( qgsDoubleNear( value1, value2 ) && qgsDoubleNear( value1, itemList.at( index ).value ) )
  {
    //the two value are the same and are equal to the value in the item list --> render only one color
    breakColors.append( itemList.at( index ).color );
    return;
  }

  while ( index < itemList.count() && itemList.at( index ).value <= value2 )
  {
    breakValues.append( itemList.at( index ).value );
    breakColors.append( itemList.at( index ).color );
    index++;
  }
}

void QgsInterpolatedLineColor::graduatedColorsInterpolated( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() == QgsColorRampShader::Interpolated );
  Q_ASSERT( breakValues.isEmpty() );
  Q_ASSERT( breakColors.isEmpty() );
  Q_ASSERT( gradients.isEmpty() );


  const QList<QgsColorRampShader::ColorRampItem> &itemList = mColorRampShader.colorRampItemList();
  if ( itemList.empty() )
    return;

  if ( itemList.count() == 1 )
  {
    breakColors.append( itemList.first().color );
    return;
  }

  if ( value2 <= itemList.first().value ) // completely out of range and less
  {
    if ( !mColorRampShader.clip() )
      breakColors.append( itemList.first().color ); // render only the first color in the whole range if not clipped
    return;
  }

  if ( value1 > itemList.last().value ) // completely out of range and greater
  {
    if ( !mColorRampShader.clip() )
      breakColors.append( itemList.last().color ); // render only the last color in the whole range if not clipped
    return;
  }

  if ( qgsDoubleNear( value1, value2 ) )
  {
    // the two values are the same
    //  --> render only one color
    int r, g, b, a;
    QColor color;
    if ( mColorRampShader.shade( value1, &r, &g, &b, &a ) )
      color = QColor( r, g, b, a );
    breakColors.append( color );
    return;
  }

  // index of the inf value of the interval where value1 is in the color ramp shader
  int index = itemColorIndexInf( value1 );
  if ( index < 0 ) // value1 out of range
  {
    QColor color = itemList.first().color;
    breakColors.append( color );
    if ( mColorRampShader.clip() ) // The first value/color returned is the first of the item list
      breakValues.append( itemList.first().value );
    else // The first value/color returned is the first color of the item list and value1
      breakValues.append( value1 );
  }
  else
  {
    // shade the color
    int r, g, b, a;
    QColor color;
    if ( mColorRampShader.shade( value1, &r, &g, &b, &a ) )
      color = QColor( r, g, b, a );
    breakValues.append( value1 );
    breakColors.append( color );
  }

  index++; // increment the index before go through the intervals

  while ( index <  itemList.count() && itemList.at( index ).value < value2 )
  {
    QColor color1 = breakColors.last();
    QColor color2 = itemList.at( index ).color;
    breakValues.append( itemList.at( index ).value );
    breakColors.append( color2 );
    gradients.append( makeSimpleLinearGradient( color1, color2 ) );
    index++;
  }

  // close the lists with value2 or last item if >value2
  QColor color1 = breakColors.last();
  QColor color2;
  if ( value2 < itemList.last().value )
  {
    int r, g, b, a;
    if ( mColorRampShader.shade( value2, &r, &g, &b, &a ) )
      color2 = QColor( r, g, b, a );
    breakValues.append( value2 );
  }
  else
  {
    color2 = itemList.last().color;
    if ( mColorRampShader.clip() )
      breakValues.append( itemList.last().value );
    else
      breakValues.append( value2 );
  }
  breakColors.append( color2 );
  gradients.append( makeSimpleLinearGradient( color1, color2 ) );
}


void QgsInterpolatedLineColor::graduatedColorsDiscrete( double value1, double value2, QList<double> &breakValues, QList<QColor> &breakColors, QList<QLinearGradient> &gradients ) const
{
  Q_ASSERT( mColorRampShader.colorRampType() == QgsColorRampShader::Discrete );
  Q_ASSERT( breakValues.isEmpty() );
  Q_ASSERT( breakColors.isEmpty() );
  Q_ASSERT( gradients.isEmpty() );

  const QList<QgsColorRampShader::ColorRampItem> &itemList = mColorRampShader.colorRampItemList();
  if ( itemList.empty() )
    return;

  if ( itemList.count() == 1 )
  {
    breakColors.append( itemList.first().color );
    return;
  }

  double lastValue = itemList.at( itemList.count() - 2 ).value;


  if ( value2 <= itemList.first().value ) // completely out of range and less
  {
    breakColors.append( itemList.first().color ); // render only the first color in the whole range
    return;
  }

  if ( value1 > lastValue ) // completely out of range and greater
  {
    breakColors.append( itemList.last().color ); // render only the last color in the whole range
    return;
  }

  // index of the inf value of the interval where value1 is in the color ramp shader
  int index = itemColorIndexInf( value1 );

  if ( qgsDoubleNear( value1, value2 ) )
  {
    // the two values are the same and are equal to the value in the item list
    //  --> render only one color, the sup one
    breakColors.append( itemList.at( index + 1 ).color );
    return;
  }

  if ( index < 0 ) // value1 out of range
  {
    breakValues.append( value1 );
    breakColors.append( itemList.first().color );
  }
  else // append the first value with corresponding color
  {
    QColor color = itemList.at( index ).color;
    breakValues.append( value1 );
    breakColors.append( color );
  }

  index++; // increment the index before go through the intervals

  while ( index < ( itemList.count() - 1 ) && itemList.at( index ).value < value2 )
  {
    QColor color = itemList.at( index ).color;
    breakValues.append( itemList.at( index ).value );
    breakColors.append( color );
    gradients.append( makeSimpleLinearGradient( color, color ) );
    index++;
  }

  // add value2 to close
  QColor lastColor = itemList.at( index ).color;
  breakColors.append( lastColor );
  breakValues.append( value2 );
  gradients.append( makeSimpleLinearGradient( lastColor, lastColor ) );

}


QgsInterpolatedLineFeatureRenderer::QgsInterpolatedLineFeatureRenderer(): QgsFeatureRenderer( QStringLiteral( "interpolatedLineRenderer" ) )
{
}

QgsInterpolatedLineFeatureRenderer *QgsInterpolatedLineFeatureRenderer::clone() const
{
  std::unique_ptr<QgsInterpolatedLineFeatureRenderer> newRenderer = std::make_unique<QgsInterpolatedLineFeatureRenderer>();
  newRenderer->mFirstExpressionString = mFirstExpressionString;
  newRenderer->mSecondExpressionString = mSecondExpressionString;

  newRenderer->mLineRender.mStrokeWidth = mLineRender.mStrokeWidth;
  newRenderer->mLineRender.mStrokeColoring = mLineRender.mStrokeColoring;
  newRenderer->mLineRender.mStrokeWidthUnit = mLineRender.mStrokeWidthUnit;

  return newRenderer.release();
}

QgsSymbol *QgsInterpolatedLineFeatureRenderer::symbolForFeature( const QgsFeature &, QgsRenderContext & ) const
{
  return nullptr;
}

QSet<QString> QgsInterpolatedLineFeatureRenderer::usedAttributes( const QgsRenderContext & ) const
{
  QSet<QString> attributes;

  // mFirstValueExpression and mSecondValueExpression can contain either attribute name or an expression.
  // Sometimes it is not possible to distinguish between those two,
  // e.g. "a - b" can be both a valid attribute name or expression.
  // Since we do not have access to fields here, try both options.
  attributes << mFirstExpressionString;
  attributes << mSecondExpressionString;

  QgsExpression testExprFirst( mFirstExpressionString );
  if ( !testExprFirst.hasParserError() )
    attributes.unite( testExprFirst.referencedColumns() );

  QgsExpression testExprSecond( mSecondExpressionString );
  if ( !testExprSecond.hasParserError() )
    attributes.unite( testExprSecond.referencedColumns() );

  return attributes;
}


QgsFeatureRenderer *QgsInterpolatedLineFeatureRenderer::create( QDomElement &element, const QgsReadWriteContext &context )
{
  std::unique_ptr<QgsInterpolatedLineFeatureRenderer> newRender = std::make_unique<QgsInterpolatedLineFeatureRenderer>();

  QDomElement strokeWidthElem = element.firstChildElement( QStringLiteral( "mesh-stroke-width" ) );
  if ( !strokeWidthElem.isNull() )
    newRender->mLineRender.mStrokeWidth.readXml( strokeWidthElem, context );

  QDomElement strokeColorElem = element.firstChildElement( QStringLiteral( "mesh-stroke-color" ) );
  if ( !strokeColorElem.isNull() )
    newRender->mLineRender.mStrokeColoring.readXml( strokeColorElem, context );

  newRender->mLineRender.mStrokeWidthUnit = static_cast<QgsUnitTypes::RenderUnit>( element.attribute( QStringLiteral( "width-unit" ) ).toInt() );

  newRender->mFirstExpressionString = element.attribute( QStringLiteral( "first-expression" ) );
  newRender->mSecondExpressionString = element.attribute( QStringLiteral( "second-expression" ) );

  return newRender.release();
}

QgsInterpolatedLineFeatureRenderer *QgsInterpolatedLineFeatureRenderer::convertFromRenderer( QgsFeatureRenderer *renderer )
{
  if ( renderer->type() == QStringLiteral( "interpolatedLineRenderer" ) )
    return static_cast<QgsInterpolatedLineFeatureRenderer *>( renderer )->clone();
  else
    return new QgsInterpolatedLineFeatureRenderer();
}

void QgsInterpolatedLineFeatureRenderer::setExpressionsString( QString first, QString second )
{
  mFirstExpressionString = first;
  mSecondExpressionString = second;
}

QString QgsInterpolatedLineFeatureRenderer::firstExpression() const
{
  return mFirstExpressionString;
}

QString QgsInterpolatedLineFeatureRenderer::secondExpression() const
{
  return mSecondExpressionString;
}

void QgsInterpolatedLineFeatureRenderer::setColorRampShader( const QgsColorRampShader &colorRamp )
{
  mLineRender.mStrokeColoring.setColor( colorRamp );
}

QgsColorRampShader QgsInterpolatedLineFeatureRenderer::colorRampShader() const
{
  return mLineRender.mStrokeColoring.colorRampShader();
}

void QgsInterpolatedLineFeatureRenderer::setWidthUnit( const QgsUnitTypes::RenderUnit &strokeWidthUnit )
{
  mLineRender.mStrokeWidthUnit = strokeWidthUnit;
}

void QgsInterpolatedLineFeatureRenderer::setInterpolatedWidth( const QgsInterpolatedLineWidth &interpolatedLineWidth )
{
  mLineRender.mStrokeWidth = interpolatedLineWidth;
}

bool QgsInterpolatedLineFeatureRenderer::prepareForRendering(
  const QgsFeature &feature,
  QgsRenderContext &context,
  QVector<QgsPolylineXY> &lineStrings,
  double &val1,
  double &val2,
  double &variationPerMapUnit ) const
{
  QgsGeometry geom = feature.geometry();

  if ( geom.isEmpty() )
    return false;

  switch ( QgsWkbTypes::flatType( geom.wkbType() ) )
  {
    case QgsWkbTypes::Unknown:
    case QgsWkbTypes::Point:
    case QgsWkbTypes::Polygon:
    case QgsWkbTypes::Triangle:
    case QgsWkbTypes::MultiPoint:
    case QgsWkbTypes::MultiPolygon:
    case QgsWkbTypes::GeometryCollection:
    case QgsWkbTypes::CurvePolygon:
    case QgsWkbTypes::MultiSurface:
    case QgsWkbTypes::NoGeometry:
      return false;
      break;
    case QgsWkbTypes::LineString:
    case QgsWkbTypes::CircularString:
    case QgsWkbTypes::CompoundCurve:
      lineStrings.append( geom.asPolyline() );
      break;
    case QgsWkbTypes::MultiCurve:
    case QgsWkbTypes::MultiLineString:
      lineStrings = geom.asMultiPolyline();
      break;
    default:
      return false;
      break;
  }

  QgsExpressionContext expressionContext = context.expressionContext();
  expressionContext.setFeature( feature );

  QVariant val1Variant;
  QVariant val2Variant;
  bool ok = true;

  if ( mFirstExpression )
  {
    val1Variant = mFirstExpression->evaluate( &expressionContext );
    ok |= mFirstExpression->hasEvalError();
  }
  else
    val1Variant = feature.attribute( mFirstAttributeIndex );

  if ( mSecondExpression )
  {
    val2Variant = mSecondExpression->evaluate( &expressionContext );
    ok |= mSecondExpression->hasEvalError();
  }
  else
    val2Variant = feature.attribute( mSecondAttributeIndex );

  if ( !ok )
    return false;

  val1 = val1Variant.toDouble( &ok );
  if ( !ok )
    return false;

  val2 = val2Variant.toDouble( &ok );
  if ( !ok )
    return false;

  double totalLength = geom.length();

  if ( totalLength == 0 )
    return false;

  variationPerMapUnit = ( val2 - val1 ) / totalLength;

  return true;
}


QDomElement QgsInterpolatedLineFeatureRenderer::save( QDomDocument &doc, const QgsReadWriteContext &context )
{
  QDomElement rendererElem = doc.createElement( RENDERER_TAG_NAME );
  rendererElem.setAttribute( QStringLiteral( "type" ), QStringLiteral( "interpolatedLineRenderer" ) );

  rendererElem.appendChild( mLineRender.mStrokeWidth.writeXml( doc, context ) );
  rendererElem.appendChild( mLineRender.mStrokeColoring.writeXml( doc, context ) );
  rendererElem.setAttribute( QStringLiteral( "width-unit" ), QString::number( mLineRender.mStrokeWidthUnit ) );
  rendererElem.setAttribute( QStringLiteral( "first-expression" ), mFirstExpressionString );
  rendererElem.setAttribute( QStringLiteral( "second-expression" ), mSecondExpressionString );

  return rendererElem;
}

void QgsInterpolatedLineFeatureRenderer::startRender( QgsRenderContext &context, const QgsFields &fields )
{
  QgsFeatureRenderer::startRender( context, fields );

  // find out classification attribute index from name
  mFirstAttributeIndex = fields.lookupField( mFirstExpressionString );
  mSecondAttributeIndex = fields.lookupField( mSecondExpressionString );
  if ( mFirstAttributeIndex == -1 )
  {
    mFirstExpression.reset( new QgsExpression( mFirstExpressionString ) );
    mFirstExpression->prepare( &context.expressionContext() );
  }
  if ( mFirstAttributeIndex == -1 )
  {
    mSecondExpression.reset( new QgsExpression( mSecondExpressionString ) );
    mSecondExpression->prepare( &context.expressionContext() );
  }
}

void QgsInterpolatedLineFeatureRenderer::stopRender( QgsRenderContext &context )
{
  QgsFeatureRenderer::stopRender( context );
  mFirstExpression.reset();
  mSecondExpression.reset();
}

QList<QgsLayerTreeModelLegendNode *> QgsInterpolatedLineFeatureRenderer::extraLegendNodes( QgsLayerTreeLayer *nodeLayer ) const
{

  const QgsColorRampShader rampShader = mLineRender.mStrokeColoring.colorRampShader();

  QList<QgsLayerTreeModelLegendNode *> res;

  switch ( rampShader.colorRampType() )
  {
    case QgsColorRampShader::Interpolated:
      if ( !rampShader.colorRampItemList().isEmpty() )
      {
        res << new QgsColorRampLegendNode( nodeLayer, rampShader.createColorRamp(),
                                           rampShader.legendSettings() ? *rampShader.legendSettings() : QgsColorRampLegendNodeSettings(),
                                           rampShader.minimumValue(), rampShader.maximumValue() );
      }
      break;
    case QgsColorRampShader::Discrete:
    case QgsColorRampShader::Exact:
    {
      // for all others we use itemised lists
      QList< QPair< QString, QColor > > items;
      rampShader.legendSymbologyItems( items );
      res.reserve( items.size() );
      for ( const QPair< QString, QColor > &item : std::as_const( items ) )
        res << new QgsRasterSymbolLegendNode( nodeLayer, item.second, item.first );
      break;
    }
  }

  return res;
}

bool QgsInterpolatedLineFeatureRenderer::willRenderFeature( const QgsFeature &feature, QgsRenderContext &context ) const
{
  QVector<QgsPolylineXY> lineStrings;

  double val1;
  double val2;
  double variationPerMapUnit;
  return prepareForRendering( feature, context, lineStrings, val1, val2, variationPerMapUnit );
}

bool QgsInterpolatedLineFeatureRenderer::renderFeature( const QgsFeature &feature, QgsRenderContext &context, int, bool selected, bool )
{
  QVector<QgsPolylineXY> lineStrings;

  double val1;
  double val2;
  double variationPerMapUnit;

  if ( !prepareForRendering( feature, context, lineStrings, val1, val2, variationPerMapUnit ) )
    return false;

  mLineRender.setSelected( selected );

  for ( const QgsPolylineXY &poly : std::as_const( lineStrings ) )
  {
    double lengthFromStart = 0;
    for ( int i = 1; i < poly.count(); ++i )
    {
      QgsPointXY p1 = poly.at( i - 1 );
      QgsPointXY p2 = poly.at( i );

      double v1 = val1 + variationPerMapUnit * lengthFromStart;
      lengthFromStart += p1.distance( p2 );
      double v2 = val1 + variationPerMapUnit * lengthFromStart;
      mLineRender.render( v1, v2, p1, p2, context );
    }
  }

  return true;

}
