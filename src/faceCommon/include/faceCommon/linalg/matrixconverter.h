#pragma once

#include "common.h"
#include "vector.h"

namespace Face {
namespace LinAlg {

class MatrixConverter
{
public:
    static Matrix grayscaleImageToDoubleMatrix(const cv::Mat &in)
    {
        Matrix out(in.rows, in.cols);

        for (int r = 0; r < in.rows; r++)
        {
            for (int c = 0; c < in.cols; c++)
            {
                out(r, c) = in.at<unsigned char>(r, c)/255.0;
            }
        }
        return out;
    }

    static ImageGrayscale DoubleMatrixToGrayscaleImage(const Matrix &in)
    {
        ImageGrayscale out(in.rows, in.cols);
        for (int r = 0; r < in.rows; r++)
        {
            for (int c = 0; c < in.cols; c++)
            {
                out(r, c) = cv::saturate_cast<double>(in(r, c)*255.0);
            }
        }
        return out;
    }

    static Vector matrixToColumnVector(const Matrix &in)
    {
        Vector out(in.rows * in.cols);
        int i = 0;
        for (int r = 0; r < in.rows; r++)
        {
            for (int c = 0; c < in.cols; c++)
            {
                out(i) = in(r,c);
                i++;
            }
        }
        return out;
    }

    static Matrix columnVectorToMatrix(const Vector &in, int cols)
    {
        int rows = in.rows/cols;
        Matrix out(rows, cols, CV_64F);
        int i = 0;
        for (int r = 0; r < out.rows; r++)
        {
            for (int c = 0; c < out.cols; c++)
            {
                out(r,c) = in(i);
                i++;
            }
        }
        return out;
    }

    static Matrix imageToMatrix(const std::string &path)
    {
        cv::Mat img = cv::imread(path, CV_LOAD_IMAGE_GRAYSCALE);
        return MatrixConverter::grayscaleImageToDoubleMatrix(img);
    }

    static Matrix imageToColumnVector(const std::string &path)
    {
        Matrix m = imageToMatrix(path);
        return matrixToColumnVector(m);
    }

    static Matrix columnVectorsToDataMatrix(const std::vector<Vector> &vectors)
    {
        int cols = vectors.size();
        int rows = vectors[0].rows;

        Matrix data(rows, cols, CV_64F);

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                data(r,c) = vectors[c](r,0);
            }
        }

        return data;
    }

    static Matrix equalize(const Matrix &in)
    {
        ImageGrayscale gs = DoubleMatrixToGrayscaleImage(in);
        cv::equalizeHist(gs, gs);
        return grayscaleImageToDoubleMatrix(gs);
    }

    static Matrix scale(const Matrix &in, double scaleFactor)
    {
        cv::Size size(in.cols*scaleFactor, in.rows*scaleFactor);
        cv::Mat_<double> result(size);
        cv::resize(in, result, size);
        return result;
    }

    static std::vector<Vector> vectorizeAndScale(const std::vector<Matrix> &input, double scaleFactor)
    {
        std::vector<Vector> result;
        for (const Matrix &m : input)
        {
            result.push_back(matrixToColumnVector(scale(m, scaleFactor)));
        }
        return result;
    }
};

}
}
