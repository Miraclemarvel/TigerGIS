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
	//����������&͹������
	DataInput(QPlainTextEdit* pLogger, int nSelect, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr);
	//���ӷ���
	DataInput(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr, int nSelect = 0);
	~DataInput();
public slots:
	//����������
	int Buffer(double dRadius, GDALDataset* poDataset, const char* sFilePath);
	//͹������
	int Convex(GDALDataset* dataset, const char* sFilePath);
	//���ӷ���
	//����
	int IntersectLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath);
	//����
	int UnionLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath);
	//����
	int EraseLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath);
	//���ÿؼ��ɼ���
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
	QMap<QString, GDALDataset*> mDatasetMap;//�򿪵�ͼ�㼯��
	GDALDataset* mpDataset;					//��ǰ����ĵ����ݼ�
	int mnSelect;							//ѡ����
	logger* mpLog;							//�ռ������־
};

#endif // DATAINPUT_H