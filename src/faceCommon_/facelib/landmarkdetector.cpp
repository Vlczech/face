#include <QDateTime>
#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

#include "landmarkdetector.h"
#include "util.h"
#include "map.h"
#include "maskedvector.h"
#include "linalg/vector.h"
#include "linalg/kernelgenerator.h"

LandmarkDetector::LandmarkDetector(Mesh &mesh) : mesh(mesh)
{
    stripeWidth = 50; //100;
    //depthSmoothIterations = 20; // 20;
    //depthSmoothAlpha = 0.5; //1.0;
    depthGaussSize = 7;
    depthGaussIterations = 3;
    depthErode = 7;
    depthLevelSelect = 0;
    depthScale = 1; //2;
    peakDensityWindowsSize = 21; //10;
    //pitDensityWindowsSize = 11; //5;
    pitsStripeSmoothKernel = 7; //11;
    minYDistanceFromNosetipToEyes = 20; //40;
    maxYDistanceFromNosetipToEyes = 70; //140;
    minXDistanceFromNosetipToEyes = 12; //25;
    maxXDistanceFromNosetipToEyes = 20; //40;

    // Convert to range image
    depth = SurfaceProcessor::depthmap(mesh, converter, depthScale, ZCoord);

    // smooth
    Matrix smoothKernel = KernelGenerator::gaussianKernel(depthGaussSize);
    depth.applyFilter(smoothKernel, depthGaussIterations, true);
    //for (int i = 0; i < depthGaussIterations; i++)
    //    cv::GaussianBlur(depth.values, depth.values, cv::Size(depthGaussSize, depthGaussSize), -1);

    //cv::imshow("kernel", smoothKernel);
    //cv::imshow("smoothed", depth.toMatrix());

    // select only points with z-coordinate higher or equal than some threshold
    depth.levelSelect((mesh.maxz+mesh.minz)/2);
    int cropWidth, cropHeight;
    depth.getCropParams(cropStartX, cropWidth, cropStartY, cropHeight);
    croppedDepth = depth.subMap(cropStartX, cropWidth, cropStartY, cropHeight);

    //cv::imshow("depth", croppedDepth.toMatrix());
    //cv::waitKey();

    // calculate curvature
    curvature = SurfaceProcessor::calculateCurvatures(croppedDepth, false);

    // density maps
    peakDensity = curvature.peaks.densityMap(peakDensityWindowsSize, true);
    //pitDensity = curvature.pits.densityMap(pitDensityWindowsSize, false);

    /*cv::imshow("depth", depth.toMatrix());
    cv::imshow("croppedDepth", croppedDepth.toMatrix());
    cv::imshow("curvature", curvature.curvatureIndex.toMatrix());
    cv::imshow("peaks", curvature.peaks.toMatrix(0, 0, 1));
    cv::imshow("pits", curvature.pits.toMatrix(0, 0, 1));
    cv::imshow("peakDensity", peakDensity.toMatrix(0, 0, 1));
    //cv::imshow("pitDensity", pitDensity.toMatrix(0, 0, 1));
    cv::waitKey();*/
}

Landmarks LandmarkDetector::detect()
{
    Landmarks l;

    nosetip(l);
    //innerEyeCorners(l);

    return l;
}

void LandmarkDetector::nosetip(Landmarks &l)
{
    //qDebug() << "  nose tip";
    int x = -1;
    int y = -1;
    peakDensity.maxIndex(x, y);
    if (x == -1 && y == -1)
    {
        qDebug() << "Nose coordinates not found";
        return;
    }

    bool ok;
    cv::Point3d nosetip = converter.MapToMeshCoords(depth, cv::Point2d(x + cropStartX, y + cropStartY), &ok);
    if (ok)
    {
        l.set(Landmarks::Nosetip, nosetip);
    }
    else
    {
        qDebug() << "Nose coordinates not found";
    }
}

