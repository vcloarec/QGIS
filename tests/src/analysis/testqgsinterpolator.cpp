/***************************************************************************
  testqgsinterpolator.cpp
  -----------------------
Date                 : November 2017
Copyright            : (C) 2017 by Nyall Dawson
Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgstest.h"

#include "qgsapplication.h"
#include "qgsdualedgetriangulation.h"

class TestQgsInterpolator : public QObject
{
    Q_OBJECT

  public:

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() ;// will be called before each testfunction is executed.
    void cleanup() ;// will be called after every testfunction.
    void dualEdge();
    void dualEdgeEditing();

  private:
};

void  TestQgsInterpolator::initTestCase()
{
  //
  // Runs once before any tests are run
  //
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestQgsInterpolator::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsInterpolator::init()
{}

void TestQgsInterpolator::cleanup()
{}

void TestQgsInterpolator::dualEdge()
{
  QgsDualEdgeTriangulation tri;
  QVERIFY( !tri.point( 0 ) );
  QVERIFY( !tri.point( 1 ) );
  QCOMPARE( tri.pointsCount(), 0 );
  QCOMPARE( tri.dimension(), -1 );

  tri.addPoint( QgsPoint( 1, 2, 3 ) );
  QCOMPARE( *tri.point( 0 ), QgsPoint( 1, 2, 3 ) );
  QCOMPARE( tri.pointsCount(), 1 );
  QCOMPARE( tri.dimension(), 0 );

  tri.addPoint( QgsPoint( 3, 0, 4 ) );
  QCOMPARE( *tri.point( 1 ), QgsPoint( 3, 0, 4 ) );
  QCOMPARE( tri.pointsCount(), 2 );
  QCOMPARE( tri.dimension(), 1 );

  tri.addPoint( QgsPoint( 4, 4, 5 ) );
  QCOMPARE( *tri.point( 2 ), QgsPoint( 4, 4, 5 ) );
  QCOMPARE( tri.pointsCount(), 3 );
  QCOMPARE( tri.dimension(), 2 );

  QgsPoint p1( 0, 0, 0 );
  QgsPoint p2( 0, 0, 0 );
  QgsPoint p3( 0, 0, 0 );
  int n1 = 0;
  int n2 = 0;
  int n3 = 0;
  QVERIFY( !tri.pointInside( 0, 1 ) );
  QVERIFY( !tri.triangleVertices( 0, 1, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 0, 1, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( !tri.pointInside( 1, 1 ) );
  QVERIFY( !tri.triangleVertices( 1, 1, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 1, 1, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( !tri.pointInside( 4, 1 ) );
  QVERIFY( !tri.triangleVertices( 4, 1, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 4, 1, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( !tri.pointInside( 2, 4 ) );
  QVERIFY( !tri.triangleVertices( 2, 4, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 2, 4, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( !tri.pointInside( 3, -1 ) );
  QVERIFY( !tri.triangleVertices( 3, -1, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 3, -1, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( tri.pointInside( 2, 2 ) );
  QVERIFY( tri.triangleVertices( 2, 2, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QVERIFY( tri.triangleVertices( 2, 2, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( n1, 0 );
  QCOMPARE( n2, 1 );
  QCOMPARE( n3, 2 );
  QVERIFY( tri.pointInside( 3, 1 ) );
  QVERIFY( tri.triangleVertices( 3, 1, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QVERIFY( tri.triangleVertices( 3, 1, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( n1, 0 );
  QCOMPARE( n2, 1 );
  QCOMPARE( n3, 2 );
  QVERIFY( tri.pointInside( 3.5, 3.5 ) );
  QVERIFY( tri.triangleVertices( 3.5, 3.5, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QVERIFY( tri.triangleVertices( 3.5, 3.5, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( n1, 0 );
  QCOMPARE( n2, 1 );
  QCOMPARE( n3, 2 );

  QCOMPARE( tri.oppositePoint( 0, 1 ), -1 );
  QCOMPARE( tri.oppositePoint( 0, 2 ), 1 );
  QCOMPARE( tri.oppositePoint( 1, 0 ), 2 );
  QCOMPARE( tri.oppositePoint( 1, 2 ), -1 );
  QCOMPARE( tri.oppositePoint( 2, 0 ), -1 );
  QCOMPARE( tri.oppositePoint( 2, 1 ), 0 );

  // add another point
  tri.addPoint( QgsPoint( 2, 4, 6 ) );
  QCOMPARE( *tri.point( 3 ), QgsPoint( 2, 4, 6 ) );
  QCOMPARE( tri.pointsCount(), 4 );
  QVERIFY( !tri.pointInside( 2, 4.5 ) );
  QVERIFY( !tri.triangleVertices( 2, 4.5, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 2, 4.5, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( !tri.pointInside( 1, 4 ) );
  QVERIFY( !tri.triangleVertices( 1, 4, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 1, 4, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( tri.pointInside( 2, 3.5 ) );
  QVERIFY( tri.triangleVertices( 2, 3.5, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( p3, QgsPoint( 2, 4, 6 ) );
  QVERIFY( tri.triangleVertices( 2, 3.5, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( p3, QgsPoint( 2, 4, 6 ) );
  QCOMPARE( n1, 0 );
  QCOMPARE( n2, 2 );
  QCOMPARE( n3, 3 );
  QVERIFY( tri.triangleVertices( 2, 2, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( n1, 0 );
  QCOMPARE( n2, 1 );
  QCOMPARE( n3, 2 );

  QCOMPARE( tri.oppositePoint( 0, 1 ), -1 );
  QCOMPARE( tri.oppositePoint( 0, 2 ), 1 );
  QCOMPARE( tri.oppositePoint( 0, 3 ), 2 );
  QCOMPARE( tri.oppositePoint( 1, 0 ), 2 );
  QCOMPARE( tri.oppositePoint( 1, 2 ), -1 );
  QCOMPARE( tri.oppositePoint( 1, 3 ), -10 );
  QCOMPARE( tri.oppositePoint( 2, 0 ), 3 );
  QCOMPARE( tri.oppositePoint( 2, 1 ), 0 );
  QCOMPARE( tri.oppositePoint( 2, 3 ), -1 );
  QCOMPARE( tri.oppositePoint( 3, 0 ), -1 );
  QCOMPARE( tri.oppositePoint( 3, 1 ), -10 );
  QCOMPARE( tri.oppositePoint( 3, 2 ), 0 );


  // add another point
  tri.addPoint( QgsPoint( 2, 2, 7 ) );
  QCOMPARE( *tri.point( 4 ), QgsPoint( 2, 2, 7 ) );
  QCOMPARE( tri.pointsCount(), 5 );
  QVERIFY( !tri.pointInside( 2, 4.5 ) );
  QVERIFY( !tri.triangleVertices( 2, 4.5, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 2, 4.5, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( !tri.pointInside( 1, 4 ) );
  QVERIFY( !tri.triangleVertices( 1, 4, p1, p2, p3 ) );
  QVERIFY( !tri.triangleVertices( 1, 4, p1, n1, p2, n2, p3, n3 ) );
  QVERIFY( tri.pointInside( 2, 3.5 ) );
  QVERIFY( tri.triangleVertices( 2, 3.5, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 2, 4, 6 ) );
  QCOMPARE( p2, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p3, QgsPoint( 2, 2, 7 ) );
  QVERIFY( tri.triangleVertices( 2, 3.5, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 2, 4, 6 ) );
  QCOMPARE( p2, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p3, QgsPoint( 2, 2, 7 ) );
  QCOMPARE( n1, 3 );
  QCOMPARE( n2, 0 );
  QCOMPARE( n3, 4 );
  QVERIFY( tri.pointInside( 2, 1.5 ) );
  QVERIFY( tri.triangleVertices( 2, 1.5, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 2, 2, 7 ) );
  QVERIFY( tri.triangleVertices( 2, 1.5, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 1, 2, 3 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 2, 2, 7 ) );
  QCOMPARE( n1, 0 );
  QCOMPARE( n2, 1 );
  QCOMPARE( n3, 4 );
  QVERIFY( tri.pointInside( 3.1, 1 ) );
  QVERIFY( tri.triangleVertices( 3.1, 1, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 2, 2, 7 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QVERIFY( tri.triangleVertices( 3.1, 1, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 2, 2, 7 ) );
  QCOMPARE( p2, QgsPoint( 3, 0, 4 ) );
  QCOMPARE( p3, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( n1, 4 );
  QCOMPARE( n2, 1 );
  QCOMPARE( n3, 2 );
  QVERIFY( tri.pointInside( 2.5, 3.5 ) );
  QVERIFY( tri.triangleVertices( 2.5, 3.5, p1, p2, p3 ) );
  QCOMPARE( p1, QgsPoint( 2, 2, 7 ) );
  QCOMPARE( p2, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( p3, QgsPoint( 2, 4, 6 ) );
  QVERIFY( tri.triangleVertices( 2.5, 3.5, p1, n1, p2, n2, p3, n3 ) );
  QCOMPARE( p1, QgsPoint( 2, 2, 7 ) );
  QCOMPARE( p2, QgsPoint( 4, 4, 5 ) );
  QCOMPARE( p3, QgsPoint( 2, 4, 6 ) );
  QCOMPARE( n1, 4 );
  QCOMPARE( n2, 2 );
  QCOMPARE( n3, 3 );

  QCOMPARE( tri.oppositePoint( 0, 1 ), -1 );
  QCOMPARE( tri.oppositePoint( 0, 2 ), -10 );
  QCOMPARE( tri.oppositePoint( 0, 3 ), 4 );
  QCOMPARE( tri.oppositePoint( 0, 4 ), 1 );
  QCOMPARE( tri.oppositePoint( 1, 0 ), 4 );
  QCOMPARE( tri.oppositePoint( 1, 2 ), -1 );
  QCOMPARE( tri.oppositePoint( 1, 3 ), -10 );
  QCOMPARE( tri.oppositePoint( 1, 4 ), 2 );
  QCOMPARE( tri.oppositePoint( 2, 0 ), -10 );
  QCOMPARE( tri.oppositePoint( 2, 1 ), 4 );
  QCOMPARE( tri.oppositePoint( 2, 3 ), -1 );
  QCOMPARE( tri.oppositePoint( 2, 4 ), 3 );
  QCOMPARE( tri.oppositePoint( 3, 0 ), -1 );
  QCOMPARE( tri.oppositePoint( 3, 1 ), -10 );
  QCOMPARE( tri.oppositePoint( 3, 2 ), 4 );
  QCOMPARE( tri.oppositePoint( 3, 4 ), 0 );
  QCOMPARE( tri.oppositePoint( 4, 0 ), 3 );
  QCOMPARE( tri.oppositePoint( 4, 1 ), 0 );
  QCOMPARE( tri.oppositePoint( 4, 2 ), 1 );
  QCOMPARE( tri.oppositePoint( 4, 3 ), 2 );

//  QVERIFY( tri.getSurroundingTriangles( 0 ).empty() );

  QgsMesh mesh = tri.triangulationToMesh();
  QCOMPARE( mesh.faceCount(), 4 );
  QCOMPARE( mesh.vertexCount(), 5 );

  QCOMPARE( mesh.vertex( 0 ), QgsMeshVertex( 1.0, 2.0, 3.0 ) );
  QCOMPARE( mesh.vertex( 1 ), QgsMeshVertex( 3.0, 0.0, 4.0 ) );
  QCOMPARE( mesh.vertex( 2 ), QgsMeshVertex( 4.0, 4.0, 5.0 ) );
  QCOMPARE( mesh.vertex( 3 ), QgsMeshVertex( 2.0, 4.0, 6.0 ) );
  QCOMPARE( mesh.vertex( 4 ), QgsMeshVertex( 2.0, 2.0, 7.0 ) );

  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {0, 4, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {4, 2, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {1, 4, 0} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {2, 4, 1} ) ) );

}

void TestQgsInterpolator::dualEdgeEditing()
{
  QgsDualEdgeTriangulation tri;
  tri.startEditMode();
  QgsRectangle changedExtent;
  QgsRectangle minimalExtent;
  minimalExtent.setMinimal();

  QgsMesh mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( mesh.vertices.size(), 0 );
  QCOMPARE( mesh.faces.size(), 0 );
  QCOMPARE( tri.dimension(), -1 );
  QCOMPARE( changedExtent, minimalExtent );

  tri.addPoint( QgsPoint( 0, 0, 0 ) );
  QCOMPARE( tri.dimension(), 0 );
  tri.addPoint( QgsPoint( 2, 0, 0 ) );
  QCOMPARE( tri.dimension(), 1 );
  tri.addPoint( QgsPoint( 2, 2, 0 ) );
  QCOMPARE( tri.dimension(), 2 );
  tri.addPoint( QgsPoint( 0, 2, 0 ) );
  QCOMPARE( tri.dimension(), 2 );

  QCOMPARE( tri.pointsCount(), 4 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 2, 2 ) );
  QCOMPARE( mesh.faces.count(), 2 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {0, 1, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {0, 2, 3} ) ) );

  int lastPoint = tri.addPoint( QgsPoint( 1, 1, 0 ) );
  QCOMPARE( tri.pointsCount(), 5 );
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 2, 2 ) );
  QCOMPARE( mesh.faces.count(), 4 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {4, 1, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {1, 4, 0} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {3, 4, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {0, 4, 3} ) ) );

  //! Try without changing anything
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, minimalExtent );
  QCOMPARE( mesh.faces.count(), 4 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {4, 1, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {1, 4, 0} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {3, 4, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {0, 4, 3} ) ) );

  tri.removePoint( lastPoint );
  QCOMPARE( tri.pointsCount(), 4 ); //non last point that is removed still presents but nullptr when editing, here it is the last point
  QVERIFY( !tri.point( lastPoint ) );
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 2, 2 ) );
  QCOMPARE( mesh.faces.count(), 4 ); //all faces still present but removed
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {0, 2, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {0, 1, 2} ) ) );

  tri.removePoint( 0 );
  QCOMPARE( tri.pointsCount(), 4 ); //non last point that is removed still presents but nullptr when editing, here it is not the last point
  QVERIFY( !tri.point( 0 ) );
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 2, 2 ) );
  QCOMPARE( mesh.faces.count(), 4 ); //all faces still present but removed
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {1, 2, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );

  tri.addPoint( QgsPoint( -0.5, -0.5, 0 ) );
  QCOMPARE( tri.pointsCount(), 4 ); //new point replace a old one that is null
  QVERIFY( tri.point( 0 ) );
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( -0.5, -0.5, 2, 2 ) );
  QCOMPARE( mesh.faces.count(), 4 ); //all faces still present but removed
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {1, 2, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {1, 3, 0} ) ) );

  tri.removePoint( 0 );
  QCOMPARE( tri.pointsCount(), 4 ); //non last point that is removed still presents but nullptr when editing
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( mesh.faces.count(), 4 ); //all faces still present but removed
  QCOMPARE( changedExtent, QgsRectangle( -0.5, -0.5, 2, 2 ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {1, 2, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );

  tri.removePoint( 3 );
  QCOMPARE( tri.pointsCount(), 3 ); //non last point that is removed still presents but nullptr when editing, here it is not the last point
  QVERIFY( !tri.point( 3 ) );
  QCOMPARE( tri.dimension(), 1 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 2, 2 ) );
  QCOMPARE( mesh.faces.count(), 4 ); //all faces still present but removed
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );

  tri.removePoint( 2 );
  QCOMPARE( tri.pointsCount(), 2 );
  QVERIFY( !tri.point( 2 ) );
  QCOMPARE( tri.dimension(),  0 );

  tri.removePoint( 1 );
  QCOMPARE( tri.pointsCount(), 0 );
  QVERIFY( !tri.point( 1 ) );
  QCOMPARE( tri.dimension(),  -1 );

  mesh = tri.editedTriangulationToMesh( changedExtent );

  QCOMPARE( changedExtent, QgsRectangle( 2, 0, 2, 2 ) );
  QCOMPARE( mesh.faces.count(), 4 ); //faces are still presents but void
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );
  tri.endEditMode();
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( mesh.faces.count(), 0 );

  tri.startEditMode();
  // with colineear points
  tri.addPoint( QgsPoint( 1, 1 ) );
  tri.addPoint( QgsPoint( 1, 2 ) );
  tri.addPoint( QgsPoint( 1, 3 ) );
  tri.addPoint( QgsPoint( 2, 1 ) );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  2 );

  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 1, 1, 2, 3 ) );
  QCOMPARE( mesh.faces.count(), 2 );

  tri.removePoint( 3 );
  QCOMPARE( tri.pointsCount(), 3 );
  QCOMPARE( tri.dimension(),  1 );
  tri.removePoint( 2 );
  QCOMPARE( tri.pointsCount(), 2 );
  QCOMPARE( tri.dimension(),  1 );
  tri.removePoint( 1 );
  QCOMPARE( tri.pointsCount(), 1 );
  QCOMPARE( tri.dimension(),  0 );
  tri.removePoint( 0 );
  QCOMPARE( tri.pointsCount(), 0 );
  QCOMPARE( tri.dimension(),  -1 );

  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 1, 1, 2, 3 ) );

  // again with colineear points butr remove in other order
  tri.addPoint( QgsPoint( 1, 1 ) );
  tri.addPoint( QgsPoint( 1, 2 ) );
  tri.addPoint( QgsPoint( 1, 3 ) );
  tri.addPoint( QgsPoint( 2, 1 ) );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  2 );

  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 1, 1, 2, 3 ) );
  QCOMPARE( mesh.faces.count(), 2 );

  tri.removePoint( 2 );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  2 );
  tri.removePoint( 1 );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  1 );
  tri.removePoint( 3 );
  QCOMPARE( tri.pointsCount(), 1 );
  QCOMPARE( tri.dimension(),  0 );
  tri.removePoint( 0 );
  QCOMPARE( tri.pointsCount(), 0 );
  QCOMPARE( tri.dimension(),  -1 );
  tri.endEditMode();

  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 1, 1, 2, 3 ) );
  QCOMPARE( mesh.faces.count(), 0 );

  //more complex editing
  tri.startEditMode();
  tri.addPoint( QgsPoint( 0, 1 ) );
  tri.addPoint( QgsPoint( 1, 0 ) );
  tri.addPoint( QgsPoint( 2, 0.5 ) );
  tri.addPoint( QgsPoint( 3, 0 ) );
  tri.addPoint( QgsPoint( 4, 1 ) );
  tri.addPoint( QgsPoint( 5, 0.5 ) );
  tri.addPoint( QgsPoint( 6, 1 ) );
  tri.addPoint( QgsPoint( 3, 3 ) );

  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 6, 3 ) );

  tri.swapEdge( 1, 0.75 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 3, 3 ) );
  tri.swapEdge( 3, 0.75 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 2, 0, 4, 3 ) );
  tri.swapEdge( 5, 0.9 );
  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 3, 0.5, 6, 3 ) );

  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {1, 2, 7} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {3, 2, 1} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {7, 2, 3 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {3, 4, 7 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( {5, 4, 3 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( {5, 6, 7 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( {7, 0, 1 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( {7, 4, 5 } ) ) );

  tri.removePoint( 7 );

  mesh = tri.editedTriangulationToMesh( changedExtent );
  QCOMPARE( changedExtent, QgsRectangle( 0, 0, 6, 3 ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {3, 2, 1} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {4, 5, 6 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {2, 3, 4 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( {5, 4, 3 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( {0, 2, 4 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( {0, 1, 2 } ) ) );

  tri.endEditMode();
}


QGSTEST_MAIN( TestQgsInterpolator )
#include "testqgsinterpolator.moc"
