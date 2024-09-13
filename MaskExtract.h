#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include "ui_MaskExtract.h"
#include "logger.h"
#include "gdal.h"
#include "gdal_priv.h"

class MaskExtract : public QDialog
{
	Q_OBJECT

public:
	// 构造函数
	MaskExtract(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr);
	~MaskExtract();

protected:
	// 矢量掩膜
	int applyVectorMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath);
	// 栅格掩膜
	int applyRasterMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath);

private:
	Ui::MaskExtractClass ui;
	QMap<QString, GDALDataset*> mDatasetMap;  // 打开的数据集集合
	logger* mpLog;                            // 栅格分析日志
};
