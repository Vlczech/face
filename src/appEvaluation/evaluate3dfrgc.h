#ifndef EVALUATE3DFRGC_H_
#define EVALUATE3DFRGC_H_

#include <qvector.h>
#include <qstring.h>
#include <qdebug.h>
#include <cassert>

#include "linalg/common.h"
#include "linalg/loader.h"
#include "linalg/vector.h"
#include "linalg/pca.h"
#include "linalg/matrixconverter.h"
#include "biometrics/biodataprocessing.h"
#include "biometrics/featureextractor.h"
#include "biometrics/discriminativepotential.h"
#include "biometrics/scorelevefusion.h"
#include "facelib/mesh.h"
#include "facelib/surfaceprocessor.h"
#include "facelib/landmarkdetector.h"
#include "facelib/landmarks.h"
#include "facelib/facealigner.h"
#include "facelib/glwidget.h"
#include "linalg/kernelgenerator.h"
#include "linalg/serialization.h"
#include "biometrics/isocurveprocessing.h"
#include "linalg/histogram.h"
#include "biometrics/histogramfeatures.h"
#include "biometrics/geneticweightoptimization.h"
#include "biometrics/eerpotential.h"

class Evaluate3dFrgc
{
public:
	static void compare(QVector<int> &first, QVector<int> &second)
	{
		int n = first.count();
		assert(n == second.count());
		for (int i = 0; i < n; i++)
			assert(first[i] == second[i]);
	}

    static void align()
    {
        QString srcDirPath = "/home/stepo/data/frgc/spring2004/bin/";
        QString outDirPath = "/home/stepo/data/frgc/spring2004/zbin-aligned/";

        Mesh mean = Mesh::fromOBJ("../../test/meanForAlign.obj");
        FaceAligner aligner(mean);
        MapConverter converter;

        QDir srcDir(srcDirPath, "*.bin");
        QFileInfoList srcFiles = srcDir.entryInfoList();
        foreach (const QFileInfo &srcFileInfo, srcFiles)
        {
            Mesh mesh = Mesh::fromBIN(srcFileInfo.absoluteFilePath());
            aligner.icpAlign(mesh, 15);

            QString resultPath = outDirPath + srcFileInfo.baseName() + ".binz";
            mesh.writeBINZ(resultPath);

            Map texture = SurfaceProcessor::depthmap(mesh, converter, cv::Point2d(-100,-100), cv::Point2d(100,100), 1, Texture_I);
            QString resultTexturePath = outDirPath + srcFileInfo.baseName() + ".png";
            cv::imwrite(resultTexturePath.toStdString(), texture.toMatrix()*255);
        }
    }

    static int createIsoCurves()
    {
        QString srcDirPath = "/home/stepo/data/frgc/spring2004/zbin-aligned/";
        QString outDirPath = "/home/stepo/data/frgc/spring2004/zbin-aligned/isocurves2/";
        QDir srcDir(srcDirPath, "*.binz");
        QFileInfoList srcFiles = srcDir.entryInfoList();
        foreach (const QFileInfo &srcFileInfo, srcFiles)
        {
            QString resultPath = outDirPath + srcFileInfo.baseName() + ".xml";
            if (QFile::exists(resultPath)) continue;

            Mesh mesh = Mesh::fromBINZ(srcFileInfo.absoluteFilePath());
            LandmarkDetector detector(mesh);
            Landmarks lm = detector.detect();
            cv::Point3d nosetip = lm.get(Landmarks::Nosetip);
            mesh.translate(-nosetip);

            MapConverter converter;
            Map depth = SurfaceProcessor::depthmap(mesh, converter, 2, ZCoord);
            Matrix gaussKernel = KernelGenerator::gaussianKernel(7);
            depth.applyFilter(gaussKernel, 3, true);

            QVector<VectorOfPoints> isoCurves;
            int startD = 10;
            for (int d = startD; d <= 100; d += 10)
            {
                VectorOfPoints isoCurve = SurfaceProcessor::isoGeodeticCurve(depth, converter, cv::Point3d(0,20,0), d, 100, 2);
                isoCurves << isoCurve;
            }

            Serialization::serializeVectorOfPointclouds(isoCurves, resultPath);
        }
    }

