#pragma once
#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <QDialog>
#include "ui_AttributeWidget.h"
#include "gdal.h"
#include "gdal_priv.h"
#include <ogrsf_frmts.h>
#include "logger.h"

class AttributeWidget : public QDialog
{
	Q_OBJECT

public:
	AttributeWidget(logger* pLog, int nSelect, QWidget* parent = nullptr, GDALDataset* dataset = nullptr);
	~AttributeWidget();

public slots:
	//ͳ������
	int calculateNum(GDALDataset* dataset);
	//ͳ�����
	int calculateArea(GDALDataset* dataset);
	//ͳ���ܳ�
	int calculatePerimeter(GDALDataset* dataset);
	//ͳ�Ƴ���
	int calculateLength(GDALDataset* dataset);

private:
	Ui::AttributeWidgetClass ui;
};
#endif // ATTRIBUTE_H