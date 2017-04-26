#include "softkinetic/ds325sensorfactory.h"
#include "softkinetic/ds325sensor.h"
#include "softkinetic/ds325sensor2.h"

using namespace Face::Sensors::SoftKinetic;

DS325SensorFactory::DS325SensorFactory()
{
    context = DepthSense::Context::createStandalone();
}
DS325SensorFactory::~DS325SensorFactory(){

}

std::vector<DepthSense::Device> DS325SensorFactory::getConnectedDevices()
{
    return context.getDevices();
}

IDS325Sensor::Ptr DS325SensorFactory::create(int sensorCount, const std::string &haarFaceDetectionPath, const std::string &landmarkDetectorPath, const Settings &settings)
{
    if (sensorCount == 1) return new DS325Sensor(haarFaceDetectionPath, landmarkDetectorPath, settings, context);
    else if (sensorCount == 2) return new DS325Sensor2(haarFaceDetectionPath, landmarkDetectorPath, settings, context);

    throw FACELIB_EXCEPTION("Unknown sensor count: " + std::to_string(sensorCount));
}

IDS325Sensor::Ptr DS325SensorFactory::create(const std::string &haarFaceDetectionPath, const std::string &landmarkDetectorPath, const Settings &settings)
{
    return create(getConnectedDevices().size(), haarFaceDetectionPath, landmarkDetectorPath, settings);
}

Face::Sensors::SensorData IDS325Sensor::sensorData()
{
    Face::Sensors::SensorData result;
    result.processedScan.mesh = faceMesh;
    result.processedScan.landmarks = landmarks;
    result.processedScan.texture = ImageGrayscale::zeros(1,1);
    return result;
}
