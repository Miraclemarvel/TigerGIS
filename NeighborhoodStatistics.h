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
    // ���캯��
    NeighborhoodStatistics(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr);
    ~NeighborhoodStatistics();

private:
    Ui::NeighborhoodStatisticsClass ui;
    QMap<QString, GDALDataset*> mDatasetMap;  // �򿪵����ݼ�����
    logger* mpLog;                            // դ�������־

    // ���ڼ�������ƽ��ֵ�ĺ���
    int calculateMean(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth);

    // ���ڼ����������ֵ�ĺ���
    int calculateMaximum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth);

    // ���ڼ���������Сֵ�ĺ���
    int calculateMinimum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth);
};