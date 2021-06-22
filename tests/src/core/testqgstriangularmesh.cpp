/***************************************************************************
                         testqgstriangularmesh.cpp
                         -------------------------
    begin                : January 2019
    copyright            : (C) 2019 by Peter Petrik
    email                : zilolv at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"
#include <QObject>
#include <QString>

//qgis includes...
#include "qgstriangularmesh.h"
#include "qgsapplication.h"
#include "qgsproject.h"
#include "qgis.h"

/**
 * \ingroup UnitTests
 * This is a unit test for a triangular mesh
 */
class TestQgsTriangularMesh : public QObject
{
    Q_OBJECT

  public:
    TestQgsTriangularMesh() = default;

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {} // will be called before each testfunction is executed.
    void cleanup() {} // will be called after every testfunction.

    void test_triangulate();

    void test_update_mesh();

    void test_centroids();

  private:
    void populateMeshVertices( QgsTriangularMesh &mesh );

};

void TestQgsTriangularMesh::populateMeshVertices( QgsTriangularMesh &mesh )
{
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 0, 0, 0 ) );
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 0, 1, 0 ) );
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 0, 1, 0 ) );
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 1, 1, 0 ) );
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 2, 0, 0 ) );
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 2, 1, 0 ) );
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 0, 2, 0 ) );
  mesh.mTriangularMesh.vertices.append( QgsMeshVertex( 1, 2, 0 ) );
}

void TestQgsTriangularMesh::initTestCase()
{
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();
  QgsApplication::showSettings();
}

void TestQgsTriangularMesh::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsTriangularMesh::test_triangulate()
{
  {
    QgsTriangularMesh mesh;
    populateMeshVertices( mesh );
    QgsMeshFace point = { 0 };
    mesh.triangulate( point, 0 );
    QCOMPARE( 0, mesh.mTriangularMesh.faces.size() );
  }

  {
    QgsTriangularMesh mesh;
    populateMeshVertices( mesh );
    QgsMeshFace line = { 0, 1 };
    mesh.triangulate( line, 0 );
    QCOMPARE( 0, mesh.mTriangularMesh.faces.size() );
  }

  {
    QgsTriangularMesh mesh;
    populateMeshVertices( mesh );
    QgsMeshFace triangle = { 0, 1, 2 };
    mesh.triangulate( triangle, 0 );
    QCOMPARE( 1, mesh.mTriangularMesh.faces.size() );
    QgsMeshFace firstTriangle = {1, 2, 0};
    QCOMPARE( firstTriangle, mesh.mTriangularMesh.faces[0] );
  }

  {
    QgsTriangularMesh mesh;
    populateMeshVertices( mesh );
    QgsMeshFace quad = { 0, 1, 2, 3 };
    mesh.triangulate( quad, 0 );
    QCOMPARE( 2, mesh.mTriangularMesh.faces.size() );
    QgsMeshFace firstTriangle = {2, 3, 0};
    QCOMPARE( firstTriangle, mesh.mTriangularMesh.faces[0] );
    QgsMeshFace secondTriangle = {1, 2, 0};
    QCOMPARE( secondTriangle, mesh.mTriangularMesh.faces[1] );
  }

  {
    QgsTriangularMesh mesh;
    populateMeshVertices( mesh );
    QgsMeshFace poly = { 1, 2, 3, 4, 5, 6, 7 };
    mesh.triangulate( poly, 0 );
    QCOMPARE( 5, mesh.mTriangularMesh.faces.size() );
  }
}

