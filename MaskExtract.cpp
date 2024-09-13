#include "MaskExtract.h"
#include <QFileDialog>
#include "gdalwarper.h"
#include <QDebug>

// 构造函数
MaskExtract::MaskExtract(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget *parent)
	: QDialog(parent)
	, mDatasetMap(datasetMap)
	, mpLog(nullptr)
{
	ui.setupUi(this);
	// 初始化日志
	mpLog = new logger("掩膜提取", pLogger);
	// 更新输入图层列表
	QStringList layers = datasetMap.keys();
	QStringList rasterLayers;
	for (const auto& layer : layers) {
		if (layer.endsWith(".tif") || layer.endsWith(".TIF")) {
			rasterLayers << layer;
		}
	}
	ui.inputCombo->addItems(rasterLayers);
	// 更新掩膜图层列表
	ui.maskCombo->addItems(layers);
	// 选择输入文件夹
	connect(ui.inputFileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath1 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.tif *.TIF)");
		QStringList temp = sFilePath1.split("/");		//通过路径分隔符分隔字符串
		QString basename = temp.at(temp.size() - 1);	//获取文件名
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
	});
	// 选择掩膜文件夹
	connect(ui.maskFileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath2 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp *.tif *.TIF)");
		QStringList temp = sFilePath2.split("/");		//通过路径分隔符分隔字符串
		QString basename = temp.at(temp.size() - 1);	//获取文件名
		ui.maskCombo->addItem(basename);
		ui.maskCombo->setCurrentText(basename);
	});
	// 选择输出文件夹
	connect(ui.outputFileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath3 = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("输出文件").toUtf8().constData()),
			"./output", "TIFF Files (*.tif)");
		ui.outputEdit->setText(sFilePath3);
	});
	// 运行按钮
	connect(ui.conductBtn, &QPushButton::clicked, [&]() {
		// 输入栅格
		QString sInputLayer = ui.inputCombo->currentText();
		const char* sInputFilePath = mDatasetMap[sInputLayer]->GetDescription();
		// 日志输出
		auto info = "正在对图层" + sInputLayer.toStdString() + "进行掩膜提取...";
		mpLog->logInfo(info.c_str());
		// 输入掩膜
		QString sMaskLayer = ui.maskCombo->currentText();
		const char* sMaskFilePath = mDatasetMap[sMaskLayer]->GetDescription();
		// 输出文件
		QString filePath = ui.outputEdit->text();
		QByteArray byteArray = filePath.toUtf8();
		const char* sOutputFilePath = byteArray.constData();
		// 执行掩膜提取
		if (sMaskLayer.endsWith(".tif") || sMaskLayer.endsWith(".TIF")) {
			if (applyRasterMask(sInputFilePath, sMaskFilePath, sOutputFilePath) == 0) {
				mpLog->logInfo("按栅格掩膜提取成功");
			}
			else {
				mpLog->logWarnInfo("按栅格掩膜提取失败");
			}
		}
		else {
			if (applyVectorMask(sInputFilePath, sMaskFilePath, sOutputFilePath) == 0) {
				mpLog->logInfo("按矢量掩膜提取成功");
			}
			else {
				mpLog->logWarnInfo("按矢量掩膜提取失败");
			}
		}
		this->close();
	});
	// 取消按钮,清空文本框
	connect(ui.cancelBtn, &QPushButton::clicked, [&]() {
		ui.inputCombo->clear();
		ui.maskCombo->clear();
		ui.outputEdit->clear();
		mpLog->logInfo("取消掩膜提取");
		this->close();
	});
}
// 析构函数
MaskExtract::~MaskExtract()
{}
// 矢量掩膜
int MaskExtract::applyVectorMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath) {
	// 打开栅格图层
	GDALDataset* poDataset = (GDALDataset*)GDALOpen(sInputRasterPath, GA_ReadOnly);
	if (poDataset == NULL) {
		mpLog->logErrorInfo("无法打开输入栅格");
		return -1;
	}

	// 创建输出栅格文件
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (poDriver == NULL) {
		mpLog->logErrorInfo("栅格驱动不可用");
		GDALClose(poDataset);
		return -1;
	}

	GDALDataset* poOutputDS = poDriver->CreateCopy(sOutputRasterPath, poDataset, FALSE, NULL, NULL, NULL);
	if (poOutputDS == NULL) {
		mpLog->logErrorInfo("输出栅格创建失败");
		GDALClose(poDataset);
		return -1;
	}

	// 打开矢量掩膜图层
	GDALDataset* poMaskDS = (GDALDataset*)GDALOpenEx(sMaskPath, GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poMaskDS == NULL) {
		mpLog->logErrorInfo("无法打开掩膜图层");
		GDALClose(poDataset);
		GDALClose(poOutputDS);
		return -1;
	}

	// 获取矢量图层
	OGRLayerH hLayer = GDALDatasetGetLayer(poMaskDS, 0);
	if (hLayer == NULL) {
		mpLog->logErrorInfo("无法获取掩膜矢量");
		GDALClose(poDataset);
		GDALClose(poOutputDS);
		GDALClose(poMaskDS);
		return -1;
	}
	// 获取栅格的波段数量
	int nBands = poDataset->GetRasterCount();
	// 设置波段列表
	int* anBandList = (int*)CPLMalloc(sizeof(int) * nBands);
	for (int i = 0; i < nBands; ++i) {
		anBandList[i] = i + 1;  // 波段从1开始编号
	}

	// 设置填充值，这里设置为0，表示矢量范围外的部分将填充为NoData值
	double* adfBurnValues = (double*)CPLMalloc(sizeof(double) * nBands);
	for (int i = 0; i < nBands; ++i) {
		adfBurnValues[i] = 0;
	}

	// 设置栅格化选项
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "ALL_TOUCHED", "TRUE");

	// 栅格化矢量图层到输出栅格数据集
	CPLErr eErr = GDALRasterizeLayers((GDALDatasetH)poOutputDS, nBands, anBandList, 1, &hLayer,
		NULL, NULL, adfBurnValues, papszOptions, NULL, NULL);

	if (eErr != CE_None) {
		mpLog->logErrorInfo("掩膜矢量栅格化失败");
		CPLFree(anBandList);
		CPLFree(adfBurnValues);
		GDALClose(poDataset);
		GDALClose(poOutputDS);
		GDALClose(poMaskDS);
		return -1;
	}

	// 设置掩膜外区域为NoData值
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
			if (pafData[i] != 0) {  // 掩膜外区域
				pafData[i] = static_cast<unsigned char>(255.0);  // 设置为0或NoData值
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

	// 关闭数据集
	GDALClose(poDataset);
	GDALClose(poOutputDS);
	GDALClose(poMaskDS);
	return 0;
}
// 栅格掩膜
int MaskExtract::applyRasterMask(const char* sInputRasterPath, const char* sMaskPath, const char* sOutputRasterPath) {
	// 打开输入栅格
	GDALDataset* poDataset = (GDALDataset*)GDALOpen(sInputRasterPath, GA_ReadOnly);
	if (poDataset == NULL) {
		mpLog->logErrorInfo("无法打开输入栅格");
		return -1;
	}

	// 打开掩膜栅格
	GDALDataset* poMask = (GDALDataset*)GDALOpen(sMaskPath, GA_ReadOnly);
	if (poMask == NULL) {
		mpLog->logErrorInfo("无法打开掩膜栅格");
		GDALClose(poDataset);
		return -1;
	}

	// 获取输入栅格的尺寸和地理参考信息
	int inputWidth = poDataset->GetRasterXSize();
	int inputHeight = poDataset->GetRasterYSize();
	double transform[6];
	poDataset->GetGeoTransform(transform);

	// 使用 GDALWarp 将掩膜图层对齐到输入图层
	GDALDataset* poAlignedMask = static_cast<GDALDataset*>(GDALAutoCreateWarpedVRT(poMask, NULL, poDataset->GetProjectionRef(), GRA_NearestNeighbour, 0.0, NULL));

	// 读取对齐后的掩膜波段数据
	GDALRasterBand* alignedMaskBandPtr = poAlignedMask->GetRasterBand(1);
	std::vector<uint8_t> maskBuffer(inputWidth * inputHeight);
	alignedMaskBandPtr->RasterIO(GF_Read, 0, 0, inputWidth, inputHeight, maskBuffer.data(), inputWidth, inputHeight, GDT_Byte, 0, 0);

	// 创建输出栅格
	GDALDataset* poOutput = GetGDALDriverManager()->GetDriverByName("GTiff")->Create(sOutputRasterPath, inputWidth, inputHeight, poDataset->GetRasterCount(), GDT_Byte, NULL);
	if (poOutput == NULL) {
		mpLog->logErrorInfo("输出栅格创建失败");
		GDALClose(poDataset);
		GDALClose(poMask);
		GDALClose(poAlignedMask);
		return -1;
	}

	poOutput->SetGeoTransform(transform);
	poOutput->SetProjection(poDataset->GetProjectionRef());



	// 对每个波段进行掩膜操作
	for (int band = 1; band <= poDataset->GetRasterCount(); band++) {
		GDALRasterBand* inputBandPtr = poDataset->GetRasterBand(band);
		GDALRasterBand* outputBandPtr = poOutput->GetRasterBand(band);

		std::vector<uint8_t> inputBuffer(inputWidth * inputHeight);
		std::vector<uint8_t> outputBuffer(inputWidth * inputHeight);

		inputBandPtr->RasterIO(GF_Read, 0, 0, inputWidth, inputHeight, inputBuffer.data(), inputWidth, inputHeight, GDT_Byte, 0, 0);

		for (int i = 0; i < inputHeight * inputWidth; i++) {
			if (maskBuffer[i] != 255.0) { // 只保留掩膜为非零的区域
				outputBuffer[i] = inputBuffer[i];
			}
			else {
				outputBuffer[i] = 255.0; // 掩膜为0的区域设为无效值
			}
		}
		outputBandPtr->RasterIO(GF_Write, 0, 0, inputWidth, inputHeight, outputBuffer.data(), inputWidth, inputHeight, GDT_Byte, 0, 0);
	}

	// 关闭数据集
	GDALClose(poDataset);
	GDALClose(poMask);
	//GDALClose(poAlignedMask);
	GDALClose(poOutput);
	return 0;
}