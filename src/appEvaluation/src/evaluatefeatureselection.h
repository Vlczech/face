#ifndef EVALUATEFEATURESELECTION_H
#define EVALUATEFEATURESELECTION_H

#include <QVector>

#include "faceCommon/biometrics/template.h"
#include "faceCommon/biometrics/biodataprocessing.h"
#include "faceCommon/biometrics/eerpotential.h"
#include "faceCommon/biometrics/discriminativepotential.h"
#include "faceCommon/linalg/vector.h"
#include "faceCommon/linalg/metrics.h"
#include "faceCommon/biometrics/featureselection.h"
#include "faceCommon/linalg/loader.h"
#include "faceCommon/linalg/pca.h"

class EvaluateFeatureSelection
{
public:

    static void evaluateFRGCShapeIndexPCATemplatesCorrDist()
    {
        std::vector<Template> allTemplates =
                Template::loadTemplates("/media/frgc/frgc-norm-iterative/shapeindex-roi2-pca","-");
        QList<std::vector<Template> > clusters = BioDataProcessing::divideTemplatesToClusters(allTemplates, 25);
        clusters.removeLast();

        //EERPotential eerPotential(clusters[0]);

        /*ZScoreNormResult zScoreNormParams = Template::zScoreNorm(clusters[0]);
        for (int i = 1; i < clusters.count(); i++)
        {
            Template::zScoreNorm(clusters[i], zScoreNormParams);
        }*/

        CorrelationMetric corrMetric;

        BatchEvaluationResult bev = Evaluation::batch(clusters, corrMetric);
        for (int i = 0; i < bev.results.count(); i++)
        {
            qDebug() << bev.results[i].eer;
        }
    }

    static void evaluateFRGCShapeIndexPCATemplatesCosineDist()
    {
        std::vector<Template> allTemplates =
                Template::loadTemplates("/media/frgc/frgc-norm-iterative/shapeindex-roi2-pca","-");
        QList<std::vector<Template> > clusters = BioDataProcessing::divideTemplatesToClusters(allTemplates, 25);
        clusters.removeLast();

        CosineMetric cosineMetric;
        //EuclideanMetric euclideanMetric;
        //CityblockMetric cityblockMetric;

        EERPotential eerPotential(clusters[0]);


        ZScoreNormalizationResult zScoreNormParams = Template::zScoreNorm(clusters[0]);
        //Template::stats(clusters[0], "hist-0");
        //Template::zScoreNorm(clusters[0], zScoreNormParams);
        for (int i = 1; i < clusters.count(); i++)
        {
            Template::zScoreNorm(clusters[i], zScoreNormParams);
            //Template::stats(clusters[i], "hist-" + QString::number(i));
        }

        CosineWeightedMetric cosineWeightedMetric;
        Matrix scores = eerPotential.createWeights();
        Matrix flags = eerPotential.createSelectionWeights(0.5);
        cosineWeightedMetric.w = flags; //scores.mul(flags);
        //cosineWeightedMetric.normalizeWeights();

        //std::vector<Matrix> trainVectors = Template::getVectors(clusters[0]);
        //MahalanobisMetric mahalanobisMetric(trainVectors);

        BatchEvaluationResult bev = Evaluation::batch(clusters, cosineWeightedMetric);
        for (int i = 0; i < bev.results.count(); i++)
        {
            qDebug() << bev.results[i].eer;
        }
    }

    static void createFRGCShapeIndexPCATemplates()
    {
        std::vector<int> allClasses;
        std::vector<Vector> allImages;
        Loader::loadImages("/media/frgc/frgc-norm-iterative/shapeindex-roi2",
                           allImages, &allClasses, "*.png", "d", false);

        QList<std::vector<int> > classClusters;
        QList<std::vector<Vector> > imagesClusters;
        BioDataProcessing::divideTemplatesToClusters(allImages, allClasses, 25, imagesClusters, classClusters);
        //classClusters.removeLast(); imagesClusters.removeLast();

        PCA pca(imagesClusters[1]);
        pca.modesSelectionThreshold();

        // create templates
        qDebug() << "Creating templates";
        QList<std::vector<Template> > templates;
        for (int i = 0; i < classClusters.count(); i++)
        {
            std::vector<Template> curCluster;
            for (int j = 0; j < classClusters[i].count(); j++)
            {
                Template t;
                t.subjectID = classClusters[i][j];
                t.featureVector = pca.project(imagesClusters[i][j]);
                curCluster.append(t);
            }
            templates.append(curCluster);
        }

        qDebug() << "Saving templates";
        Template::saveTemplates(templates, "/media/frgc/frgc-norm-iterative/shapeindex-roi2-pca2");
    }

    /*static void compareEERandDPSelectionOnFRGCAnatomical()
    {
        std::vector<Template> templates = Template::loadTemplates("/home/stepo/SVN/frgc/frgc-norm-iterative/anatomical","-");
        QList<std::vector<Template> > clusters = BioDataProcessing::divide(templates, 25);
        clusters.removeLast();

        CityblockWeightedMetric metrics;

        DiscriminativePotential discriminativePotential(clusters[0]);

        // normalize anatomical Templates
        for (int i = 0; i < clusters.count(); i++)
        {
            Template::normalizeFeatureVectorComponents(clusters[i],
                                                       discriminativePotential.minValues,
                                                       discriminativePotential.maxValues);
        }

        std::vector<Template> trainTemplates = clusters[0];
        clusters.removeFirst();*/

        /*// evaluate DP
        double step = (discriminativePotential.maxScore - discriminativePotential.minScore)/20;
        double bestTrain = 1.0;
        double bestValidation = 1.0;
        for (double t = discriminativePotential.minScore; t <= discriminativePotential.maxScore; t += step)
        {
            metrics.w = discriminativePotential.createSelectionWeights(t);

            Evaluation onTrain(trainTemplates, metrics, false);
            BatchEvaluationResult eval = Evaluation::batch(clusters, metrics);
            //qDebug() << t << "DP on test:" << onTrain.eer << "validation:" << eval.meanEER << eval.stdDevOfEER;

            if (onTrain.eer < bestTrain)
            {
                bestTrain = onTrain.eer;
                bestValidation = eval.meanEER;
            }
        }
        qDebug() << "DP: train:" << bestTrain << "validation:" << bestValidation;

        // evaluate EER Potential
        EERPotential eerPotential(trainTemplates);
        step = (eerPotential.maxScore - eerPotential.minScore)/20;
        bestTrain = 1.0;
        bestValidation = 1.0;
        for (double t = eerPotential.minScore; t <= eerPotential.maxScore; t += step)
        {
            metrics.w = eerPotential.createSelectionWeights(t);

            Evaluation onTrain(clusters[0], metrics, false);
            BatchEvaluationResult eval = Evaluation::batch(clusters, metrics);
            //qDebug() << t << "DP on test:" << onTrain.eer << "validation:" << eval.meanEER << eval.stdDevOfEER;

            if (onTrain.eer < bestTrain)
            {
                bestTrain = onTrain.eer;
                bestValidation = eval.meanEER;
            }
        }
        qDebug() << "DP: train:" << bestTrain << "validation:" << bestValidation;*/

        /*FeatureSelection selection(trainTemplates, metrics, &clusters);
    }
};*/

#endif // EVALUATEFEATURESELECTION_H
