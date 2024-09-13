#pragma once

#include <QDialog>
#include "ui_NeighborhoodStatistics.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "proj.h"
#include "logger.h"
#include <QPlainTextEdit>

class NeighborhoodStatistics : public QDialog
{
    Q_OBJECT

public:
    // 构造函数
    NeighborhoodStatistics(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr);
    ~NeighborhoodStatistics();

private:
    Ui::NeighborhoodStatisticsClass ui;
    QMap<QString, GDALDataset*> mDatasetMap;  // 打开的数据集集合
    logger* mpLog;                            // 栅格分析日志

    // 用于计算邻域平均值的函数
    int calculateMean(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth);

    // 用于计算邻域最大值的函数
    int calculateMaximum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth);

    // 用于计算邻域最小值的函数
    int calculateMinimum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth);
};