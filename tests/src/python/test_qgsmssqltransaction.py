# -*- coding: utf-8 -*-
"""QGIS Unit tests for MS SQL transaction.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Vincent Cloarec'
__date__ = '2021-03-11'
__copyright__ = 'Copyright 2021, The QGIS Project'

import qgis

import os

from qgis.core import (QgsSettings,
                       QgsVectorLayer,
                       QgsFeatureRequest,
                       QgsFeature,
                       QgsField,
                       QgsFields,
                       QgsDataSourceUri,
                       QgsWkbTypes,
                       QgsGeometry,
                       QgsPointXY,
                       QgsProject,
                       QgsRectangle,
                       QgsProviderRegistry,
                       NULL,
                       QgsVectorLayerExporter,
                       QgsCoordinateReferenceSystem,
                       QgsDataProvider,
                       QgsTransaction)

from qgis.PyQt.QtCore import QDate
from utilities import unitTestDataPath
from qgis.testing import start_app, unittest
from providertestbase import ProviderTestCase

start_app()
TEST_DATA_DIR = unitTestDataPath()


class TestPyQgsMssqlTransaction(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""
        cls.dbconn = "service='testsqlserver' user=sa password='<YourStrong!Passw0rd>' sslmode=disable"
        if 'QGIS_MSSQLTEST_DB' in os.environ:
            cls.dbconn = os.environ['QGIS_MSSQLTEST_DB'] +' sslmode=disable'
        # Create test layers
        cls.point_layer = QgsVectorLayer(
            cls.dbconn + ' key=\'pk\' srid=4326 type=POINT table="qgis_test"."someData" (geom) sql=',
            'point_layer', 'mssql')
        assert cls.point_layer.dataProvider() is not None, "No data provider for {}".format(cls.point_layer.source())
        assert cls.point_layer.isValid(), cls.point_layer.dataProvider().error().message()
        cls.source = cls.point_layer.dataProvider()
        cls.poly_layer = QgsVectorLayer(
            cls.dbconn + ' key=\'pk\' srid=4326 type=POLYGON table="qgis_test"."some_poly_data" (geom) sql=',
            'poly_layer', 'mssql')
        assert cls.poly_layer.isValid(), cls.poly_layer.dataProvider().error().message()
        cls.poly_provider = cls.poly_layer.dataProvider()

        # Triggers a segfault in the sql server odbc driver on Travis - TODO test with more recent Ubuntu base image
        if os.environ.get('QGIS_CONTINUOUS_INTEGRATION_RUN', 'true'):
            del cls.getEditableLayer

        # Use connections API
        md = QgsProviderRegistry.instance().providerMetadata('mssql')
        cls.conn_api = md.createConnection(cls.dbconn, {})

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        pass

    def setUp(self):
        for t in ['new_table', 'new_table_multipoint', 'new_table_multipolygon']:
            self.execSQLCommand('DROP TABLE IF EXISTS qgis_test.[{}]'.format(t))

    def execSQLCommand(self, sql):
        self.assertTrue(self.conn_api)
        self.conn_api.executeSql(sql)

    def getEditableLayer(self):
        return self.getSource()

    def testTransactionInstance(self):
        md = QgsProviderRegistry.instance().providerMetadata('mssql')
        transaction = md.createTransaction(self.dbconn)
        assert transaction is not None

        self.assertTrue(transaction.supportsTransaction(self.point_layer))
        self.assertTrue(transaction.supportsTransaction(self.poly_layer))

        self.assertTrue(transaction.addLayer(self.point_layer))
        self.assertTrue(transaction.addLayer(self.poly_layer))

        error = transaction.begin()


    def testLayersEditing(self):
        project = QgsProject.instance()
        #project.setAutoTransaction(False)

        #project.addMapLayer(self.point_layer)
        #project.addMapLayer(self.poly_layer)
        #self.assertTrue(self.point_layer.startEditing())
        #self.assertTrue(self.point_layer.isEditable())
        #self.assertFalse(self.poly_layer.isEditable())

        #self.point_layer.rollBack()
        #self.assertFalse(self.point_layer.isEditable())
        #self.assertFalse(self.poly_layer.isEditable())

        #QgsProject.instance().setAutoTransaction(True)
        #self.assertTrue(self.point_layer.startEditing())
        #self.assertTrue(self.point_layer.isEditable())
        #self.assertTrue(self.poly_layer.isEditable())

if __name__ == '__main__':
    unittest.main()