    static void evaluateIsoCurves()
    {
        QString dirPath = "/home/stepo/data/frgc/spring2004/zbin-aligned/isocurves2/";
        QVector<SubjectIsoCurves> data = IsoCurveProcessing::readDirectory(dirPath, "d", "*.xml");
        IsoCurveProcessing::selectIsoCurves(data, 0, 5);
        IsoCurveProcessing::stats(data);
        IsoCurveProcessing::sampleIsoCurvePoints(data, 5);
        QVector<Template> rawData = IsoCurveProcessing::generateTemplates(data);
        //QVector<Template> rawData = IsoCurveProcessing::generateEuclDistanceTemplates(data);

        QVector<int> classes;
        QVector<Vector> rawFeatureVectors;
        Template::splitVectorsAndClasses(rawData, rawFeatureVectors, classes);

        int clusterCount = 10;
        QList<QVector<int> > classesInClusters;
        QList<QVector<Vector> > rawVectorsInClusters;
        BioDataProcessing::divideToNClusters(rawFeatureVectors, classes, clusterCount, rawVectorsInClusters, classesInClusters);

        PCA pca(rawVectorsInClusters[0]);
        //PCAExtractor pcaExtractor(pca);
        ZScorePCAExtractor zscorePcaExtractor(pca, rawVectorsInClusters[1]);
        zscorePcaExtractor.serialize("../../test/isocurves/shifted-pca.yml", "../../test/isocurves/shifted-normparams.yml");
        //EuclideanMetric euclMetric;
        CosineMetric cosMetric;

        BatchEvaluationResult batchResult = Evaluation::batch(rawVectorsInClusters, classesInClusters, zscorePcaExtractor, cosMetric, 2);
        qDebug() << batchResult.meanEER << batchResult.stdDevOfEER;
    }

    static int createMaps()
    {
        QString srcDirPath = "/home/stepo/data/frgc/spring2004/zbin-aligned/";
        QDir srcDir(srcDirPath, "*.binz");
        QFileInfoList srcFiles = srcDir.entryInfoList();
        MapConverter converter;

        Matrix smoothKernel2 = KernelGenerator::gaussianKernel(7);
        //QVector<double> allZValues;
        //bool first = true;
        foreach (const QFileInfo &srcFileInfo, srcFiles)
        {
            if (QFile::exists(srcDirPath + "depth2/" + srcFileInfo.baseName() + ".png") &&
                QFile::exists(srcDirPath + "mean2/" + srcFileInfo.baseName() + ".png") &&
                QFile::exists(srcDirPath + "gauss2/" + srcFileInfo.baseName() + ".png") &&
                QFile::exists(srcDirPath + "index2/" + srcFileInfo.baseName() + ".png") &&
                QFile::exists(srcDirPath + "eigencur2/" + srcFileInfo.baseName() + ".png"))
            {
                continue;
            }

            Mesh mesh = Mesh::fromBINZ(srcFileInfo.absoluteFilePath());
            Map depthmap = SurfaceProcessor::depthmap(mesh, converter,
                                                      cv::Point2d(-75, -75),
                                                      cv::Point2d(75, 75),
                                                      2, ZCoord);
            //allZValues << depthmap.getUsedValues();

            depthmap.bandPass(-75, 0, false, false);
            Matrix depthImage = depthmap.toMatrix(0, -75, 0);
            QString out = srcDirPath + "depth2/" + srcFileInfo.baseName() + ".png";
            cv::imwrite(out.toStdString(), depthImage*255);

            Map smoothedDepthmap = depthmap;
            smoothedDepthmap.applyFilter(smoothKernel2, 7, true);
            CurvatureStruct cs = SurfaceProcessor::calculateCurvatures(smoothedDepthmap);

            cs.curvatureMean.bandPass(-0.1, 0.1, false, false);
            Matrix meanImage = cs.curvatureMean.toMatrix(0, -0.1, 0.1);
            out = srcDirPath + "mean2/" + srcFileInfo.baseName() + ".png";
            cv::imwrite(out.toStdString(), meanImage*255);

            cs.curvatureGauss.bandPass(-0.01, 0.01, false, false);
            Matrix gaussImage = cs.curvatureGauss.toMatrix(0, -0.01, 0.01);
            out = srcDirPath + "gauss2/" + srcFileInfo.baseName() + ".png";
            cv::imwrite(out.toStdString(), gaussImage*255);

            cs.curvatureIndex.bandPass(0, 1, false, false);
            Matrix indexImage = cs.curvatureIndex.toMatrix(0, 0, 1);
            out = srcDirPath + "index2/" + srcFileInfo.baseName() + ".png";
            cv::imwrite(out.toStdString(), indexImage*255);

            cs.curvaturePcl.bandPass(0, 0.0025, false, false);
            Matrix pclImage = cs.curvaturePcl.toMatrix(0, 0, 0.0025);
            out = srcDirPath + "eigencur2/" + srcFileInfo.baseName() + ".png";
            cv::imwrite(out.toStdString(), pclImage*255);
        }

        /*Histogram zValuesHistogram(allZValues, 20, false);
        qDebug() << "mean" << zValuesHistogram.mean;
        qDebug() << "stdDev" << zValuesHistogram.stdDev;
        qDebug() << "min" << zValuesHistogram.minValue;
        qDebug() << "max" << zValuesHistogram.maxValue;
        Common::savePlot(zValuesHistogram.histogramValues, zValuesHistogram.histogramCounter, "zValuesHistogram");*/
    }

