/***************************************************************************
  qgsmesh3dsymbol.cpp
  -------------------
  Date                 : January 2019
  Copyright            : (C) 2019 by Peter Petrik
  Email                : zilolv at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmesh3dsymbol.h"
#include "qgssymbollayerutils.h"
#include "qgs3dutils.h"

QgsAbstract3DSymbol *QgsMesh3DSymbol::clone() const
{
  return new QgsMesh3DSymbol( *this );
}

void QgsMesh3DSymbol::writeXml( QDomElement &elem, const QgsReadWriteContext &context ) const
{
  Q_UNUSED( context )

  QDomDocument doc = elem.ownerDocument();

  elem.setAttribute( QStringLiteral( "renderer-type" ), mRenderType );

  //Simple symbol
  QDomElement elemDataProperties = doc.createElement( QStringLiteral( "data" ) );
  elemDataProperties.setAttribute( QStringLiteral( "alt-clamping" ), Qgs3DUtils::altClampingToString( mAltClamping ) );
  elemDataProperties.setAttribute( QStringLiteral( "height" ), mHeight );
  elemDataProperties.setAttribute( QStringLiteral( "add-back-faces" ), mAddBackFaces ? QStringLiteral( "1" ) : QStringLiteral( "0" ) );
  elem.appendChild( elemDataProperties );

  QDomElement elemMaterial = doc.createElement( QStringLiteral( "material" ) );
  mMaterial.writeXml( elemMaterial );
  elem.appendChild( elemMaterial );

  //Advanced symbol
  QDomElement elemAdvancedSettings = doc.createElement( QStringLiteral( "advanced-settings" ) );
  elemAdvancedSettings.setAttribute( QStringLiteral( "smoothed-triangle" ), mSmoothedTriangles ? QStringLiteral( "1" ) : QStringLiteral( "0" ) );
  elemAdvancedSettings.setAttribute( QStringLiteral( "wireframe-enabled" ), mWireframeEnabled ? QStringLiteral( "1" ) : QStringLiteral( "0" ) );
  elemAdvancedSettings.setAttribute( QStringLiteral( "wireframe-line-width" ), mWireframeLineWidth );
  elemAdvancedSettings.setAttribute( QStringLiteral( "wireframe-line-color" ), QgsSymbolLayerUtils::encodeColor( mWireframeLineColor ) );
  elemAdvancedSettings.setAttribute( QStringLiteral( "verticale-scale" ), mVerticaleScale );
  elemAdvancedSettings.setAttribute( QStringLiteral( "texture-type" ), mTextureType );
  elemAdvancedSettings.appendChild( mColorRampShader.writeXml( doc ) );
  elemAdvancedSettings.setAttribute( QStringLiteral( "texture-unique-color" ), QgsSymbolLayerUtils::encodeColor( mUniqueColor ) );
  elem.appendChild( elemAdvancedSettings );

  QDomElement elemDDP = doc.createElement( QStringLiteral( "data-defined-properties" ) );
  mDataDefinedProperties.writeXml( elemDDP, propertyDefinitions() );
  elem.appendChild( elemDDP );
}

void QgsMesh3DSymbol::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  Q_UNUSED( context )

  //Simple symbol
  QDomElement elemDataProperties = elem.firstChildElement( QStringLiteral( "data" ) );
  mAltClamping = Qgs3DUtils::altClampingFromString( elemDataProperties.attribute( QStringLiteral( "alt-clamping" ) ) );
  mHeight = elemDataProperties.attribute( QStringLiteral( "height" ) ).toFloat();
  mAddBackFaces = elemDataProperties.attribute( QStringLiteral( "add-back-faces" ) ).toInt();

  QDomElement elemMaterial = elem.firstChildElement( QStringLiteral( "material" ) );
  mMaterial.readXml( elemMaterial );

  //Advanced symbol
  QDomElement elemAdvancedSettings = elem.firstChildElement( QStringLiteral( "advanced-settings" ) );
  mSmoothedTriangles = elemAdvancedSettings.attribute( QStringLiteral( "smoothed-triangle" ) ).toInt();
  mWireframeEnabled = elemAdvancedSettings.attribute( QStringLiteral( "wireframe-enabled" ) ).toInt();
  mWireframeLineWidth = elemAdvancedSettings.attribute( QStringLiteral( "wireframe-line-width" ) ).toDouble();
  mWireframeLineColor = QgsSymbolLayerUtils::decodeColor( elemAdvancedSettings.attribute( QStringLiteral( "wireframe-line-color" ) ) );
  mVerticaleScale = elemAdvancedSettings.attribute( "verticale-scale" ).toDouble();
  mTextureType = static_cast<QgsMesh3DSymbol::TextureType>( elemAdvancedSettings.attribute( QStringLiteral( "texture-type" ) ).toInt() );
  mColorRampShader.readXml( elemAdvancedSettings.firstChildElement( "colorrampshader" ) );
  mUniqueColor = QgsSymbolLayerUtils::decodeColor( elemAdvancedSettings.attribute( QStringLiteral( "texture-unique-color" ) ) );

  QDomElement elemDDP = elem.firstChildElement( QStringLiteral( "data-defined-properties" ) );
  if ( !elemDDP.isNull() )
    mDataDefinedProperties.readXml( elemDDP, propertyDefinitions() );
}

bool QgsMesh3DSymbol::smoothedTriangles() const
{
  return mSmoothedTriangles;
}

void QgsMesh3DSymbol::setSmoothedTriangles( bool smoothTriangles )
{
  mSmoothedTriangles = smoothTriangles;
}

bool QgsMesh3DSymbol::wireframeEnabled() const
{
  return mWireframeEnabled;
}

void QgsMesh3DSymbol::setWireframeEnabled( bool wireframeEnabled )
{
  mWireframeEnabled = wireframeEnabled;
}

double QgsMesh3DSymbol::wireframeLineWidth() const
{
  return mWireframeLineWidth;
}

void QgsMesh3DSymbol::setWireframeLineWidth( double wireframeLineWidth )
{
  mWireframeLineWidth = wireframeLineWidth;
}

QColor QgsMesh3DSymbol::wireframeLineColor() const
{
  return mWireframeLineColor;
}

void QgsMesh3DSymbol::setWireframeLineColor( const QColor &wireframeLineColor )
{
  mWireframeLineColor = wireframeLineColor;
}

double QgsMesh3DSymbol::verticaleScale() const
{
  return mVerticaleScale;
}

void QgsMesh3DSymbol::setVerticaleScale( double verticaleScale )
{
  mVerticaleScale = verticaleScale;
}

QgsColorRampShader QgsMesh3DSymbol::colorRampShader() const
{
  return mColorRampShader;
}

void QgsMesh3DSymbol::setColorRampShader( const QgsColorRampShader &colorRampShader )
{
  mColorRampShader = colorRampShader;
}

QColor QgsMesh3DSymbol::uniqueMeshColor() const
{
  return mUniqueColor;
}

void QgsMesh3DSymbol::setUniqueMeshColor( const QColor &color )
{
  mUniqueColor = color;
}

QgsMesh3DSymbol::TextureType QgsMesh3DSymbol::meshTextureType() const
{
  return mTextureType;
}

void QgsMesh3DSymbol::setMeshTextureType( const QgsMesh3DSymbol::TextureType &coloringType )
{
  mTextureType = coloringType;
}

QgsMesh3DSymbol::RendererType QgsMesh3DSymbol::renderType() const
{
  return mRenderType;
}

void QgsMesh3DSymbol::setRendererType( const QgsMesh3DSymbol::RendererType &renderType )
{
  mRenderType = renderType;
}
