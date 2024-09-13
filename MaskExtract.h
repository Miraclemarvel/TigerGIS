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
	// ���캯��
	MaskExtract(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent = nullptr);
	~MaskExtract();

protected:
	// ʸ����Ĥ
	int applyVectorMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath);
	// դ����Ĥ
	int applyRasterMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath);

private:
	Ui::MaskExtractClass ui;
	QMap<QString, GDALDataset*> mDatasetMap;  // �򿪵����ݼ�����
	logger* mpLog;                            // դ�������־
};
