# -*- coding: utf-8 -*-

"""
***************************************************************************
    TinInterpolation.py
    ---------------------
    Date                 : October 2016
    Copyright            : (C) 2016 by Alexander Bruy
    Email                : alexander dot bruy at gmail dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Vincent Cloarec'
__date__ = 'August 2020'
__copyright__ = '(C) 2020, Vincent Cloarec'

import os
import math

from qgis.PyQt.QtGui import QIcon

from qgis.core import (QgsProcessingUtils,
                       QgsProcessing,
                       QgsProcessingParameterEnum,
                       QgsProcessingParameterNumber,
                       QgsProcessingParameterExtent,
                       QgsProcessingParameterDefinition,
                       QgsProcessingParameterRasterDestination,
                       QgsWkbTypes,
                       QgsProcessingParameterFeatureSink,
                       QgsProcessingException,
                       QgsCoordinateReferenceSystem)
from qgis.analysis import (QgsInterpolator,
                           QgsTinInterpolator,
                           QgsGridFileWriter)

from processing.algs.qgis.QgisAlgorithm import QgisAlgorithm
from processing.algs.qgis.ui.TinMeshWidgets import ParameterTinMeshData

pluginPath = os.path.split(os.path.split(os.path.dirname(__file__))[0])[0]


class TinMeshCreation(QgisAlgorithm):
    SOURCE_DATA = 'SOURCE_DATA'
    OUTPUT_MESH = 'OUTPUT_MESH'

    def icon(self):
        return QIcon(os.path.join(pluginPath, 'images', 'interpolation.png'))

    def group(self):
        return self.tr('Mesh')

    def groupId(self):
        return 'mesh'

    def __init__(self):
        super().__init__()

    def initAlgorithm(self, config=None):
        self.addParameter(ParameterTinMeshData(self.SOURCE_DATA,self.tr('Input layer(s)')))

    def name(self):
        return 'tinmeshcreation'

    def displayName(self):
        return self.tr('TIN mesh creation')

    def processAlgorithm(self, parameters, context, feedback):
        sourceData = ParameterTinMeshData.parseValue(parameters[self.SOURCE_DATA])

        if sourceData is None:
            raise QgsProcessingException(
                self.tr('You need to specify at least one input layer.'))

        pointsLayers = []
        pointsValueSource = []
        breakLineLayers= []
        breakLinesValueSource = []
        crs = QgsCoordinateReferenceSystem()
        for i, row in enumerate(sourceData.split('::|::')):
            v = row.split('::~::')

            # need to keep a reference until interpolation is complete
            sourceLayer = QgsProcessingUtils.variantToSource(v[0], context)
            transformContext = context.transformContext()

            if not crs.isValid():
                crs = sourceLayer.sourceCrs()

            valueSource = int(v[1])
            valueAttribute = int(v[2])
            if valueSource == "value_attribute" and valueAttribute == -1:
                raise QgsProcessingException(self.tr('Layer {} is set to use a value attribute, but no attribute was set'.format(i + 1)))

            if v[3] == '0':  #points
                pointsLayers.append(sourceLayer)
                pointsValueSource.append(valueAttribute)
            else:            #lines
                breakLineLayers.append(sourceLayer)
                breakLinesValueSource.append(valueAttribute)

        #
        # interpolator = QgsTinInterpolator(layerData, interpolationMethod, feedback)
        # if triangulation_sink is not None:
        #     interpolator.setTriangulationSink(triangulation_sink)
        #

        return {self.OUTPUT_MESH: 0}
