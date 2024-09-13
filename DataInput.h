#pragma once
//DataInput.h
#ifndef DATAINPUT_H
#define DATAINPUT_H

#include <QDialog>
#include "ui_DataInput.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "proj.h"
#include <QGraphicsItem>
#include "logger.h"
class DataInput : public QDialog
{
	Q_OBJECT

public:
	//缓冲区分析&凸包计算
	DataInput(QPlainTextEdit* pLogger, int nSelect, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr);
	//叠加分析
	DataInput(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr, int nSelect = 0);
	~DataInput();
public slots:
	//缓冲区分析
	int Buffer(double dRadius, GDALDataset* poDataset, const char* sFilePath);
	//凸包分析
	int Convex(GDALDataset* dataset, const char* sFilePath);
	//叠加分析
	//交集
	int IntersectLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath);
	//并集
	int UnionLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath);
	//擦除
	int EraseLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath);
	//设置控件可见度
	void setRadiusLabelVisible(bool visible);
	void setBufferEditVisible(bool visible);
	void setOpenLabelVisible_1(bool visible);
	void setOpenLabelVisible_2(bool visible);
	void setOverlayComboVisible(bool visible);
	void setinputComboVisible(bool visible);
	void setFileButtonVisible_1(bool visible);
	void setFileButtonVisible_2(bool visible);

private:
	Ui::DataInputClass ui;
	QMap<QString, GDALDataset*> mDatasetMap;//打开的图层集合
	GDALDataset* mpDataset;					//当前处理的的数据集
	int mnSelect;							//选择功能
	logger* mpLog;							//空间分析日志
};

#endif // DATAINPUT_H