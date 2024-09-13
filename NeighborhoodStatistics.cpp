#include "NeighborhoodStatistics.h"
#include <QFileDialog>
#include <QDebug>
#include <QPushButton>
#include <vector>
#include <algorithm>
#include <gdal_alg.h>
#include <thread>
#include <mutex>

std::mutex gdalMutex;

NeighborhoodStatistics::NeighborhoodStatistics(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent)
    : QDialog(parent)
    , mDatasetMap(datasetMap)
    , mpLog(nullptr)
{
    ui.setupUi(this);
    // 初始化日志
    mpLog = new logger("栅格分析", pLogger);
	//更新输入图层列表
	QStringList layers = datasetMap.keys();
	QStringList rasterLayers;
	for (const auto& layer : layers) {
		if (layer.endsWith(".tif") || layer.endsWith(".TIF")) {
			rasterLayers << layer;
		}
	}
	ui.inputCombo->addItems(rasterLayers);

    // 选择输入文件
    connect(ui.openPushButton, &QPushButton::clicked, [&]() {
        auto sFilePath1 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.tif *.TIF)");
		QStringList temp = sFilePath1.split("/");					//通过路径分隔符分隔字符串
		QString basename = temp.at(temp.size() - 1);	//获取文件名
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
        });

    // 选择输出文件夹
    connect(ui.outputPushButton, &QPushButton::clicked, [&]() {
        auto sFilePath2 = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("输出文件").toUtf8().constData()),
            "./output", "TIFF Files (*.tif)");
        ui.outputLineEdit->setText(sFilePath2);
        });

    // 确定按钮执行邻域统计
    connect(ui.surePushButton, &QPushButton::clicked, [&]() {
        // 输入栅格
        QString sLayerName = ui.inputCombo->currentText();
        const char* sFilePath1 = mDatasetMap[sLayerName]->GetDescription();
        // 日志输出
        auto info = "正在对图层" + sLayerName.toStdString() + "进行领域分析...";
        mpLog->logInfo(info.c_str());
        // 输出文件
        QString filePath2 = ui.outputLineEdit->text();
        QByteArray byteArray2 = filePath2.toUtf8();
        const char* sFilePath2 = byteArray2.constData();

        // 高度和宽度
        int nHeight = ui.heightLineEdit->text().toUtf8().toInt();
        int nWidth = ui.widthLineEdit->text().toUtf8().toInt();

        // 获取选择的统计类型
        QString statType = ui.comboBox->currentText();

        // 根据选择的统计类型执行对应的函数
        if (statType == "MEAN") {
            if (calculateMean(sFilePath1, sFilePath2, nHeight, nWidth) == 0) {
                mpLog->logInfo("领域分析(mean)运行成功");
            }
            else {
                mpLog->logWarnInfo("领域分析(mean)运行失败");
            }
        }
        else if (statType == "MAXIMUM") {
            if (calculateMaximum(sFilePath1, sFilePath2, nHeight, nWidth) == 0) {
                mpLog->logInfo("领域分析(maxmum)运行成功");
            }
            else {
                mpLog->logWarnInfo("领域分析(maxmum)运行失败");
            }
        } 
        else if (statType == "MINIMUM") {
            if (calculateMinimum(sFilePath1, sFilePath2, nHeight, nWidth) == 0) {
                mpLog->logInfo("领域分析(minmum)运行成功");
            }
            else {
                mpLog->logWarnInfo("领域分析(minmum)运行失败");
            }
        }

        this->close();
        });

    // 取消按钮,清空文本框
    connect(ui.cancelPushButton, &QPushButton::clicked, [&]() {
        ui.inputCombo->clear();
        ui.outputLineEdit->clear();
        ui.heightLineEdit->clear();
        ui.widthLineEdit->clear();
        mpLog->logInfo("取消领域分析");
        this->close();
        });
}

NeighborhoodStatistics::~NeighborhoodStatistics()
{}

