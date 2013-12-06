#ifndef LOADER_H
#define LOADER_H

#include <QVector>
#include <QDir>
#include <QStringList>
#include <QDebug>
#include <QString>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include "linalg/vector.h"
#include "matrixconverter.h"
#include "facelib/map.h"

enum PathType { AbsoluteFull, Filename, baseFilename };

class Loader
{
public:
    static QVector<Vector> loadShapes(const QString &dirPath);

    static QVector<Matrix> loadImages(const QString &dirPath);

    static void loadImages(const QString &dirPath, QVector<Matrix> &images, QVector<int> *classes = 0,
                           const char *extensionFilter = "*.png",
                           const char *classSeparator = "-",
                           int maxImages = -1,
                           cv::Rect roi = cv::Rect(),
                           bool qdebug = false);

    static void loadImages(const QString &dirPath, QVector<Vector> &vectors, QVector<int> *classes = 0,
                           const char *extensionFilter = "*.png",
                           const char *classSeparator = "-",
                           int maxImages = -1,
                           cv::Rect roi = cv::Rect(),
                           bool qdebug = false);

    static void loadMaps(const QString &dirPath, QVector<Map>, QVector<int> *classes = 0,
                         const char *extensionFilter = "*.map",
                         const char *classSeparator = "-", bool qdebug = false);

    static void loadVectors(const QString &dirPath, QVector<Vector> &vectors, QVector<int> &classes,
                            const char *classSeparator = "-", const char *nameFilter = "*");

    static void loadMatrices(const QString &dirPath, QVector<Matrix> &matrices, QVector<int> &classes,
                             const char *classSeparator = "-", const char *nameFilter = "*", int maxCount = -1);

    static QVector<QString> listFiles(const QString &path, const QString &filter, PathType pathType);
};

#endif // LOADER_H