    static void evaluateHistogramFeaturesGenerateStripesBinsMap()
    {
        QString srcDirPath = "/home/stepo/data/frgc/spring2004/zbin-aligned/depth2";
        QDir srcDir(srcDirPath, "*.png");
        QFileInfoList srcFiles = srcDir.entryInfoList();

        qDebug() << "Loading...";
        QVector<ImageGrayscale> allImages;
        QVector<int> allClasses;
        foreach (const QFileInfo &fileInfo, srcFiles)
        {
            ImageGrayscale full = cv::imread(fileInfo.absoluteFilePath().toStdString(), cv::IMREAD_GRAYSCALE);
            cv::GaussianBlur(full, full, cv::Size(21,21), 0);
            ImageGrayscale cropped = full(cv::Rect(40, 20, 220, 180));
            allImages << cropped;
            allClasses << fileInfo.baseName().split('d')[0].toInt();
        }

        QList<QVector<ImageGrayscale> > imagesInClusters;
        QList<QVector<int> > classesInClusters;
        BioDataProcessing::divideToNClusters<ImageGrayscale>(allImages, allClasses, 10, imagesInClusters, classesInClusters);

        for (int i = 0; i < 3; i++)
        {
            qDebug() << "Evaluating" << i << imagesInClusters[i].count();
            for (int stripes = 3; stripes <= 50; stripes++)
            {
                for (int bins = 3; bins <= 50; bins++)
                {
                    QVector<Vector> rawVectors;
                    foreach(const ImageGrayscale &image, imagesInClusters[i])
                    {
                        HistogramFeatures features(image, stripes, bins);
                        rawVectors << features.toVector();
                    }

                    Evaluation e(rawVectors, classesInClusters[i], PassExtractor(), CityblockMetric());
                    qDebug() << stripes << bins << e.eer; //  results.meanEER << "+-" << results.stdDevOfEER;
                }
                qDebug() << "";
            }
        }
    }

