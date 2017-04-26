#include "kinect/kinectsensorplugin.h"

#include <Poco/ClassLibrary.h>

#include "kinect/kinect.h"
#include "faceCommon/linalg/vector.h"
#include "faceCommon/linalg/matrixconverter.h"
#include "faceCommon/linalg/histogram.h"

using namespace Face::Sensors::Kinect;

KinectSensorPlugin::KinectSensorPlugin(const std::string &faceDetectorPath) :
    detector(new Face::ObjectDetection::OpenCVDetector(faceDetectorPath)),
    rotRect(cv::Point2f(320, 240), cv::Size2f(120, 160), 0),
    rect(320 - 60, 240 - 80, 120, 160),
    state(Waiting),
    _mesh(0),
    positionInterupted(false)
{
    WIDTH = 640;
    HEIGHT = 480;
    CHANNELS = 3;
    CONSECUTIVE_DETECTIONS = 10;
    CONSECUTIVE_NO_DATA = 50;
    DATA_RATIO = 0.5;
    POSITIONING_MAX_DISTANCE = 870;
    POSITIONING_MIN_DISTANCE = 830;
    MAX_GET_DATA_DISTANCE = 1000;

    rgbBuffer = new unsigned char[WIDTH * HEIGHT * CHANNELS];
    depthBuffer = new double[WIDTH * HEIGHT];
    depthMask = new bool[WIDTH * HEIGHT];

    img = ImageGrayscale::zeros(HEIGHT, WIDTH);
    Matrix maskMatrix = Matrix::zeros(HEIGHT, WIDTH);
    cv::ellipse(maskMatrix, rotRect, 1.0, CV_FILLED);
    ellipsePointsCount = 0;
    memset(depthMask, 0, sizeof(bool)*WIDTH*HEIGHT);
    for (int r = 240 - 80; r < 240 + 80; r++)
    {
        for (int c = 320 - 60; c < 320 + 60; c++)
        {
            if (maskMatrix(r, c))
            {
                depthMask[WIDTH*r+c] = true;
                ellipsePointsCount++;
            }
        }
    }
}

KinectSensorPlugin::~KinectSensorPlugin()
{
    delete rgbBuffer;
    delete depthBuffer;
    delete depthMask;
}

KinectSensorPlugin::PositionResponse KinectSensorPlugin::depthStats()
{
    int count = 0;
    double sum = 0.0;
    for (int i = 0; i < HEIGHT*WIDTH; i++)
    {
        if (depthBuffer[i])
        {
            count++;
            sum += depthBuffer[i];
        }
    }

    double ratio = (double)count/ellipsePointsCount;
    double mean = sum/count;

    //qDebug() << ratio << mean;
    if (ratio < DATA_RATIO) return NoData;
    if (mean > POSITIONING_MAX_DISTANCE) return MoveCloser;
    if (mean < POSITIONING_MIN_DISTANCE) return MoveFar;
    return Ok;

    //Kinect::depthToMatrix(depthBuffer, depth);
    //Vector depthVec = MatrixConverter::matrixToColumnVector(depth);
    //Histogram depthHistogram(depthVec.toQVector(), 3, true, 750.0, 1000.0, true);
    //Matrix histogramPlot = depthHistogram.plot();
    //cv::imshow("histogram", histogramPlot);
}

void KinectSensorPlugin::putText(const char *text)
{
    cv::putText(img, text, cv::Point(64, 240), CV_FONT_HERSHEY_SIMPLEX, 1.0, 0, 5);
    cv::putText(img, text, cv::Point(64, 240), CV_FONT_HERSHEY_SIMPLEX, 1.0, 255, 2);
}

void KinectSensorPlugin::getData()
{
    Kinect::getRGB(rgbBuffer);
    Kinect::getDepth(depthBuffer, depthMask, 0.0, MAX_GET_DATA_DISTANCE);
    Kinect::RGBToGrayscale(rgbBuffer, img);

    //Matrix d = Kinect::depthToMatrix(depthBuffer);
    //double min, max;
    //cv::minMaxIdx(d, &min, &max);
    //cv::imshow("depth", (d-min)/(max-min));
}