void TestQgsTriangularMesh::test_centroids()
{
  QgsTriangularMesh triangularMesh;

  QgsMesh nativeMesh;
  nativeMesh.vertices << QgsMeshVertex( 0, 10, 0 ) << QgsMeshVertex( 10, 10, 0 ) << QgsMeshVertex( 10, 0, 0 ) << QgsMeshVertex( 0, 0, 0 )
                      << QgsMeshVertex( 20, 0, 0 ) << QgsMeshVertex( 30, 10, 0 ) << QgsMeshVertex( 20, 10, 0 );

  nativeMesh.faces << QgsMeshFace( {0, 1, 2, 3} ) << QgsMeshFace( {1, 2, 4, 5} );

  triangularMesh.update( &nativeMesh, QgsCoordinateTransform() );

  QVector<QgsMeshVertex> centroids = triangularMesh.faceCentroids();

  QCOMPARE( 2, centroids.count() );

  QVERIFY( qgsDoubleNear( centroids.at( 0 ).x(), 5, 0.00001 ) );
  QVERIFY( qgsDoubleNear( centroids.at( 0 ).y(), 5, 0.00001 ) );
  QVERIFY( qgsDoubleNear( centroids.at( 1 ).x(), 17.777777777, 0.00001 ) );
  QVERIFY( qgsDoubleNear( centroids.at( 1 ).y(), 5.5555555555, 0.00001 ) );

  // with big coordinates
  nativeMesh.clear();
  triangularMesh = QgsTriangularMesh();

  nativeMesh.vertices << QgsMeshVertex( 900000000, 300000010, 0 ) << QgsMeshVertex( 900000010, 300000010, 0 ) << QgsMeshVertex( 900000010, 300000000, 0 ) << QgsMeshVertex( 900000000, 300000000, 0 )
                      << QgsMeshVertex( 900000020, 300000000, 0 ) << QgsMeshVertex( 900000030, 300000010, 0 ) << QgsMeshVertex( 900000020, 300000010, 0 );

  nativeMesh.faces << QgsMeshFace( {0, 1, 2, 3} ) << QgsMeshFace( {1, 2, 4, 5} );
  triangularMesh.update( &nativeMesh, QgsCoordinateTransform() );

  centroids = triangularMesh.faceCentroids();

  QCOMPARE( 2, centroids.count() );

  QVERIFY( qgsDoubleNear( centroids.at( 0 ).x(), 900000005, 0.00001 ) );
  QVERIFY( qgsDoubleNear( centroids.at( 0 ).y(), 300000005, 0.00001 ) );
  QVERIFY( qgsDoubleNear( centroids.at( 1 ).x(), 900000017.777777, 0.00001 ) );
  QVERIFY( qgsDoubleNear( centroids.at( 1 ).y(), 300000005.555555, 0.00001 ) );

}


