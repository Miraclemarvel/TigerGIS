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
    // ��ʼ����־
    mpLog = new logger("դ�����", pLogger);
	//��������ͼ���б�
	QStringList layers = datasetMap.keys();
	QStringList rasterLayers;
	for (const auto& layer : layers) {
		if (layer.endsWith(".tif") || layer.endsWith(".TIF")) {
			rasterLayers << layer;
		}
	}
	ui.inputCombo->addItems(rasterLayers);

    // ѡ�������ļ�
    connect(ui.openPushButton, &QPushButton::clicked, [&]() {
        auto sFilePath1 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.tif *.TIF)");
		QStringList temp = sFilePath1.split("/");					//ͨ��·���ָ����ָ��ַ���
		QString basename = temp.at(temp.size() - 1);	//��ȡ�ļ���
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
        });

    // ѡ������ļ���
    connect(ui.outputPushButton, &QPushButton::clicked, [&]() {
        auto sFilePath2 = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("����ļ�").toUtf8().constData()),
            "./output", "TIFF Files (*.tif)");
        ui.outputLineEdit->setText(sFilePath2);
        });

    // ȷ����ťִ������ͳ��
    connect(ui.surePushButton, &QPushButton::clicked, [&]() {
        // ����դ��
        QString sLayerName = ui.inputCombo->currentText();
        const char* sFilePath1 = mDatasetMap[sLayerName]->GetDescription();
        // ��־���
        auto info = "���ڶ�ͼ��" + sLayerName.toStdString() + "�����������...";
        mpLog->logInfo(info.c_str());
        // ����ļ�
        QString filePath2 = ui.outputLineEdit->text();
        QByteArray byteArray2 = filePath2.toUtf8();
        const char* sFilePath2 = byteArray2.constData();

        // �߶ȺͿ��
        int nHeight = ui.heightLineEdit->text().toUtf8().toInt();
        int nWidth = ui.widthLineEdit->text().toUtf8().toInt();

        // ��ȡѡ���ͳ������
        QString statType = ui.comboBox->currentText();

        // ����ѡ���ͳ������ִ�ж�Ӧ�ĺ���
        if (statType == "MEAN") {
            if (calculateMean(sFilePath1, sFilePath2, nHeight, nWidth) == 0) {
                mpLog->logInfo("�������(mean)���гɹ�");
            }
            else {
                mpLog->logWarnInfo("�������(mean)����ʧ��");
            }
        }
        else if (statType == "MAXIMUM") {
            if (calculateMaximum(sFilePath1, sFilePath2, nHeight, nWidth) == 0) {
                mpLog->logInfo("�������(maxmum)���гɹ�");
            }
            else {
                mpLog->logWarnInfo("�������(maxmum)����ʧ��");
            }
        } 
        else if (statType == "MINIMUM") {
            if (calculateMinimum(sFilePath1, sFilePath2, nHeight, nWidth) == 0) {
                mpLog->logInfo("�������(minmum)���гɹ�");
            }
            else {
                mpLog->logWarnInfo("�������(minmum)����ʧ��");
            }
        }

        this->close();
        });

    // ȡ����ť,����ı���
    connect(ui.cancelPushButton, &QPushButton::clicked, [&]() {
        ui.inputCombo->clear();
        ui.outputLineEdit->clear();
        ui.heightLineEdit->clear();
        ui.widthLineEdit->clear();
        mpLog->logInfo("ȡ���������");
        this->close();
        });
}

NeighborhoodStatistics::~NeighborhoodStatistics()
{}

