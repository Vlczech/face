#ifndef EVALUATESOFTKINETIC_H
#define EVALUATESOFTKINETIC_H

#include "faceCommon/biometrics/multiextractor.h"
#include "faceCommon/facedata/facealigner.h"
#include "faceCommon/biometrics/zpcacorrw.h"

class EvaluateSoftKinetic
{
private:
    std::map<int, std::vector<Face::Biometrics::MultiTemplate>> gallery;
    std::vector<Face::Biometrics::MultiTemplate> probes;

    static Face::Biometrics::ZPCACorrW getFRGCExtractor();
    static std::vector<Face::LinAlg::Vector> smoothMeshes(const std::vector<Face::FaceData::Mesh> &in, float coef, int smoothIters,
		const Face::FaceData::FaceAlignerIcp &aligner, int icpIters, bool useMdenoise);

public:
    Face::Biometrics::MultiExtractor extractor;
    Face::FaceData::FaceAlignerIcp aligner;

    EvaluateSoftKinetic();
    void loadDirectory();
    void loadDirectory(const std::string &dirPath, unsigned int galleryCount, bool randomize);
    void evaluatePerformance();
    void evaluateMetrics();

    static void evaluateOptimalSmoothing();
    static void stats();
    static void evaluateAging();
    static void problematicScans();

    static void evaluateAlignBaseline();
    static void evaluateCVAlign();
    static void deviceOpenCVtest();

    static void evaluateVariability();

    static int evaluateJenda(int argc, char *argv[]);

private:
    template<typename T>
    std::string outScores(const std::vector<T> &in)
    {
        std::stringstream ss;
        for (T d : in)
        {
            ss << d << "; ";
        }
        return ss.str();
    }
};

#endif // EVALUATESOFTKINETIC_H