    static void evaluateHistogramFeatures()
    {
        QString srcDirPath = "/home/stepo/data/frgc/spring2004/zbin-aligned/";
        QDir srcDir(srcDirPath, "*.binz");
        QFileInfoList srcFiles = srcDir.entryInfoList();

        cv::Point2d meshStart(-60,-30);
        cv::Point2d meshEnd(60,60);
        int kSize = 9;
        int gaussTimes = 1;
        //int stripes = 20;
        //int bins = 20;

        Matrix gaussKernel = KernelGenerator::gaussianKernel(kSize);

        qDebug() << "Loading...";
        QVector<int> allClasses;
        QVector<Map> allMaps;
        bool first = true;
        foreach (const QFileInfo &fileInfo, srcFiles)
        {
            Mesh mesh = Mesh::fromBINZ(fileInfo.absoluteFilePath());
            MapConverter mapConverter;
            Map depth = SurfaceProcessor::depthmap(mesh, mapConverter, meshStart, meshEnd, 2.0, ZCoord);
            depth.applyFilter(gaussKernel, gaussTimes, true);

            if (first)
            {
                first = false;
                cv::imshow("depth", depth.toMatrix());
                cv::waitKey();
            }
            allMaps << depth;
            allClasses << fileInfo.baseName().split('d')[0].toInt();
        }

        double minEER = 1.0;
        int minEERStripes;
        int minEERBins;
        for (int stripes = 26; stripes <= 30; stripes++)
        {
            for (int bins = 5; bins <= 30; bins++)
            {
                QVector<Vector> allRawVectors;

                foreach (const Map &map, allMaps)
                {
                    HistogramFeatures hf(map, stripes, bins);
                    allRawVectors << hf.toVector();
                }

                QList<QVector<int> > classesInClusters;
                QList<QVector<Vector> > rawVectorsInClusters;
                BioDataProcessing::divideToNClusters(allRawVectors, allClasses, 5, rawVectorsInClusters, classesInClusters);

                ZScorePassExtractor zPassExtractor(rawVectorsInClusters[1]);
                //Evaluation result(rawVectorsInClusters[1], classesInClusters[1], zPassExtractor, CosineMetric());
                BatchEvaluationResult result = Evaluation::batch(rawVectorsInClusters, classesInClusters, zPassExtractor, CosineMetric(), 2);
                qDebug() << stripes << bins << result.meanEER;

                if (result.meanEER < minEER)
                {
                    minEER = result.meanEER;
                    minEERStripes = stripes;
                    minEERBins = bins;
                }
            }
            qDebug() << "";
        }

        qDebug() << "Best results:" << minEERStripes << minEERBins << minEER;


        /*double selThresholds[5] = {0.95, 0.96, 0.97, 0.98, 0.99};
        for (int selThresholdIndex = 0; selThresholdIndex < 5; selThresholdIndex++)
        {
            double selThreshold = selThresholds[selThresholdIndex];
            PCA pca(rawVectorsInClusters[0]);
            pca.modesSelectionThreshold(selThreshold);

            PCAExtractor pcaExtractor(pca);
            ZScorePCAExtractor zPcaExtractor(pca, rawVectorsInClusters[1]);
            ZScorePassExtractor zPassExtractor(rawVectorsInClusters[1]);

            qDebug() << "selThreshold" << selThreshold << "PCA, SAD";
            BatchEvaluationResult results = Evaluation::batch(rawVectorsInClusters, classesInClusters, pcaExtractor, CityblockMetric(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "PCA, SSD";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, pcaExtractor, SumOfSquareDifferences(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "PCA, cos";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, pcaExtractor, CosineMetric(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "zPCA, SAD";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, zPcaExtractor, CityblockMetric(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "zPCA, SSD";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, zPcaExtractor, SumOfSquareDifferences(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "zPCA, cos";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, zPcaExtractor, CosineMetric(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "zPass, SAD";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, zPassExtractor, CityblockMetric(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "zPass, SSD";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, zPassExtractor, SumOfSquareDifferences(), 2);
            qDebug() << results.meanEER;

            qDebug() << "selThreshold" << selThreshold << "zPass, cos";
            results = Evaluation::batch(rawVectorsInClusters, classesInClusters, zPassExtractor, CosineMetric(), 2);
            qDebug() << results.meanEER;
        }*/

        /*qDebug() << "Initial results...";
        MahalanobisMetric mahal(rawVectorsInClusters[0]);
        BatchEvaluationResult results = Evaluation::batch(rawVectorsInClusters, classesInClusters, PassExtractor(), mahal, 2);
        qDebug() << results.meanEER << "+-" << results.stdDevOfEER;*/

        /*qDebug() << "Train set and validation set...";
        QVector<Template> trainSet = Template::joinVectorsAndClasses(rawVectorsInClusters[0], classesInClusters[0]);
        QVector<Template> validationSet = Template::joinVectorsAndClasses(rawVectorsInClusters[1], classesInClusters[1]);

        qDebug() << "EER potential...";
        EERPotential eerPotential(trainSet);
        CityblockWeightedMetric cityWeights;
        cityWeights.w = eerPotential.createWeights();

        qDebug() << "EER potential results...";
        results = Evaluation::batch(rawVectorsInClusters, classesInClusters, PassExtractor(), cityWeights, 2);
        qDebug() << results.meanEER << "+-" << results.stdDevOfEER;

        qDebug() << "Genetic optimization...";
        GeneticWeightOptimizationSettings settings;
        GeneticWeightOptimization optimization(trainSet, validationSet, cityWeights, settings);
        optimization.trainWeights();

        qDebug() << "Genetic results...";
        results = Evaluation::batch(rawVectorsInClusters, classesInClusters, PassExtractor(), cityWeights, 2);
        qDebug() << results.meanEER << "+-" << results.stdDevOfEER;*/
    }
};

#endif