// ������ص����ڵ�ƽ��ֵ�ĺ���
int NeighborhoodStatistics::calculateMean(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth){
    // ������դ���ļ�
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

    // ��������߳̽��в��д���
    int numThreads = std::thread::hardware_concurrency();
    int rowsPerThread = nYSize / numThreads;
    std::vector<std::thread> threads;

    auto processWindow = [&](int iStart, int iEnd) {
        // Ϊÿ���̵߳�����һ��GDALDatasetʵ��
        GDALDataset* poThreadDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
        GDALRasterBand* poThreadBand = poThreadDataset->GetRasterBand(1);

        std::vector<float> inputBuffer(nXSize * nHeight);
        std::vector<float> outputBuffer(nXSize * (iEnd - iStart), noDataValue);

        for (int i = iStart; i < iEnd; i += nHeight) {
            // ��ȡ��ǰ����������
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

        // д�봦����
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

    // �ȴ������߳����
    for (auto& t : threads) {
        t.join();
    }

    GDALClose(poDataset);
    GDALClose(poOutputDataset);
    return 0;
}

// ������ص����ڵ����ֵ�ĺ���
int NeighborhoodStatistics::calculateMaximum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth){
    // ������դ���ļ�
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
    if (poDataset == nullptr) {
        qDebug() << "Failed to open the input raster.";
        return -1;
    }

    // ��ȡդ�񲨶κ�����ϵ
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    int nXSize = poBand->GetXSize();
    int nYSize = poBand->GetYSize();
    const char* pszProjection = poDataset->GetProjectionRef();
    double adfGeoTransform[6];
    poDataset->GetGeoTransform(adfGeoTransform);

    // �������դ��
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* poOutputDataset = poDriver->Create(outputFilePath, nXSize, nYSize, 1, poBand->GetRasterDataType(), nullptr);
    poOutputDataset->SetProjection(pszProjection);
    poOutputDataset->SetGeoTransform(adfGeoTransform);

    // ����Nodataֵ
    int bGotNodataValue;
    double noDataValue = poBand->GetNoDataValue(&bGotNodataValue);
    poOutputDataset->GetRasterBand(1)->SetNoDataValue(noDataValue);

    // ��������߳̽��в��д���
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

// ������ص����ڵ���Сֵ�ĺ���
int NeighborhoodStatistics::calculateMinimum(const char* inputFilePath, const char* outputFilePath, int nHeight, int nWidth){
    // ������դ���ļ�
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
    if (poDataset == nullptr) {
        qDebug() << "Failed to open the input raster.";
        return -1;
    }

    // ��ȡդ�񲨶κ�����ϵ
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    int nXSize = poBand->GetXSize();
    int nYSize = poBand->GetYSize();
    const char* pszProjection = poDataset->GetProjectionRef();
    double adfGeoTransform[6];
    poDataset->GetGeoTransform(adfGeoTransform);

    // �������դ��
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* poOutputDataset = poDriver->Create(outputFilePath, nXSize, nYSize, 1, poBand->GetRasterDataType(), nullptr);

    // ��������ϵ�ͷ���任
    poOutputDataset->SetProjection(pszProjection);
    poOutputDataset->SetGeoTransform(adfGeoTransform);

    // ����Nodataֵ
    int bGotNodataValue;
    double noDataValue = poBand->GetNoDataValue(&bGotNodataValue);
    poOutputDataset->GetRasterBand(1)->SetNoDataValue(noDataValue);

    // ��������߳̽��в��д���
    int numThreads = std::thread::hardware_concurrency();
    int rowsPerThread = nYSize / numThreads;
    std::vector<std::thread> threads;

    auto processWindow = [&](int iStart, int iEnd) {
        // Ϊÿ���̵߳�����һ��GDALDatasetʵ��
        GDALDataset* poThreadDataset = (GDALDataset*)GDALOpen(inputFilePath, GA_ReadOnly);
        GDALRasterBand* poThreadBand = poThreadDataset->GetRasterBand(1);

        std::vector<float> inputBuffer(nXSize * nHeight);
        std::vector<float> outputBuffer(nXSize * (iEnd - iStart), noDataValue);

        for (int i = iStart; i < iEnd; i += nHeight) {
            // ��ȡ��ǰ����������
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

        // д�봦����
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