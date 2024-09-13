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
	//统计数量
	int calculateNum(GDALDataset* dataset);
	//统计面积
	int calculateArea(GDALDataset* dataset);
	//统计周长
	int calculatePerimeter(GDALDataset* dataset);
	//统计长度
	int calculateLength(GDALDataset* dataset);

private:
	Ui::AttributeWidgetClass ui;
};
#endif // ATTRIBUTE_H