void LandmarkDetector::innerEyeCorners(Landmarks &l)
{
    cv::Point3d nosetip = l.get(Landmarks::Nosetip);
    cv::Point2d nose2d = converter.MeshToMapCoords(depth, cv::Point2d(nosetip.x, nosetip.y));
    nose2d.x -= cropStartX;
    nose2d.y -= cropStartY;

    // Y coordinate
    int h = depth.h;
    Vector pitsAlongY(h);
    for (int y = 0; y < h; y++)
    {
        int count = 0;
        for (int x = nose2d.x - stripeWidth/2; x <= nose2d.x + stripeWidth/2; x++)
        {
            count = curvature.pits.isSet(x, y) ? count+1 : count;
        }
        pitsAlongY(y) = count;
    }
    Vector smoothedPits = pitsAlongY.smooth(pitsStripeSmoothKernel);

    int eyes2dY = smoothedPits.maxIndex(nose2d.y - maxYDistanceFromNosetipToEyes,
                                        nose2d.y - minYDistanceFromNosetipToEyes);
    if (eyes2dY < 0)
    {
        qDebug() << "Y coordinate of inner eye corners not found";
        return;
    }

    // X coordinate
    int w = depth.w;
    Vector pitsAlongX(w);
    for (int x = 0; x < w; x++)
    {
        int count = 0;
        for (int y = eyes2dY - stripeWidth/2; y <= eyes2dY + stripeWidth/2; y++)
        {
            count = curvature.pits.isSet(x,y) ? count+1 : count;
        }
        pitsAlongX(x) = count;
    }
    smoothedPits = pitsAlongX.smooth(pitsStripeSmoothKernel);

    //depth.horizontalProfile(eyes2dY).savePlot("../test/horizontalProfile");
    //Vector::toFileWithIndicies(smoothedPits, "../test/pitsAlongX");

    int leftEye2dX = smoothedPits.maxIndex(nose2d.x - maxXDistanceFromNosetipToEyes,
                                           nose2d.x - minXDistanceFromNosetipToEyes);
    if (leftEye2dX < 0)
    {
        qDebug() << "X coordinate of left inner eye corner not found";
    }
    else
    {
        l.set(Landmarks::LeftInnerEye,
              converter.MapToMeshCoords(depth, cv::Point2d(leftEye2dX + cropStartX, eyes2dY + cropStartY)));
    }

    int rightEye2dX = smoothedPits.maxIndex(nose2d.x + minXDistanceFromNosetipToEyes,
                                            nose2d.x + maxXDistanceFromNosetipToEyes);

    if (rightEye2dX < 0)
    {
        qDebug() << "X coordinate of right inner eye corner not found";
    }
    else
    {
        l.set(Landmarks::RightInnerEye,
              converter.MapToMeshCoords(depth, cv::Point2d(rightEye2dX + cropStartX, eyes2dY + cropStartY)));
    }
}

/*void LandmarkDetector::InnerEyeCorners(Landmarks &l)
{
    cv::Point3d nosetip = l.get(Landmarks::Nosetip);
    cv::Point2d nose2d = converter.MeshToMapCoords(depth, cv::Point2d(nosetip.x, nosetip.y));

    // left eye
    Map leftEyeArea = pitDensity.reshape(0, nose2d.x, 0, nose2d.y);
    cv::imshow("left", leftEyeArea.toMatrix(0, 0, 1));
    int i = leftEyeArea.maxIndex();
    int x = leftEyeArea.indexToX(i);
    int y = leftEyeArea.indexToY(i);
    cv::Point3d leftInnerEye = converter.MapToMeshCoords(depth, cv::Point2d(x, y));
    l.set(Landmarks::LeftInnerEye, leftInnerEye);

    // right eye
    Map rightEyeArea = pitDensity.reshape(nose2d.x, pitDensity.w - nose2d.x, 0, nose2d.y);
    cv::imshow("right", rightEyeArea.toMatrix(0, 0, 1));
    i = rightEyeArea.maxIndex();
    x = rightEyeArea.indexToX(i) + nose2d.x;
    y = rightEyeArea.indexToY(i);
    cv::Point3d rightInnerEye = converter.MapToMeshCoords(depth, cv::Point2d(x, y));
    l.set(Landmarks::RightInnerEye, rightInnerEye);

    //cv::imshow("both", pitDensity.toMatrix(0,0,1));
}*/