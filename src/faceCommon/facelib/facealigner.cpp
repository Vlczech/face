#include "facealigner.h"

#include <QDir>
#include <QFileInfoList>
#include <QFileInfo>

#include "facelib/landmarkdetector.h"
#include "linalg/procrustes.h"

void FaceAligner::init()
{
    sampleStartX = -40;
    sampleEndX = 40;
    sampleStartY = -40;
    sampleEndY = 60;
    sampleStep = 5;
}

FaceAligner::FaceAligner(Mesh &meanFace) : meanFace(meanFace)
{
    init();
}

FaceAligner::FaceAligner(const QString &dirWithLandmarksAndXYZfiles)
{
    init();

    QDir dir(dirWithLandmarksAndXYZfiles);
    QStringList xmlFilter; xmlFilter << "*.xml";
    QFileInfoList lmFiles = dir.entryInfoList(xmlFilter, QDir::Files, QDir::Name);
    QVector<Landmarks> vecOfLandmarks;

    int lmCount = 9;
    VectorOfPoints meanLandmarks(lmCount);
    QVector<Mesh> vectorOfFaces;
    foreach (const QFileInfo &lmInfo, lmFiles)
    {
        //if (vecOfLandmarks.count() == 5) break; // DEBUG ONLY!!!

        qDebug() << lmInfo.fileName();
        Landmarks lm(lmInfo.absoluteFilePath());
        vecOfLandmarks << lm;
        cv::Point3d shift = -lm.get(Landmarks::Nosetip);
        Procrustes3D::translate(lm.points, shift);

        for (int i = 0; i < lmCount; i++)
        {
            meanLandmarks[i] += lm.points[i];
        }

        Mesh f = Mesh::fromXYZFile(dirWithLandmarksAndXYZfiles + QDir::separator() + lmInfo.baseName() + ".abs.xyz");
        f.move(shift);
        vectorOfFaces << f;
    }
    for (int i = 0; i < lmCount; i++)
    {
        meanLandmarks[i].x = meanLandmarks[i].x/lmFiles.count();
        meanLandmarks[i].y = meanLandmarks[i].y/lmFiles.count();
        meanLandmarks[i].z = meanLandmarks[i].z/lmFiles.count();
    }

    Map meanDepth = Map(320, 480);
    meanDepth.setAll(0);
    MapConverter converter;
    for (int i = 0; i < vecOfLandmarks.count(); i++)
    {
        Matrix rotation = Procrustes3D::getOptimalRotation(vecOfLandmarks[i].points, meanLandmarks);
        vectorOfFaces[i].transform(rotation);
        Map depth = SurfaceProcessor::depthmap(vectorOfFaces[i], converter, cv::Point2d(-160, -240), cv::Point2d(160, 240), 1.0, ZCoord);
        meanDepth.add(depth);
    }

    for (int y = sampleStartY; y <= sampleEndY; y += sampleStep)
    {
        for (int x = sampleStartX; x <= sampleEndX; x += sampleStep)
        {
            cv::Point2d mapPoint = converter.MeshToMapCoords(meanDepth, cv::Point2d(x, y));
            cv::Point3d meshPoint = converter.MapToMeshCoords(meanDepth, mapPoint);
            meanFace.points << meshPoint;
        }
    }
    meanFace.scale(cv::Point3d(1,1,1.0/vecOfLandmarks.count()));
    meanFace.recalculateMinMax();
    meanFace.calculateTriangles();
}

Landmarks FaceAligner::align(Mesh &face, int iterations)
{
    assert(iterations >= 1);
    Landmarks lm;

    for (int i = 0; i < iterations; i++)
    {
        LandmarkDetector lmDetector(face);
        lm = lmDetector.detect();
        face.move(-lm.get(Landmarks::Nosetip));

        MapConverter converter;
        Map depth = SurfaceProcessor::depthmap(face, converter, 1.0, ZCoord);

        double minTheta;
        double minD = 1e300;
        Matrix rotation;
        for (double theta = -0.15; theta <= 0.15; theta += 0.005)
        {
            Mesh sampledFace;
            VectorOfPoints referencePoints;
            double cosT = cos(theta);
            double sinT = sin(theta);
            int index = 0;
            for (int y = sampleStartY; y <= sampleEndY; y += sampleStep)
            {
                for (int x = sampleStartX; x <= sampleEndX; x += sampleStep)
                {
                    double xr = x * cosT - y * sinT;
                    double yr = x * sinT + y * cosT;
                    cv::Point2d mapPoint = converter.MeshToMapCoords(depth, cv::Point2d(xr, yr));
                    bool success;
                    cv::Point3d meshPoint = converter.MapToMeshCoords(depth, mapPoint, &success);
                    if (success)
                    {
                        sampledFace.points << meshPoint;
                        referencePoints << meanFace.points[index];
                    }
                    index++;
                }
            }

            sampledFace.rotate(0, 0, -theta);
            sampledFace.recalculateMinMax();
            sampledFace.calculateTriangles();
            MapConverter testMC;
            Map testMap = SurfaceProcessor::depthmap(sampledFace, testMC, 1, ZCoord);
            cv::imshow("test", testMap.toMatrix());
            cv::waitKey();

            Matrix rotationCandidate = Procrustes3D::getOptimalRotation(sampledFace.points, referencePoints);
            Procrustes3D::transform(sampledFace.points, rotationCandidate);
            double d = Procrustes3D::diff(sampledFace.points, referencePoints);

            if (d < minD)
            {
                minD = d;
                minTheta = theta;
                rotation = rotationCandidate.clone();
            }
        }

        qDebug() << "theta" << minTheta;
        face.rotate(0, 0, -minTheta);
        face.transform(rotation);
    }

    return lm;
}
