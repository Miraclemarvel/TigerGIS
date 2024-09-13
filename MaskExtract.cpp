#include "MaskExtract.h"
#include <QFileDialog>
#include "gdalwarper.h"
#include <QDebug>

// ���캯��
MaskExtract::MaskExtract(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget *parent)
	: QDialog(parent)
	, mDatasetMap(datasetMap)
	, mpLog(nullptr)
{
	ui.setupUi(this);
	// ��ʼ����־
	mpLog = new logger("��Ĥ��ȡ", pLogger);
	// ��������ͼ���б�
	QStringList layers = datasetMap.keys();
	QStringList rasterLayers;
	for (const auto& layer : layers) {
		if (layer.endsWith(".tif") || layer.endsWith(".TIF")) {
			rasterLayers << layer;
		}
	}
	ui.inputCombo->addItems(rasterLayers);
	// ������Ĥͼ���б�
	ui.maskCombo->addItems(layers);
	// ѡ�������ļ���
	connect(ui.inputFileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath1 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.tif *.TIF)");
		QStringList temp = sFilePath1.split("/");		//ͨ��·���ָ����ָ��ַ���
		QString basename = temp.at(temp.size() - 1);	//��ȡ�ļ���
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
	});
	// ѡ����Ĥ�ļ���
	connect(ui.maskFileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath2 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp *.tif *.TIF)");
		QStringList temp = sFilePath2.split("/");		//ͨ��·���ָ����ָ��ַ���
		QString basename = temp.at(temp.size() - 1);	//��ȡ�ļ���
		ui.maskCombo->addItem(basename);
		ui.maskCombo->setCurrentText(basename);
	});
	// ѡ������ļ���
	connect(ui.outputFileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath3 = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("����ļ�").toUtf8().constData()),
			"./output", "TIFF Files (*.tif)");
		ui.outputEdit->setText(sFilePath3);
	});
	// ���а�ť
	connect(ui.conductBtn, &QPushButton::clicked, [&]() {
		// ����դ��
		QString sInputLayer = ui.inputCombo->currentText();
		const char* sInputFilePath = mDatasetMap[sInputLayer]->GetDescription();
		// ��־���
		auto info = "���ڶ�ͼ��" + sInputLayer.toStdString() + "������Ĥ��ȡ...";
		mpLog->logInfo(info.c_str());
		// ������Ĥ
		QString sMaskLayer = ui.maskCombo->currentText();
		const char* sMaskFilePath = mDatasetMap[sMaskLayer]->GetDescription();
		// ����ļ�
		QString filePath = ui.outputEdit->text();
		QByteArray byteArray = filePath.toUtf8();
		const char* sOutputFilePath = byteArray.constData();
		// ִ����Ĥ��ȡ
		if (sMaskLayer.endsWith(".tif") || sMaskLayer.endsWith(".TIF")) {
			if (applyRasterMask(sInputFilePath, sMaskFilePath, sOutputFilePath) == 0) {
				mpLog->logInfo("��դ����Ĥ��ȡ�ɹ�");
			}
			else {
				mpLog->logWarnInfo("��դ����Ĥ��ȡʧ��");
			}
		}
		else {
			if (applyVectorMask(sInputFilePath, sMaskFilePath, sOutputFilePath) == 0) {
				mpLog->logInfo("��ʸ����Ĥ��ȡ�ɹ�");
			}
			else {
				mpLog->logWarnInfo("��ʸ����Ĥ��ȡʧ��");
			}
		}
		this->close();
	});
	// ȡ����ť,����ı���
	connect(ui.cancelBtn, &QPushButton::clicked, [&]() {
		ui.inputCombo->clear();
		ui.maskCombo->clear();
		ui.outputEdit->clear();
		mpLog->logInfo("ȡ����Ĥ��ȡ");
		this->close();
	});
}
// ��������
MaskExtract::~MaskExtract()
{}
// ʸ����Ĥ
int MaskExtract::applyVectorMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath) {
	// ��դ��ͼ��
	GDALDataset* poDataset = (GDALDataset*)GDALOpen(sInputRasterPath, GA_ReadOnly);
	if (poDataset == NULL) {
		mpLog->logErrorInfo("�޷�������դ��");
		return -1;
	}

	// �������դ���ļ�
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (poDriver == NULL) {
		mpLog->logErrorInfo("դ������������");
		GDALClose(poDataset);
		return -1;
	}

	GDALDataset* poOutputDS = poDriver->CreateCopy(sOutputRasterPath, poDataset, FALSE, NULL, NULL, NULL);
	if (poOutputDS == NULL) {
		mpLog->logErrorInfo("���դ�񴴽�ʧ��");
		GDALClose(poDataset);
		return -1;
	}

	// ��ʸ����Ĥͼ��
	GDALDataset* poMaskDS = (GDALDataset*)GDALOpenEx(sMaskPath, GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poMaskDS == NULL) {
		mpLog->logErrorInfo("�޷�����Ĥͼ��");
		GDALClose(poDataset);
		GDALClose(poOutputDS);
		return -1;
	}

	// ��ȡʸ��ͼ��
	OGRLayerH hLayer = GDALDatasetGetLayer(poMaskDS, 0);
	if (hLayer == NULL) {
		mpLog->logErrorInfo("�޷���ȡ��Ĥʸ��");
		GDALClose(poDataset);
		GDALClose(poOutputDS);
		GDALClose(poMaskDS);
		return -1;
	}
	// ��ȡդ��Ĳ�������
	int nBands = poDataset->GetRasterCount();
	// ���ò����б�
	int* anBandList = (int*)CPLMalloc(sizeof(int) * nBands);
	for (int i = 0; i < nBands; ++i) {
		anBandList[i] = i + 1;  // ���δ�1��ʼ���
	}

	// �������ֵ����������Ϊ0����ʾʸ����Χ��Ĳ��ֽ����ΪNoDataֵ
	double* adfBurnValues = (double*)CPLMalloc(sizeof(double) * nBands);
	for (int i = 0; i < nBands; ++i) {
		adfBurnValues[i] = 0;
	}

	// ����դ��ѡ��
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "ALL_TOUCHED", "TRUE");

	// դ��ʸ��ͼ�㵽���դ�����ݼ�
	CPLErr eErr = GDALRasterizeLayers((GDALDatasetH)poOutputDS, nBands, anBandList, 1, &hLayer,
		NULL, NULL, adfBurnValues, papszOptions, NULL, NULL);

	if (eErr != CE_None) {
		mpLog->logErrorInfo("��Ĥʸ��դ��ʧ��");
		CPLFree(anBandList);
		CPLFree(adfBurnValues);
		GDALClose(poDataset);
		GDALClose(poOutputDS);
		GDALClose(poMaskDS);
		return -1;
	}

	// ������Ĥ������ΪNoDataֵ
	int nXSize = poOutputDS->GetRasterXSize();
	int nYSize = poOutputDS->GetRasterYSize();
	for (int iBand = 1; iBand <= nBands; ++iBand) {
		GDALRasterBand* poOutputBand = poOutputDS->GetRasterBand(iBand);
		GDALRasterBand* poInputBand = poDataset->GetRasterBand(iBand);
		unsigned char* pafData = (unsigned char*)CPLMalloc(sizeof(unsigned char) * nXSize * nYSize);
		unsigned char* srcData = (unsigned char*)CPLMalloc(sizeof(unsigned char) * nXSize * nYSize);

		poOutputBand->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pafData, nXSize, nYSize, GDT_Byte, 0, 0);
		poInputBand->RasterIO(GF_Read, 0, 0, nXSize, nYSize, srcData, nXSize, nYSize, GDT_Byte, 0, 0);

		for (int i = 0; i < nXSize * nYSize; i++) {
			if (pafData[i] != 0) {  // ��Ĥ������
				pafData[i] = static_cast<unsigned char>(255.0);  // ����Ϊ0��NoDataֵ
			}
			else {
				pafData[i] = srcData[i];
			}
		}

		poOutputBand->RasterIO(GF_Write, 0, 0, nXSize, nYSize, pafData, nXSize, nYSize, GDT_Byte, 0, 0);

		CPLFree(pafData);
	}

	CPLFree(anBandList);
	CPLFree(adfBurnValues);

	// �ر����ݼ�
	GDALClose(poDataset);
	GDALClose(poOutputDS);
	GDALClose(poMaskDS);
	return 0;
}
// դ����Ĥ
int MaskExtract::applyRasterMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath) {
	// ������դ��
	GDALDataset* poDataset = (GDALDataset*)GDALOpen(sInputRasterPath, GA_ReadOnly);
	if (poDataset == NULL) {
		mpLog->logErrorInfo("�޷�������դ��");
		return -1;
	}

	// ����Ĥդ��
	GDALDataset* poMask = (GDALDataset*)GDALOpen(sMaskPath, GA_ReadOnly);
	if (poMask == NULL) {
		mpLog->logErrorInfo("�޷�����Ĥդ��");
		GDALClose(poDataset);
		return -1;
	}

	// ��ȡ����դ��ĳߴ�͵���ο���Ϣ
	int inputWidth = poDataset->GetRasterXSize();
	int inputHeight = poDataset->GetRasterYSize();
	double transform[6];
	poDataset->GetGeoTransform(transform);

	// ʹ�� GDALWarp ����Ĥͼ����뵽����ͼ��
	GDALDataset* poAlignedMask = static_cast<GDALDataset*>(GDALAutoCreateWarpedVRT(poMask, NULL, poDataset->GetProjectionRef(), GRA_NearestNeighbour, 0.0, NULL));

	// ��ȡ��������Ĥ��������
	GDALRasterBand* alignedMaskBandPtr = poAlignedMask->GetRasterBand(1);
	std::vector<uint8_t> maskBuffer(inputWidth * inputHeight);
	alignedMaskBandPtr->RasterIO(GF_Read, 0, 0, inputWidth, inputHeight, maskBuffer.data(), inputWidth, inputHeight, GDT_Byte, 0, 0);

	// �������դ��
	GDALDataset* poOutput = GetGDALDriverManager()->GetDriverByName("GTiff")->Create(sOutputRasterPath, inputWidth, inputHeight, poDataset->GetRasterCount(), GDT_Byte, NULL);
	if (poOutput == NULL) {
		mpLog->logErrorInfo("���դ�񴴽�ʧ��");
		GDALClose(poDataset);
		GDALClose(poMask);
		GDALClose(poAlignedMask);
		return -1;
	}

	poOutput->SetGeoTransform(transform);
	poOutput->SetProjection(poDataset->GetProjectionRef());



	// ��ÿ�����ν�����Ĥ����
	for (int band = 1; band <= poDataset->GetRasterCount(); band++) {
		GDALRasterBand* inputBandPtr = poDataset->GetRasterBand(band);
		GDALRasterBand* outputBandPtr = poOutput->GetRasterBand(band);

		std::vector<uint8_t> inputBuffer(inputWidth * inputHeight);
		std::vector<uint8_t> outputBuffer(inputWidth * inputHeight);

		inputBandPtr->RasterIO(GF_Read, 0, 0, inputWidth, inputHeight, inputBuffer.data(), inputWidth, inputHeight, GDT_Byte, 0, 0);

		for (int i = 0; i < inputHeight * inputWidth; i++) {
			if (maskBuffer[i] != 255.0) { // ֻ������ĤΪ���������
				outputBuffer[i] = inputBuffer[i];
			}
			else {
				outputBuffer[i] = 255.0; // ��ĤΪ0��������Ϊ��Чֵ
			}
		}
		outputBandPtr->RasterIO(GF_Write, 0, 0, inputWidth, inputHeight, outputBuffer.data(), inputWidth, inputHeight, GDT_Byte, 0, 0);
	}

	// �ر����ݼ�
	GDALClose(poDataset);
	GDALClose(poMask);
	//GDALClose(poAlignedMask);
	GDALClose(poOutput);
	return 0;
}