void KinectSensorPlugin::drawGUI()
{
    cv::flip(img, img, 1);
    cv::ellipse(img, rotRect, 0, 2, CV_AA);
    cv::ellipse(img, rotRect, 255, 1, CV_AA);
}

void KinectSensorPlugin::doLoop()
{
    //qDebug() << "go()";
    switch (state)
    {
    case Off:
        off();
        break;
    case Waiting:
        wait();
        break;
    case Positioning:
        position();
        break;
    case Capturing:
        capture();
        break;
    }
}

void KinectSensorPlugin::wait()
{
    //qDebug() << "wait()";
    positionInterupted = false;
    getData();
    PositionResponse response = depthStats();
    drawGUI();
    //img = ImageGrayscale::zeros(img.rows, img.cols);
    putText("Waiting for input");
    if (response == Ok || response == MoveCloser || response == MoveFar)
    {
        std::cout << "Positioning" << std::endl;
        state = Positioning;
    }
}

void KinectSensorPlugin::position()
{
    //qDebug() << "position()";
    positionInterupted = false;
    static int consecutiveDetections = 0;
    static int consecutiveNoData = 0;

    getData();
    PositionResponse response = depthStats();
    switch (response)
    {
    case NoData:
        drawGUI();
        consecutiveNoData++;
        if (consecutiveNoData == CONSECUTIVE_NO_DATA)
        {
            consecutiveNoData = 0;
            state = Waiting;
            positionInterupted = true;
        }
        //putText("Fit your entire face into the ellipse");
        break;
    case MoveCloser:
        drawGUI();
        putText("Move closer");
        consecutiveNoData = 0;
        break;
    case MoveFar:
        drawGUI();
        putText("Move far");
        consecutiveNoData = 0;
        break;
    case Ok:
        consecutiveNoData = 0;
        ImageGrayscale roi = img(rect);
        cv::equalizeHist(roi, roi);
        cv::Rect faceRegion = detector.detect(roi);
        drawGUI();
        if (faceRegion.width > 0)
        {
            consecutiveDetections++;
            if (consecutiveDetections < CONSECUTIVE_DETECTIONS) putText("Hold still");
            std::cout << "Consecutive face detections: " << consecutiveDetections << std::endl;
        }
        else
        {
            consecutiveDetections = 0;
        }

        if (consecutiveDetections == CONSECUTIVE_DETECTIONS)
        {
            putText("Capturing");
            consecutiveDetections = 0;
            state = Capturing;
        }

        break;
    }
}

void KinectSensorPlugin::capture()
{
    Kinect::getDepth(depthBuffer, 10, depthMask, 0.0, MAX_GET_DATA_DISTANCE);
    Kinect::getRGB(rgbBuffer);
    Kinect::RGBToGrayscale(rgbBuffer, img);

    _mesh = Kinect::createMesh(depthBuffer, rgbBuffer);
    _mesh->centralize();
    _mesh->calculateTriangles();

    //aligner.icpAlign(*mesh, 20);

    state = Waiting;
}

bool KinectSensorPlugin::isKinectPluggedIn()
{
    return Kinect::isKinectPluggedIn(depthBuffer);
}

void KinectSensorPlugin::deleteMesh()
{
    if (_mesh)
    {
        delete _mesh;
        _mesh = 0;
    }
}

void KinectSensorPlugin::scan()
{
    //qDebug() << "scanFace()";
    deleteMesh();

    while (!_mesh)
    {
        doLoop();
        cv::imshow("KinectSensorPlugin", img);
        char key = cv::waitKey(30);
        if (key != -1 || isPositionInterupted())
        {
            break;
        }
    }

    if (_mesh)
    {
        getData();
        drawGUI();
        putText("Processing");
    }
    cv::imshow("KinectSensorPlugin", img);
    cv::waitKey(1);
    cv::destroyWindow("KinectSensorPlugin");
}

POCO_BEGIN_MANIFEST(Face::Sensors::ISensor)
    POCO_EXPORT_CLASS(Face::Sensors::Kinect::KinectSensorPlugin)
POCO_END_MANIFEST