void TestQgsTriangularMesh::test_update_mesh()
{
  //------------------------------------------
  // Construct a mesh of 100 x 100 square faces

  QgsMesh nativeMesh;
  QgsCoordinateReferenceSystem meshCrs( QStringLiteral( "EPSG:32620" ) );
  QgsCoordinateReferenceSystem mapCrs( QStringLiteral( "IGNF:GUAD48UTM20" ) );

  QgsCoordinateTransformContext transformContext;
  QgsCoordinateTransform transform;//( meshCrs, mapCrs, transformContext );

  double xOrigin = 691645.7;
  double yOrigin = 1758583.1;

  int xVerticesCount = 101;
  int yVerticesCount = 101;

  QgsPoint originInMap( xOrigin, yOrigin );

  for ( int i = 0; i < xVerticesCount; ++i )
    for ( int j = 0; j < yVerticesCount; ++j )
    {
      nativeMesh.vertices.append( QgsMeshVertex( xOrigin + i * 2.0, yOrigin + j * 2.0 ) );
    }

  for ( int i = 0; i < xVerticesCount - 1; ++i )
    for ( int j = 0; j < yVerticesCount - 1; ++j )
    {
      QgsMeshFace face( {i + xVerticesCount * j,
                         i + xVerticesCount * j + 1,
                         i + xVerticesCount * ( j + 1 ) + 1,
                         i + xVerticesCount * ( j + 1 )} );
      nativeMesh.faces.append( face );
    }

  QCOMPARE( nativeMesh.vertexCount(), xVerticesCount * yVerticesCount );
  QCOMPARE( nativeMesh.faceCount(), ( xVerticesCount - 1 ) * ( yVerticesCount - 1 ) );

  QgsTriangularMesh triangularMesh;
  triangularMesh.update( &nativeMesh, transform );

  QCOMPARE( triangularMesh.vertices().count(), xVerticesCount * yVerticesCount );
  QCOMPARE( triangularMesh.triangles().count(), 2 * ( xVerticesCount - 1 ) * ( yVerticesCount - 1 ) );

  //------------------------------------------
  // Remove only faces in the native meshes

  QSet<int> changedVertices;
  for ( int i = 25; i < 75; ++i )
    for ( int j = 25; j < 75; ++j )
    {
      for ( int v : nativeMesh.faces.at( i + ( xVerticesCount - 1 ) * j ) )
        changedVertices.insert( v );
      nativeMesh.faces[i + ( xVerticesCount - 1 ) * j] = QgsMeshFace();

    }

  QgsRectangle changedExtent( xOrigin + 50, yOrigin + 50, xOrigin + 150, yOrigin + 150 );
  QgsRectangle changedExtentInMap = transform.transformBoundingBox( changedExtent );
  changedExtentInMap.scale( 0.99 ); // reduce a bit the extent to avoid capturing neighbor faces

  QList<int> facesInCenter = triangularMesh.faceIndexesForRectangle( changedExtentInMap );
  QCOMPARE( facesInCenter.count(), 5000 );

  triangularMesh.update( &nativeMesh, changedVertices.values() );

  facesInCenter = triangularMesh.faceIndexesForRectangle( changedExtentInMap );
  QCOMPARE( facesInCenter.count(), 0 );

  changedExtent.setMinimal();

  //------------------------------------------
  // Add vertices and faces (100 square faces) in the native mesh where faces was removed
  changedVertices.clear();
  int vertCount = nativeMesh.vertexCount();
  for ( int i = 0; i < 11; ++i )
    for ( int j = 0; j < 11; ++j )
    {
      nativeMesh.vertices.append( QgsMeshVertex( xOrigin + 60 + i * 1.0, yOrigin + 60 + j * 1.0 ) );
      changedExtent.include( nativeMesh.vertices.last() );
      changedVertices.insert( nativeMesh.vertices.count() - 1 );
    }

  for ( int i = 0; i < 10; ++i )
    for ( int j = 0; j < 10; ++j )
    {
      nativeMesh.faces.append( QgsMeshFace( {vertCount + i + 11 * j,
                                             vertCount + i + 11 * j + 1,
                                             vertCount + i + 11 * ( j + 1 ) + 1,
                                             vertCount + i + 11 * ( j + 1 )} ) );
    }

  triangularMesh.update( &nativeMesh, changedVertices.values() );

  facesInCenter = triangularMesh.faceIndexesForRectangle( changedExtentInMap );
  QCOMPARE( facesInCenter.count(), 200 );

  //------------------------------------------
  // Add vertices (100) and faces (100 triangle faces) on a side
  changedVertices.clear();

  changedExtent = QgsRectangle( xOrigin - 1.5, yOrigin - 1, xOrigin + 0.5, yOrigin + 200 );
  changedExtentInMap = transform.transformBoundingBox( changedExtent );

  QList<int> facesOnSide = triangularMesh.faceIndexesForRectangle( changedExtentInMap );
  QCOMPARE( facesOnSide.count(), 200 );

  for ( int i = 0; i < 100; ++i )
  {
    nativeMesh.vertices.append( QgsMeshVertex( xOrigin - 1, yOrigin + 1 + i * 2 ) );
    nativeMesh.faces.append( QgsMeshFace( {i, i + 1, nativeMesh.vertices.count() - 1} ) );
    changedVertices.insert( nativeMesh.vertices.count() - 1 );
  }

  triangularMesh.update( &nativeMesh, changedVertices.values() );

  facesOnSide = triangularMesh.faceIndexesForRectangle( changedExtentInMap );
  QCOMPARE( facesOnSide.count(), 300 );
}

QGSTEST_MAIN( TestQgsTriangularMesh )
#include "testqgstriangularmesh.moc"