// 计算非重叠窗口的平均值的函数
int NeighborhoodStatistics::calculateMean(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth){
    // 打开输入栅格文件
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
    if (poDataset == nullptr) {
        qDebug() << "Failed to open the input raster.";
        return -1;
    }

    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    int nXSize = poBand->GetXSize();
    int nYSize = poBand->GetYSize();
    const char* pszProjection = poDataset->GetProjectionRef();
    double adfGeoTransform[6];
    poDataset->GetGeoTransform(adfGeoTransform);

    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* poOutputDataset = poDriver->Create(outputFilePath, nXSize, nYSize, 1, poBand->GetRasterDataType(), nullptr);
    poOutputDataset->SetProjection(pszProjection);
    poOutputDataset->SetGeoTransform(adfGeoTransform);
    int bGotNodataValue;
    double noDataValue = poBand->GetNoDataValue(&bGotNodataValue);
    poOutputDataset->GetRasterBand(1)->SetNoDataValue(noDataValue);

    // 启动多个线程进行并行处理
    int numThreads = std::thread::hardware_concurrency();
    int rowsPerThread = nYSize / numThreads;
    std::vector<std::thread> threads;

    auto processWindow = [&](int iStart, int iEnd) {
        // 为每个线程单独打开一个GDALDataset实例
        GDALDataset* poThreadDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
        GDALRasterBand* poThreadBand = poThreadDataset->GetRasterBand(1);

        std::vector<float> inputBuffer(nXSize * nHeight);
        std::vector<float> outputBuffer(nXSize * (iEnd - iStart), noDataValue);

        for (int i = iStart; i < iEnd; i += nHeight) {
            // 读取当前窗口行数据
            int actualHeight = (((nHeight) < (iEnd - i)) ? (nHeight) : (iEnd - i));
            poThreadBand->RasterIO(GF_Read, 0, i, nXSize, actualHeight, inputBuffer.data(), nXSize, actualHeight, GDT_Float32, 0, 0);

            for (int j = 0; j < nXSize; j += nWidth) {
                std::vector<float> window;
                for (int m = 0; m < actualHeight; m++) {
                    for (int n = 0; n < nWidth; n++) {
                        int x = j + n;
                        int y = m;
                        if (x < nXSize && y < actualHeight) {
                            float value = inputBuffer[y * nXSize + x];
                            if (value != noDataValue) {
                                window.push_back(value);
                            }
                        }
                    }
                }

                float meanValue = noDataValue;
                if (!window.empty()) {
                    float sum = std::accumulate(window.begin(), window.end(), 0.0f);
                    meanValue = sum / window.size();
                }

                for (int m = 0; m < actualHeight; m++) {
                    for (int n = 0; n < nWidth; n++) {
                        int x = j + n;
                        int y = m;
                        if (x < nXSize && y < actualHeight) {
                            outputBuffer[(i + y - iStart) * nXSize + x] = meanValue;
                        }
                    }
                }
            }
        }

        // 写入处理结果
        {
            std::lock_guard<std::mutex> lock(gdalMutex);
            poOutputDataset->GetRasterBand(1)->RasterIO(GF_Write, 0, iStart, nXSize, iEnd - iStart, outputBuffer.data(), nXSize, iEnd - iStart, GDT_Float32, 0, 0);
        }

        GDALClose(poThreadDataset);
    };

    for (int t = 0; t < numThreads; ++t) {
        int iStart = t * rowsPerThread;
        int iEnd = (t == numThreads - 1) ? nYSize : iStart + rowsPerThread;
        threads.emplace_back(processWindow, iStart, iEnd);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    GDALClose(poDataset);
    GDALClose(poOutputDataset);
    return 0;
}

// 计算非重叠窗口的最大值的函数
int NeighborhoodStatistics::calculateMaximum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth){
    // 打开输入栅格文件
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
    if (poDataset == nullptr) {
        qDebug() << "Failed to open the input raster.";
        return -1;
    }

    // 获取栅格波段和坐标系
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    int nXSize = poBand->GetXSize();
    int nYSize = poBand->GetYSize();
    const char* pszProjection = poDataset->GetProjectionRef();
    double adfGeoTransform[6];
    poDataset->GetGeoTransform(adfGeoTransform);

    // 创建输出栅格
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* poOutputDataset = poDriver->Create(outputFilePath, nXSize, nYSize, 1, poBand->GetRasterDataType(), nullptr);
    poOutputDataset->SetProjection(pszProjection);
    poOutputDataset->SetGeoTransform(adfGeoTransform);

    // 设置Nodata值
    int bGotNodataValue;
    double noDataValue = poBand->GetNoDataValue(&bGotNodataValue);
    poOutputDataset->GetRasterBand(1)->SetNoDataValue(noDataValue);

    // 启动多个线程进行并行处理
    int numThreads = std::thread::hardware_concurrency();
    int rowsPerThread = nYSize / numThreads;
    std::vector<std::thread> threads;

    auto processWindow = [&](int iStart, int iEnd) {
        GDALDataset* poThreadDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
        GDALRasterBand* poThreadBand = poThreadDataset->GetRasterBand(1);

        std::vector<float> window;
        std::vector<float> outputBuffer(nXSize * (iEnd - iStart), noDataValue);

        for (int i = iStart; i < iEnd; i += nHeight) {
            int actualHeight = (((nHeight) < (iEnd - i)) ? (nHeight) : (iEnd - i));
            std::vector<float> inputBuffer(nXSize * actualHeight);
            poThreadBand->RasterIO(GF_Read, 0, i, nXSize, actualHeight, inputBuffer.data(), nXSize, actualHeight, GDT_Float32, 0, 0);

            for (int j = 0; j < nXSize; j += nWidth) {
                window.clear();
                for (int m = 0; m < actualHeight; m++) {
                    for (int n = 0; n < nWidth; n++) {
                        int x = j + n;
                        int y = m;
                        if (x < nXSize && y < actualHeight) {
                            float value = inputBuffer[y * nXSize + x];
                            if (value != noDataValue) {
                                window.push_back(value);
                            }
                        }
                    }
                }

                float maxValue = noDataValue;
                if (!window.empty()) {
                    maxValue = *std::max_element(window.begin(), window.end());
                }

                for (int m = 0; m < actualHeight; m++) {
                    for (int n = 0; n < nWidth; n++) {
                        int x = j + n;
                        int y = m;
                        if (x < nXSize && y < actualHeight) {
                            outputBuffer[(i + y - iStart) * nXSize + x] = maxValue;
                        }
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(gdalMutex);
            poOutputDataset->GetRasterBand(1)->RasterIO(GF_Write, 0, iStart, nXSize, iEnd - iStart, outputBuffer.data(), nXSize, iEnd - iStart, GDT_Float32, 0, 0);
        }

        GDALClose(poThreadDataset);
        };

    for (int t = 0; t < numThreads; ++t) {
        int iStart = t * rowsPerThread;
        int iEnd = (t == numThreads - 1) ? nYSize : iStart + rowsPerThread;
        threads.emplace_back(processWindow, iStart, iEnd);
    }

    for (auto& t : threads) {
        t.join();
    }

    GDALClose(poDataset);
    GDALClose(poOutputDataset);
    return 0;
}

// 计算非重叠窗口的最小值的函数
int NeighborhoodStatistics::calculateMinimum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth){
    // 打开输入栅格文件
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
    if (poDataset == nullptr) {
        qDebug() << "Failed to open the input raster.";
        return -1;
    }

    // 获取栅格波段和坐标系
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    int nXSize = poBand->GetXSize();
    int nYSize = poBand->GetYSize();
    const char* pszProjection = poDataset->GetProjectionRef();
    double adfGeoTransform[6];
    poDataset->GetGeoTransform(adfGeoTransform);

    // 创建输出栅格
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* poOutputDataset = poDriver->Create(outputFilePath, nXSize, nYSize, 1, poBand->GetRasterDataType(), nullptr);

    // 设置坐标系和仿射变换
    poOutputDataset->SetProjection(pszProjection);
    poOutputDataset->SetGeoTransform(adfGeoTransform);

    // 设置Nodata值
    int bGotNodataValue;
    double noDataValue = poBand->GetNoDataValue(&bGotNodataValue);
    poOutputDataset->GetRasterBand(1)->SetNoDataValue(noDataValue);

    // 启动多个线程进行并行处理
    int numThreads = std::thread::hardware_concurrency();
    int rowsPerThread = nYSize / numThreads;
    std::vector<std::thread> threads;

    auto processWindow = [&](int iStart, int iEnd) {
        // 为每个线程单独打开一个GDALDataset实例
        GDALDataset* poThreadDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
        GDALRasterBand* poThreadBand = poThreadDataset->GetRasterBand(1);

        std::vector<float> inputBuffer(nXSize * nHeight);
        std::vector<float> outputBuffer(nXSize * (iEnd - iStart), noDataValue);

        for (int i = iStart; i < iEnd; i += nHeight) {
            // 读取当前窗口行数据
            int actualHeight = (((nHeight) < (iEnd - i)) ? (nHeight) : (iEnd - i));
            poThreadBand->RasterIO(GF_Read, 0, i, nXSize, actualHeight, inputBuffer.data(), nXSize, actualHeight, GDT_Float32, 0, 0);

            for (int j = 0; j < nXSize; j += nWidth) {
                std::vector<float> window;
                for (int m = 0; m < actualHeight; m++) {
                    for (int n = 0; n < nWidth; n++) {
                        int x = j + n;
                        int y = m;
                        if (x < nXSize && y < actualHeight) {
                            float value = inputBuffer[y * nXSize + x];
                            if (value != noDataValue) {
                                window.push_back(value);
                            }
                        }
                    }
                }

                float minValue = noDataValue;
                if (!window.empty()) {
                    minValue = *std::min_element(window.begin(), window.end());
                }

                for (int m = 0; m < actualHeight; m++) {
                    for (int n = 0; n < nWidth; n++) {
                        int x = j + n;
                        int y = m;
                        if (x < nXSize && y < actualHeight) {
                            outputBuffer[(i + y - iStart) * nXSize + x] = minValue;
                        }
                    }
                }
            }
        }

        // 写入处理结果
        {
            std::lock_guard<std::mutex> lock(gdalMutex);
            poOutputDataset->GetRasterBand(1)->RasterIO(GF_Write, 0, iStart, nXSize, iEnd - iStart, outputBuffer.data(), nXSize, iEnd - iStart, GDT_Float32, 0, 0);
        }

        GDALClose(poThreadDataset);
    };

    for (int t = 0; t < numThreads; ++t) {
        int iStart = t * rowsPerThread;
        int iEnd = (t == numThreads - 1) ? nYSize : iStart + rowsPerThread;
        threads.emplace_back(processWindow, iStart, iEnd);
    }

    for (auto& t : threads) {
        t.join();
    }

    GDALClose(poDataset);
    GDALClose(poOutputDataset);
    return 0;
}