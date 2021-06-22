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
#include "qgstininterpolator.h"
#include "qgsidwinterpolator.h"
#include "qgsvectorlayer.h"

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

    void TIN_IDW_Interpolator_with_attribute();
    void TIN_IDW_Interpolator_with_Z();

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

  QSet<int> changedVerticesIndex;


  QgsMesh mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( mesh.vertices.size(), 0 );
  QCOMPARE( mesh.faces.size(), 0 );
  QCOMPARE( tri.dimension(), -1 );
  QCOMPARE( changedVerticesIndex, QSet<int>() );

  tri.addPoint( QgsPoint( 0, 0, 0 ) );
  QCOMPARE( tri.dimension(), 0 );
  tri.addPoint( QgsPoint( 2, 0, 0 ) );
  QCOMPARE( tri.dimension(), 1 );
  tri.addPoint( QgsPoint( 2, 2, 0 ) );
  QCOMPARE( tri.dimension(), 2 );
  tri.addPoint( QgsPoint( 0, 2, 0 ) );
  QCOMPARE( tri.dimension(), 2 );

  QCOMPARE( tri.pointsCount(), 4 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 3} ) );
  QCOMPARE( mesh.faces.count(), 2 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {0, 1, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {0, 2, 3} ) ) );

  int lastPoint = tri.addPoint( QgsPoint( 1, 1, 0 ) );
  QCOMPARE( tri.pointsCount(), 5 );
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 3, 4} ) );
  QCOMPARE( mesh.faces.count(), 6 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {1, 4, 0} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {4, 1, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( {3, 4, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( {0, 4, 3} ) ) );

  //! Try without changing anything
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>() );
  QCOMPARE( mesh.faces.count(), 6 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {1, 4, 0} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {4, 1, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( {3, 4, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( {0, 4, 3} ) ) );

  tri.removePoint( lastPoint );
  QCOMPARE( tri.pointsCount(), 5 ); //in editing mode, removing points are still present but not connected anymore
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 3, 4} ) );
  QCOMPARE( mesh.faces.count(), 8 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( {0, 1, 2} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( {0, 2, 3} ) ) );

  tri.removePoint( 0 );
  QCOMPARE( tri.pointsCount(), 5 );
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex,  QSet<int>( {0, 1, 2, 3} ) );
  QCOMPARE( mesh.faces.count(), 9 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 8 ), QgsMeshFace( {1, 2, 3} ) ) );

  tri.addPoint( QgsPoint( -0.5, -0.5, 0 ) );
  QCOMPARE( tri.pointsCount(), 6 );
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex,  QSet<int>( { 1, 3, 5} ) );
  QCOMPARE( mesh.faces.count(), 10 );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 8 ), QgsMeshFace( {1, 2, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 9 ), QgsMeshFace( {1, 3, 5} ) ) );

  tri.removePoint( 5 );
  QCOMPARE( tri.pointsCount(), 6 ); //non last point that is removed still presents but nullptr when editing
  QCOMPARE( tri.dimension(), 2 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( mesh.faces.count(), 10 );
  QCOMPARE( changedVerticesIndex, QSet<int>( { 1, 3, 5} ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 8 ), QgsMeshFace( {1, 2, 3} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 9 ), QgsMeshFace( ) ) );

  tri.removePoint( 3 );
  QCOMPARE( tri.pointsCount(), 6 ); //non last point that is removed still presents but nullptr when editing, here it is not the last point
  QCOMPARE( tri.dimension(), 1 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex,  QSet<int>( { 1, 2, 3} ) );
  QCOMPARE( mesh.faces.count(), 10 ); //all faces still present but removed
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 8 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 9 ), QgsMeshFace( ) ) );

  tri.removePoint( 2 );
  QCOMPARE( tri.pointsCount(), 6 );
  QCOMPARE( tri.dimension(),  0 );

  tri.removePoint( 1 );
  QCOMPARE( tri.pointsCount(), 6 );
  QCOMPARE( tri.dimension(),  -1 );

  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );

  QCOMPARE( changedVerticesIndex, QSet<int>( { 1, 2} ) );
  QCOMPARE( mesh.faces.count(), 10 ); //faces are still presents but void
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 8 ), QgsMeshFace( ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 9 ), QgsMeshFace( ) ) );
  tri.endEditMode();
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( mesh.faces.count(), 0 );

  tri.startEditMode();
  // with colineear points
  tri.addPoint( QgsPoint( 1, 1 ) );
  tri.addPoint( QgsPoint( 1, 2 ) );
  tri.addPoint( QgsPoint( 1, 3 ) );
  tri.addPoint( QgsPoint( 2, 1 ) );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  2 );

  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 3} ) );
  QCOMPARE( mesh.faces.count(), 2 );

  tri.removePoint( 3 );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  1 );
  tri.removePoint( 2 );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  1 );
  tri.removePoint( 1 );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  0 );
  tri.removePoint( 0 );
  QCOMPARE( tri.pointsCount(), 4 );
  QCOMPARE( tri.dimension(),  -1 );

  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 3} ) );

  // again with colineear points but remove in other order
  tri.addPoint( QgsPoint( 1, 1 ) );
  tri.addPoint( QgsPoint( 1, 2 ) );
  tri.addPoint( QgsPoint( 1, 3 ) );
  tri.addPoint( QgsPoint( 2, 1 ) );
  QCOMPARE( tri.pointsCount(), 8 );
  QCOMPARE( tri.dimension(),  2 );

  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {4, 5, 6, 7} ) );
  QCOMPARE( mesh.faces.count(), 4 );

  tri.removePoint( 6 );
  QCOMPARE( tri.pointsCount(), 8 );
  QCOMPARE( tri.dimension(),  2 );
  tri.removePoint( 5 );
  QCOMPARE( tri.pointsCount(), 8 );
  QCOMPARE( tri.dimension(),  1 );
  tri.removePoint( 7 );
  QCOMPARE( tri.pointsCount(), 8 );
  QCOMPARE( tri.dimension(),  0 );
  tri.removePoint( 4 );
  QCOMPARE( tri.pointsCount(), 8 );
  QCOMPARE( tri.dimension(),  -1 );
  tri.endEditMode();

  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {4, 5, 6, 7} ) );
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

  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 3, 4, 5, 6, 7} ) );

  tri.swapEdge( 1, 0.75 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 7} ) );
  tri.swapEdge( 3, 0.75 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {2, 3, 4, 7} ) );
  tri.swapEdge( 5, 0.9 );
  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {4, 5, 6, 7} ) );

  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {1, 2, 7} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {3, 2, 1} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {7, 2, 3 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {3, 4, 7 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( {5, 4, 3 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( {5, 6, 7 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( {7, 0, 1 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( {7, 4, 5 } ) ) );

  tri.removePoint( 7 );

  mesh = tri.editedTriangulationToMesh( changedVerticesIndex );
  QCOMPARE( changedVerticesIndex, QSet<int>( {0, 1, 2, 3, 4, 5, 6, 7} ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 0 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 1 ), QgsMeshFace( {3, 2, 1} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 2 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 3 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 4 ), QgsMeshFace( {5, 4, 3 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 5 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 6 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 7 ), QgsMeshFace( {} ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 8 ), QgsMeshFace( {0, 2, 4 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 9 ), QgsMeshFace( {0, 1, 2 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 10 ), QgsMeshFace( {2, 3, 4 } ) ) );
  QVERIFY( QgsMesh::compareFaces( mesh.face( 11 ), QgsMeshFace( {4, 5, 6 } ) ) );

  tri.endEditMode();
}

void TestQgsInterpolator::TIN_IDW_Interpolator_with_Z()
{
  std::unique_ptr<QgsVectorLayer>mLayerPoint = std::make_unique<QgsVectorLayer>( QStringLiteral( "PointZ" ),
      QStringLiteral( "point" ),
      QStringLiteral( "memory" ) );

  QString wkt1 = "PointZ (0.0 0.0 1.0)";
  QString wkt2 = "PointZ (2.0 0.0 2.0)";
  QString wkt3 = "PointZ (0.0 2.0 3.0)";
  QString wkt4 = "PointZ (2.0 2.0 4.0)";


  QgsFields fields = mLayerPoint->fields();

  QgsFeature f1;
  f1.setGeometry( QgsGeometry::fromWkt( wkt1 ) );
  f1.setFields( fields, true );
  f1.setAttribute( "ZValue", 1.0 );
  QgsFeature f2;
  f2.setGeometry( QgsGeometry::fromWkt( wkt2 ) );
  f2.setFields( fields, true );
  f2.setAttribute( "ZValue", 2.0 );
  QgsFeature f3;
  f3.setFields( fields, true );
  f3.setGeometry( QgsGeometry::fromWkt( wkt3 ) );
  f3.setAttribute( "ZValue", 3.0 );
  QgsFeature f4;
  f4.setFields( fields, true );
  f4.setGeometry( QgsGeometry::fromWkt( wkt4 ) );
  f4.setAttribute( "ZValue", 4.0 );


  QgsFeatureList flist;
  flist << f1 << f2 << f3 << f4;
  mLayerPoint->dataProvider()->addFeatures( flist );

  QgsInterpolator::LayerData layerdata;
  layerdata.source = mLayerPoint.get();
  layerdata.valueSource = QgsInterpolator::ValueZ;
  QList<QgsInterpolator::LayerData> layerDataList;
  layerDataList.append( layerdata );

  QgsTinInterpolator tin( layerDataList );
  QgsIDWInterpolator idw( layerDataList );

  double resultTIN = -1;
  double resutlIDW = -1;
  QCOMPARE( tin.interpolatePoint( 0.5, 0.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 0.5, 0.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 1.75, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 1.6176470588, 0.00000001 ) );

  QCOMPARE( tin.interpolatePoint( 0.25, 0.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 0.25, 0.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 1.625, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 1.41999627456, 0.00000001 ) );

  QCOMPARE( tin.interpolatePoint( 1, 0.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 1, 0.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 2.0, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 2.05555555556, 0.00000001 ) );

  QCOMPARE( tin.interpolatePoint( 1, 1.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 1, 1.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 3.0, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 2.94444444444, 0.00000001 ) );

  //! Outside the hull, TIN interpolator has to return an error
  QVERIFY( tin.interpolatePoint( 2.5, 1.5, resultTIN, nullptr ) != 0 );
}

void TestQgsInterpolator::TIN_IDW_Interpolator_with_attribute()
{
  std::unique_ptr<QgsVectorLayer>mLayerPoint = std::make_unique<QgsVectorLayer>( QStringLiteral( "Point?field=ZValue:real" ),
      QStringLiteral( "point" ),
      QStringLiteral( "memory" ) );

  QVERIFY( mLayerPoint->fields().field( "ZValue" ).type() == QVariant::Double );

  QString wkt1 = "Point (0 0)";
  QString wkt2 = "Point (2 0)";
  QString wkt3 = "Point (0 2)";
  QString wkt4 = "Point (2 2)";
  QString wkt5 = "Point (1 1)";
  QString wkt6 = "Point (3 2)";

  QgsFields fields = mLayerPoint->fields();

  QgsFeature f1;
  f1.setGeometry( QgsGeometry::fromWkt( wkt1 ) );
  f1.setFields( fields, true );
  f1.setAttribute( "ZValue", 1.0 );
  QgsFeature f2;
  f2.setGeometry( QgsGeometry::fromWkt( wkt2 ) );
  f2.setFields( fields, true );
  f2.setAttribute( "ZValue", 2.0 );
  QgsFeature f3;
  f3.setFields( fields, true );
  f3.setGeometry( QgsGeometry::fromWkt( wkt3 ) );
  f3.setAttribute( "ZValue", 3.0 );
  QgsFeature f4;
  f4.setFields( fields, true );
  f4.setGeometry( QgsGeometry::fromWkt( wkt4 ) );
  f4.setAttribute( "ZValue", 4.0 );
  QgsFeature f5;
  f5.setFields( fields, true );
  f5.setGeometry( QgsGeometry::fromWkt( wkt5 ) );
  f5.setAttribute( "ZValue", QVariant( QVariant::Double ) ); //NULL value has to be ignore
  QgsFeature f6;
  f6.setFields( fields, true );
  f6.setGeometry( QgsGeometry::fromWkt( wkt6 ) );
  f6.setAttribute( "ZValue", QVariant() ); //NULL value has to be ignore


  QgsFeatureList flist;
  flist << f1 << f2 << f3 << f4 << f5 << f6;
  mLayerPoint->dataProvider()->addFeatures( flist );

  QgsInterpolator::LayerData layerdata;
  layerdata.source = mLayerPoint.get();
  layerdata.interpolationAttribute = mLayerPoint->fields().lookupField( "ZValue" );
  QList<QgsInterpolator::LayerData> layerDataList;
  layerDataList.append( layerdata );

  QgsTinInterpolator tin( layerDataList );
  QgsIDWInterpolator idw( layerDataList );

  double resultTIN = -1;
  double resutlIDW = -1;
  QCOMPARE( tin.interpolatePoint( 0.5, 0.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 0.5, 0.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 1.75, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 1.6176470588, 0.00000001 ) );

  QCOMPARE( tin.interpolatePoint( 0.25, 0.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 0.25, 0.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 1.625, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 1.41999627456, 0.00000001 ) );

  QCOMPARE( tin.interpolatePoint( 1, 0.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 1, 0.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 2.0, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 2.05555555556, 0.00000001 ) );

  QCOMPARE( tin.interpolatePoint( 1, 1.5, resultTIN, nullptr ), 0 );
  QCOMPARE( idw.interpolatePoint( 1, 1.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resultTIN, 3.0, 0.00000001 ) );
  QVERIFY( qgsDoubleNear( resutlIDW, 2.94444444444, 0.00000001 ) );

  //! Outside the hull, TIN interpolator has to return an error
  QVERIFY( tin.interpolatePoint( 2.5, 1.5, resultTIN, nullptr ) != 0 );
  QCOMPARE( idw.interpolatePoint( 2.5, 1.5, resutlIDW, nullptr ), 0 );
  QVERIFY( qgsDoubleNear( resutlIDW, 3.5108401084, 0.00000001 ) );
}

QGSTEST_MAIN( TestQgsInterpolator )
#include "testqgsinterpolator.moc"
