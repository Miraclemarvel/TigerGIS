#include "TigerGIS.h"
#include <ogrsf_frmts.h>
#include <ogr_spatialref.h>
#include "gdalwarper.h"
#include "gdal_alg.h"
#include "cpl_conv.h"
#include <gdal_priv.h>

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QDebug>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QFileDialog>
#include <QColorDialog>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <iostream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <thread>
#include <mutex>
// ���캯��
TigerGIS::TigerGIS(QWidget* parent)
	: QMainWindow(parent), mpFileList(new QStandardItemModel(this))
	, mpInput(nullptr)
	, mpoDataset(nullptr)
	, mpRootNode(nullptr)
	, mpAttrWidget(nullptr)
	, mpHist(nullptr)
	, mpNeighbor(nullptr)
	, mpMask(nullptr)
	, mpScene(new QGraphicsScene(ui.graphicsView))
	, mpLog(nullptr)
{
    ui.setupUi(this);
	// ����Ӧ�ó���ͼ��
	setWindowIcon(QIcon("./res/Tiger.png"));
	mpLog = new logger(ui.loggerEdit);
	ui.statusBar->setStyleSheet("QStatusBar::item{border:0px}");
	// ��������ƶ��źŵ��ۺ���
	connect(ui.graphicsView, &ZoomableGraphicsView::mouseMoved, this, &TigerGIS::updateStatusBar);
	// ���������б���б��⣨��Ŀ¼��ͼ�㣩
	mpFileList->setHorizontalHeaderLabels(QStringList() << QStringLiteral("ͼ��"));
	ui.contentTree->setModel(mpFileList);
	mpRootNode = mpFileList->invisibleRootItem();

	// �����б�ͼ�㹴ѡ��ʾ
	connect(mpFileList, &QStandardItemModel::itemChanged, [=](QStandardItem* item) {
		if (item->isCheckable()) {
			Qt::CheckState state = item->checkState();
			// ����״̬���н���
			if (state == Qt::Checked) {
				auto info = "��ʾͼ��" + item->text().toStdString();
				mpLog->logInfo(info.c_str());
				drawLayer(mLayerMap[item->text()]);
			}
			else {
				// ִ��δѡ��ʱ�Ĳ���
				auto info = "����ͼ��" + item->text().toStdString();
				mpLog->logInfo(info.c_str());
				removeLayer(mLayerMap[item->text()]);
			}
		}
	});
	// �����б�����Ҽ��˵�
	connect(ui.contentTree, &QTreeView::customContextMenuRequested, [=](const QPoint& pos) {
		QModelIndex index = ui.contentTree->indexAt(pos);	//��ȡ����Ľڵ�
			if (index.isValid()) {
				QStandardItemModel* model = static_cast<QStandardItemModel*>(ui.contentTree->model());
				QStandardItem* item = model->itemFromIndex(index);

				if (item && item->rowCount() == 0) { // ��ȡ�򿪵��ļ��ڵ�
					QMenu contextMenu;	//�Ҽ��˵�
					QAction* actionDel = new QAction(QStringLiteral("�Ƴ�ͼ��"), this);	//�Ƴ�ͼ��
					QAction* actionSym = new QAction(QStringLiteral("��������"), this); //����ϵͳ
					QAction* actionTFD = new QAction(QStringLiteral("��/�ٲ�ɫ��ʾ"), this); //դ����ٲ�ɫ��ʾ
					//����ͼ��
					actionDel->setIcon(QIcon("./res/remove.png")); 
					actionSym->setIcon(QIcon("./res/symbol.png"));
					actionTFD->setIcon(QIcon("./res/RGB.png"));
					//���Ӳ˵����뽻������
					connect(actionDel, &QAction::triggered, this, [&]() {
						//�Ƴ���ͼ��ʾ
						removeLayer(mLayerMap[item->text()]);
						//�ͷ��ڴ�
						for (auto& item : mLayerMap[item->text()]) {
							delete item;
							item = nullptr;
						}
						mLayerMap.remove(item->text());
						// ��־���
						auto info = "�Ƴ�ͼ��" + item->text().toStdString();
						mpLog->logInfo(info.c_str());
						//�ر����ݼ�
						GDALClose(mDatasetMap[item->text()]);
						//�Ƴ����ݼ�
						mDatasetMap.remove(item->text());
						//�Ƴ������б�
						QModelIndex parentIndex = index.parent();
						model->removeRow(index.row(), parentIndex);
						//���򿪵��ļ����Ƿ�Ϊ��
						QStandardItem* parentItem = model->itemFromIndex(parentIndex);
						if (parentItem && parentItem->rowCount() == 0) {
							model->removeRow(parentIndex.row(), parentIndex.parent());
						}
					});
					connect(actionSym, &QAction::triggered, this, [&]() {
						selectColor(index.data().toString());
					});
					connect(actionTFD, &QAction::triggered, this, [&]() {
						QString layerName = item->text();
						mpoDataset = mDatasetMap[layerName];
						// ��־���
						auto info = "ѡ��ͼ��" + layerName.toStdString() + "�Ĳ������";
						mpLog->logInfo(info.c_str());
						if (!mpoDataset) {
							return; // ���ݼ�����ͼδ��ʼ�������ش���
						}

						if (mpoDataset->GetRasterCount() == 0) {
							return; // ���ݼ���û��դ������
						}
						//��ȡ������
						int bands = mpoDataset->GetRasterCount();
						// ��ȡ���в��ε�����
						QStringList bandNames;
						for (int i = 1; i <= bands; ++i) {
							GDALRasterBand* band = mpoDataset->GetRasterBand(i);
							const char* description = band->GetDescription();
							if (description && description[0] != '\0') {
								bandNames << QString::fromUtf8(description);
							}
							else {
								// �������Ϊ�գ�����ʹ�ò���������Ϊ����
								bandNames << QString("Band %1").arg(i);
							}
						}
						int redBand = 0;
						int greenBand = 0;
						int blueBand = 0;
						if (bands >= 3) {
							// ��ȡ�û�����Ĳ������
							bool ok;
							QString redBandName = QInputDialog::getItem(nullptr, QStringLiteral("ѡ���ɫ����"), QStringLiteral("�Ӳ����б���ѡ���ɫ����"), bandNames, 0, false, &ok);
							if (!ok) return;
							QString greenBandName = QInputDialog::getItem(nullptr, QStringLiteral("ѡ����ɫ����"), QStringLiteral("�Ӳ����б���ѡ����ɫ����"), bandNames, 0, false, &ok);
							if (!ok) return;
							QString blueBandName = QInputDialog::getItem(nullptr, QStringLiteral("ѡ����ɫ����"), QStringLiteral("�Ӳ����б���ѡ����ɫ����"), bandNames, 0, false, &ok);
							if (!ok) return;
							redBand = bandNames.indexOf(redBandName) + 1;
							greenBand = bandNames.indexOf(greenBandName) + 1;
							blueBand = bandNames.indexOf(blueBandName) + 1;
						}
						else {
							mpLog->logWarnInfo("������Ϊ1���޷�����ͨ�����");
							return;
						}
						removeLayer(mLayerMap[layerName]);
						//�ͷ��ڴ�
						for (auto& item : mLayerMap[layerName]) {
							delete item;
							item = nullptr;
						}
						mLayerMap.remove(item->text());
						drawRaster(mpoDataset->GetDescription(), redBand, greenBand, blueBand);
					});
					//��Ӳ˵���
					contextMenu.addAction(actionDel); //�Ƴ�ͼ��
					contextMenu.addAction(actionSym); //����ϵͳ
					contextMenu.addAction(actionTFD); //դ����ٲ�ɫ��ʾ
					//�޶����ܷ�Χ
					if (index.data().toString().endsWith(".tif") || index.data().toString().endsWith(".TIF")) {
						actionSym->setEnabled(false);
					}
					else {
						actionTFD->setEnabled(false);
					}
					// ��ʾ�����Ĳ˵����������λ��
					contextMenu.exec(ui.contentTree->viewport()->mapToGlobal(pos));
				}
			}
	});

	// ������С��������ű���
	ui.graphicsView->setMinScale(0.5);	// ��С���ű���
	ui.graphicsView->setMaxScale(20);	// ������ű���

	// �������ű仯�źŵ���
	connect(ui.graphicsView, &ZoomableGraphicsView::scaleChanged, [this]() {
		ui.graphicsView->updateSceneScale();
		});
	// ���Ӹ�λ��ť����
	connect(ui.resetBtn, &QPushButton::clicked, this, &TigerGIS::resetView);

	OGRRegisterAll();	//ע��GDAL����

	//��wkt�ļ�
	connect(ui.actionwktFile, &QAction::triggered, [&]() {
		QString sFileName = QFileDialog::getOpenFileName(this, tr("Open WKT File"),
			"./data",
			tr("WKT Files (*.txt *.wkt *.csv)"));
		QFile file(sFileName);
		std::vector<QString> wktList;

		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			qDebug().noquote() << "Failed to open file: " << sFileName << endl;
			return;
		}

		QTextStream in(&file);
		while (!in.atEnd()) {
			QString line = in.readLine();
			QStringList fields = line.split(',');
			if (fields.size() > 0) {
				wktList.push_back(fields[0]);
			}
		}
		file.close();
		for (auto& wkt : wktList) {
			OGRGeometry* poGeometry;
			QByteArray byteArray = wkt.toUtf8();
			const char* wktCStr = byteArray.constData();
			OGRErr err = OGRGeometryFactory::createFromWkt(&wktCStr, NULL, &poGeometry);
			if (err != OGRERR_NONE) {
				qDebug().noquote() << "Failed to parse WKT: " << wkt << endl;
			}
			drawGeometry(poGeometry);
			OGRGeometryFactory::destroyGeometry(poGeometry);
		}


		});

//����ϵͳ
	//�������������ɫ
	connect(ui.symbolBtn, &QPushButton::clicked, this, &TigerGIS::selectAllColor);
}

// �������� 
TigerGIS::~TigerGIS(){
	for (auto& layer : mLayerMap) {
		for (auto& item : layer) {
			delete item;
		}
		layer.clear();
	}
	mLayerMap.clear();
	for (auto& dataset : mDatasetMap) {
		GDALClose(dataset);
	}
	mDatasetMap.clear();
	delete mpAttrWidget;
	delete mpInput;
	delete mpHist;
	delete mpNeighbor;
	delete mpMask;
	delete mpLog;
}

// ����״̬����ʾ�������
void TigerGIS::updateStatusBar(const QPointF& pos) {
	// ����״̬��
	ui.graphicsView->mpStatusLabel->setText(QString("%1, %2 %3")
		.arg(QString::number(pos.x() / 1000.0, 'f', 3))
		.arg(QString::number(-pos.y() / 1000.0, 'f', 3))
		.arg(QStringLiteral("ʮ���ƶ�")));
	ui.statusBar->addPermanentWidget(ui.graphicsView->mpStatusLabel);

}
// �˵�--�ļ�
	//��ʸ������
void TigerGIS::on_actionShpFile_triggered() {
	// ��ȡ�ļ�·��(�ļ���)
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		"./data",
		tr("Images (*.shp)"));
	// �����־
	auto info = "��ʸ���ļ�" + sFileName.toStdString();
	mpLog->logInfo(info.c_str());
	// ��ʸ�����ݼ�
	mpoDataset = static_cast<GDALDataset*>(GDALOpenEx(sFileName.toUtf8().constData(), GDAL_OF_VECTOR, NULL, NULL, NULL));
	if (mpoDataset == nullptr) {
		// �����־
		mpLog->logErrorInfo("�ļ���ʧ��");
		return;
	}
	else {
		//�����־
		mpLog->logInfo("�ļ��򿪳ɹ������ڻ���ͼ��...");
		addShpContent(sFileName);
	}
	ui.graphicsView->setCurrentScale(1.0);
	drawShp(sFileName);
}
	//��դ������
void TigerGIS::on_actionRasterFile_triggered() {
	// ��ȡ�ļ�·��(�ļ�����
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		"./data",
		tr("Images (*.tif *.TIF)"));
	// �����־
	auto info = "��դ���ļ�" + sFileName.toStdString();
	mpLog->logInfo(info.c_str());
	// ��դ�����ݼ�
	mpoDataset = (GDALDataset*)GDALOpenEx(sFileName.toUtf8().constData(), GDAL_OF_READONLY, NULL, NULL, NULL);
	if (mpoDataset == nullptr) {
		// �����־
		mpLog->logErrorInfo("�ļ���ʧ��");
		return;
	}
	else {
		//�����־
		mpLog->logInfo("�ļ��򿪳ɹ������ڻ���ͼ��...");
		addRasterContent(sFileName);
	}
	ui.graphicsView->setCurrentScale(1.0);
	drawRaster(sFileName);
}
	// �򿪴�������դ���ļ�
void TigerGIS::on_actionbigRaster_triggered() {
	// ��ȡ�ļ�·��
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		"./data",
		tr("Images (*.tif *.TIF)"));
	// �����־
	auto info = "�򿪳���������դ���ļ�" + sFileName.toUtf8();
	mpLog->logInfo(info);
	// ��դ�����ݼ�
	mpoDataset = (GDALDataset*)GDALOpenEx(sFileName.toUtf8().constData(), GDAL_OF_READONLY, NULL, NULL, NULL);
	if (mpoDataset == nullptr) {
		// �����־
		mpLog->logErrorInfo("�ļ���ʧ��");
		return;
	}
	else {
		//�����־
		mpLog->logInfo("�ļ��򿪳ɹ������ڻ���ͼ��...");
		addRasterContent(sFileName);
	}
	//��ȡդ���ļ���������
	int width = mpoDataset->GetRasterXSize();
	int height = mpoDataset->GetRasterYSize();
	int bands = mpoDataset->GetRasterCount();

	// ȷ��ͼ���ʽ
	QImage::Format format;
	if (bands == 1) {
		format = QImage::Format_Grayscale8; // ������
	}
	else if (bands >= 3) {
		format = QImage::Format_RGB32; // RGB����
	}
	else {
		return; // ��������ݲ�����
	}

	// ��ȡդ��ĵ���任����
	double geoTransform[6];
	mpoDataset->GetGeoTransform(geoTransform);
	// �������Χ
	double minGeoX = geoTransform[0];
	double maxGeoX = geoTransform[0] + width * geoTransform[1] + height * geoTransform[2];
	double minGeoY = geoTransform[3] + width * geoTransform[4] + height * geoTransform[5];
	double maxGeoY = geoTransform[3];

	qDebug() << "minGeoX = " << minGeoX << " " << "minGeoY = " << minGeoY << endl;
	qDebug() << "maxGeoX = " << maxGeoX << " " << "maxGeoY = " << maxGeoY << endl;

	int blockSizeX = 4096;	//��Ƭ��
	int blockSizeY = 4096;	//��Ƭ��

	if (bands == 1) {
		GDALRasterBand* band = mpoDataset->GetRasterBand(1); // ��ȡ��һ������
		//ȫ�ֹ�һ��
		/*double minValue, maxValue, mean, stddev;
		band->GetStatistics(FALSE, TRUE, &minValue, &maxValue, &mean, &stddev);*/

		for (int y = 0; y < height; y += blockSizeY) {
			for (int x = 0; x < width; x += blockSizeX) {
				int blockWidth = (((blockSizeX) < (width - x)) ? (blockSizeX) : (width - x));
				int blockHeight = (((blockSizeY) < (height - y)) ? (blockSizeY) : (height - y));

				std::vector<unsigned short> buffer(blockWidth * blockHeight);
				band->RasterIO(GF_Read, x, y, blockWidth, blockHeight, buffer.data(), blockWidth, blockHeight, GDT_UInt16, 0, 0);
				//�ֿ��һ��
				double minValue = *std::min_element(buffer.begin(), buffer.end());
				double maxValue = *std::max_element(buffer.begin(), buffer.end());

				QImage image(blockWidth, blockHeight, format);				
				for (int j = 0; j < blockHeight; ++j) {
					for (int i = 0; i < blockWidth; ++i) {
						int gray = static_cast<int>(255 * (buffer[j * blockWidth + i] - minValue) / maxValue - minValue);
						image.setPixel(i, j, qRgb(gray, gray, gray));
					}
				}
				QPixmap pixmap = QPixmap::fromImage(image);
				QGraphicsPixmapItem* item = new QGraphicsPixmapItem(pixmap);
				item->setPos(x, y);
				mpScene->addItem(item);
			}
		}
	}
	else if (bands >= 3) {
		GDALRasterBand* rBand = mpoDataset->GetRasterBand(1); // ��ɫ����
		GDALRasterBand* gBand = mpoDataset->GetRasterBand(2); // ��ɫ����
		GDALRasterBand* bBand = mpoDataset->GetRasterBand(3); // ��ɫ����

		auto task = [&](int x, int y, int blockWidth, int blockHeight, QGraphicsPixmapItem** ret, std::mutex& lock1, std::mutex& lock2, std::mutex& lock3) {
			std::vector<uint8_t> rBuffer(blockWidth * blockHeight);
			std::vector<uint8_t> gBuffer(blockWidth * blockHeight);
			std::vector<uint8_t> bBuffer(blockWidth * blockHeight);

			lock1.lock();
			rBand->RasterIO(GF_Read, x, y, blockWidth, blockHeight, rBuffer.data(), blockWidth, blockHeight, GDT_Byte, 0, 0);
			lock1.unlock();
			lock2.lock();
			gBand->RasterIO(GF_Read, x, y, blockWidth, blockHeight, gBuffer.data(), blockWidth, blockHeight, GDT_Byte, 0, 0);
			lock2.unlock();
			lock3.lock();
			bBand->RasterIO(GF_Read, x, y, blockWidth, blockHeight, bBuffer.data(), blockWidth, blockHeight, GDT_Byte, 0, 0);
			lock3.unlock(); 

			QImage image(blockWidth, blockHeight, format);

			for (int j = 0; j < blockHeight; ++j) {
				for (int i = 0; i < blockWidth; ++i) {
					int r = static_cast<int>(rBuffer[j * blockWidth + i]);
					int g = static_cast<int>(gBuffer[j * blockWidth + i]);
					int b = static_cast<int>(bBuffer[j * blockWidth + i]);
					image.setPixel(i, j, qRgb(r, g, b));
				}
			}
			QPixmap pixmap = QPixmap::fromImage(image);
			(*ret) = new QGraphicsPixmapItem(pixmap);
			(*ret)->setPos(x, y);
		};

		std::vector<std::thread> threads;
		std::vector<QGraphicsPixmapItem**> results;
		std::mutex lock1, lock2, lock3;

		for (int y = 0; y < height; y += blockSizeY) {
			for (int x = 0; x < width; x += blockSizeX) {
				int blockWidth = (((blockSizeX) < (width - x)) ? (blockSizeX) : (width - x));
				int blockHeight = (((blockSizeY) < (height - y)) ? (blockSizeY) : (height - y));
				QGraphicsPixmapItem** t = static_cast<QGraphicsPixmapItem**>(malloc(sizeof(QGraphicsPixmapItem*)));
				threads.emplace_back(task, x, y, blockWidth, blockHeight, t, std::ref(lock1), std::ref(lock2), std::ref(lock3));
				results.push_back(t);
			}
		}

		for (auto& item : threads) {
			item.join();
		}

		QVector<QGraphicsItem*> bigRaster;
		for (auto& result : results) {
			bigRaster.push_back(*result);
			free(result);
		}
		
		// ��ȡ�ļ���Ϣ
		QFileInfo fileInfo(sFileName);
		// ��ȡ�ļ������ļ���·��
		QString folderPath = fileInfo.absolutePath();
		// ��ȡͼ����
		QString sLayerName = fileInfo.fileName();
		mLayerMap[sLayerName] = bigRaster;
		mDatasetMap[sLayerName] = mpoDataset;
		drawLayer(bigRaster);
	}
	ui.graphicsView->setCurrentScale(1.0);
	ui.graphicsView->setScene(mpScene);
}
	// ���湤���ļ�
void TigerGIS::on_actionsaveProject_triggered() {
	// ��ȡ�����ļ���·��
	QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("���湤���ļ�"), "./output", QStringLiteral("TigerGIS�����ĵ� (*.tgs)"));
	// �����־
	auto info = "���湤���ļ���" + filePath.toStdString();
	mpLog->logInfo(info.c_str());
	// �쳣����
	if (filePath.isEmpty()) {
		mpLog->logWarnInfo("�ļ�·������");
		return;
	}
	if (!filePath.endsWith(".tgs")) {
		filePath += ".tgs";
	}

	QJsonObject project;	//�����ļ�
	QJsonArray layers;		//ͼ��

	//��ȡ�����б��ȡ�򿪵�ͼ��
	QStringList layersName = mDatasetMap.keys();
	if (layersName.isEmpty()) {
		mpLog->logWarnInfo("ͼ��Ϊ�գ��޷����湤���ļ���");
		return;
	}
	for (const auto& layerName : layersName) {
		QJsonObject layer;
		layer["name"] = layerName;
		layer["path"] = mDatasetMap[layerName]->GetDescription();
		if (layerName.endsWith(".tif") || layerName.endsWith(".TIF")) {
			layer["type"] = "raster";
		}
		else {
			layer["type"] = "vector";
		}
		layers.append(layer);
	}
	//���ͼ��
	project["layers"] = layers;
	project["projection"] = "EPSG : 4326";
	//�����ļ�
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly)) {
		mpLog->logWarnInfo("�޷�д���ļ���");
		return;
	}

	file.write(QJsonDocument(project).toJson());
	file.close();
	mpLog->logInfo("�����ļ�����ɹ�");
}
	// ���빤���ļ�
void TigerGIS::on_actionreadProject_triggered() {
	// �򿪹����ļ�
	QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("���빤���ļ�"), "./output", QStringLiteral("TigerGIS�����ĵ�(*.tgs)"));
	// �����־
	auto info = "���빤���ļ�" + filePath.toStdString();
	mpLog->logInfo(info.c_str());
	// �쳣����
	if (filePath.isEmpty()) {	// ����չ����ļ�
		mpLog->logWarnInfo("�����ļ�Ϊ�գ�");
		return;
	}
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {	// �������ļ���ȡʧ��
		mpLog->logWarnInfo("�����ļ���ȡʧ�ܣ�");
		return;
	}
	// ��ȡ�����ļ�����
	QByteArray fileData = file.readAll();
	QJsonDocument document = QJsonDocument::fromJson(fileData);
	QJsonObject project = document.object();
	// ��ȡͼ����Ϣ
	QJsonArray layers = project["layers"].toArray();
	for (const auto& layer : layers) {
		QJsonObject layerObj = layer.toObject();
		QString layerPath =  layerObj["path"].toString();
		if (layerObj["type"].toString() == "vector") {	//����ʸ������
			mpoDataset = static_cast<GDALDataset*>(GDALOpenEx(layerPath.toUtf8().constData(), GDAL_OF_VECTOR, NULL, NULL, NULL));
			addShpContent(layerPath);
			drawShp(layerPath);
		}
		else {	//����դ������
			mpoDataset = (GDALDataset*)GDALOpenEx(layerPath.toUtf8().constData(), GDAL_OF_READONLY, NULL, NULL, NULL);
			addRasterContent(layerPath);
			drawRaster(layerPath);
		}
	}
	mpLog->logInfo("�����ļ�����ɹ���");
}
// �˵�--�༭--�༭ʸ��Ҫ��
	// ��ʼ�༭Ҫ��
void TigerGIS::on_actionStartEditing_triggered() {
	QStringList layers = mLayerMap.keys(); // ��ȡ����ͼ��ļ��б�
	bool ok; // ���ڽ����û������״̬
	QString layer = QInputDialog::getItem(this, QStringLiteral("ѡ����Ҫ�༭��ͼ��: "), QStringLiteral("ͼ��:"), layers, 0, false, &ok); // ��ʾ����Ի��򣬻�ȡ�û�ѡ���ͼ��
	// ��־���
	auto info = "���ڱ༭ͼ��" + layer.toStdString();
	mpLog->logInfo(info.c_str());
	// �ж�ͼ��Ϸ���
	if (mLayerMap.contains(layer)) {
		mvEditItems = mLayerMap.value(layer);
	}

	if (ok && !layer.isEmpty()) { // ����û�ȷ�����������벻Ϊ��
		QStringList featureTypes = { "Point", "Line", "Polygon" }; // ����Ҫ�������б�
		QString featureType = QInputDialog::getItem(this, QStringLiteral("ѡ����Ҫ�༭��Ҫ������:"), QStringLiteral("Ҫ������:"), featureTypes, 0, false, &ok); // ��ʾ����Ի��򣬻�ȡ�û�ѡ���Ҫ������
		// ��־���
		auto info = "�༭ͼ��Ҫ������" + featureType.toStdString();
		mpLog->logInfo(info.c_str());

		if (ok && !featureType.isEmpty()) { // ����û�ȷ�����������벻Ϊ��
			ui.graphicsView->msCurrentLayer = layer; // ���õ�ǰͼ��
			ui.graphicsView->msCurrentFeatureType = featureType; // ���õ�ǰҪ������
			ui.graphicsView->mbIsEditing = true; // ���ñ༭״̬Ϊ true
		}
	}
}
	// ����
void TigerGIS::on_actionUndo_triggered(){
	if (ui.graphicsView->mbIsEditing) {
		if (ui.graphicsView->msCurrentFeatureType == "Polygon" && !ui.graphicsView->mvTempItems.isEmpty()) {
			auto item = dynamic_cast<QGraphicsPolygonItem*>(ui.graphicsView->mvTempItems.last());
			ui.graphicsView->scene()->removeItem(item);
			ui.graphicsView->mvUndoPolygonVector.push_back(item);
		}
		else if(ui.graphicsView->msCurrentFeatureType == "Line" && !ui.graphicsView->mvTempItems.isEmpty()){
			auto item = dynamic_cast<QGraphicsLineItem*>(ui.graphicsView->mvTempItems.last());
			ui.graphicsView->scene()->removeItem(item);
			ui.graphicsView->mvUndoLineVector.push_back(item);
		}
		else if (ui.graphicsView->msCurrentFeatureType == "Line" && !ui.graphicsView->mvTempItems.isEmpty()) {
			auto item = dynamic_cast<QGraphicsEllipseItem*>(ui.graphicsView->mvTempItems.last());
			ui.graphicsView->scene()->removeItem(item);
			ui.graphicsView->mvUndoPointVector.push_back(item);
		}
		else {
			mpLog->logWarnInfo("û�п��Գ��صĲ�����");
			return;
		}
		mpLog->logInfo("���سɹ�");
		ui.graphicsView->mvTempItems.pop_back();
	}
	else {
		mpLog->logWarnInfo("û��ͼ�㴦�ڱ༭״̬���޷�ִ�г��ز���");
		return;
	}
}
	// ����
void TigerGIS::on_actionRedo_triggered(){
	if (ui.graphicsView->mbIsEditing) {
		if (ui.graphicsView->msCurrentFeatureType == "Polygon" && !ui.graphicsView->mvUndoPolygonVector.isEmpty()) {
			auto item = ui.graphicsView->mvUndoPolygonVector.last();
			ui.graphicsView->scene()->addItem(item);
			ui.graphicsView->mvUndoPolygonVector.pop_back();
			ui.graphicsView->mvTempItems.push_back(item);
		}
		else if (ui.graphicsView->msCurrentFeatureType == "Line" && !ui.graphicsView->mvUndoLineVector.isEmpty()) {
			auto item = ui.graphicsView->mvUndoLineVector.last();
			ui.graphicsView->scene()->addItem(item);
			ui.graphicsView->mvUndoLineVector.pop_back();
			ui.graphicsView->mvTempItems.push_back(item);
		}
		else if (ui.graphicsView->msCurrentFeatureType == "Line" && !ui.graphicsView->mvUndoPointVector.isEmpty()) {
			auto item = ui.graphicsView->mvUndoPointVector.last();
			ui.graphicsView->scene()->addItem(item);
			ui.graphicsView->mvUndoPointVector.pop_back();
			ui.graphicsView->mvTempItems.push_back(item);
		}
		else {
			mpLog->logWarnInfo("û�п��������Ĳ�����");
			return;
		}
		mpLog->logInfo("�����ɹ�");
	}
	else {
		mpLog->logWarnInfo("û��ͼ�㴦�ڱ༭״̬���޷�ִ����������");
		return;
	}
}
	// ֹͣ�༭
void TigerGIS::on_actionStopEditing_triggered() {
	if (ui.graphicsView->mbIsEditing) { // ������ڱ༭״̬
		ui.graphicsView->mvUndoPolygonVector.clear();
		ui.graphicsView->mvUndoLineVector.clear();
		ui.graphicsView->mvUndoPointVector.clear();
		ui.graphicsView->mbIsEditing = false; // ���ñ༭״̬Ϊ false
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, QStringLiteral("����༭����"), QStringLiteral("�Ƿ񽫱༭���ݱ���Ϊʸ���ļ�"), // ��ʾ������ĵ�ȷ�϶Ի���
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) { // ����û�ѡ�񱣴�
			saveChanges(); // ���ñ�����ĵĺ���
		}
		else { // ����û�ѡ�񲻱���
			discardChanges(); // ���ö������ĵĺ���
		}
	}
}
//�˵�--����--ʸ������--ͳ�Ʒ���
	// ͳ������
void TigerGIS::on_actionCalculateNum_triggered() {
	QStringList layers = mLayerMap.keys(); // ��ȡ����ͼ��ļ��б�
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // ���ڽ����û������״̬
	// ��ȡͳ��ͼ��
	QString layer = QInputDialog::getItem(this, QStringLiteral("ѡ����Ҫͳ�Ƶ�ͼ��: "), QStringLiteral("ͼ��:"), vectorLayers, 0, false, &ok); // ��ʾ����Ի��򣬻�ȡ�û�ѡ���ͼ��
	// ��־���
	auto info = "����ͳ��ͼ��" + layer.toStdString() + "Ҫ������";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}

	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("ʸ�����ݼ�Ϊ��");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 1, this, mpoDataset);
	mpAttrWidget->open();
}
	// ͳ�����	
void TigerGIS::on_actionCalculateArea_triggered() {
	QStringList layers = mLayerMap.keys(); // ��ȡ����ͼ��ļ��б�
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // ���ڽ����û������״̬
	// ��ȡͳ��ͼ��
	QString layer = QInputDialog::getItem(this, QStringLiteral("ѡ����Ҫͳ�Ƶ�ͼ��: "), QStringLiteral("ͼ��:"), vectorLayers, 0, false, &ok); // ��ʾ����Ի��򣬻�ȡ�û�ѡ���ͼ��
	// ��־���
	auto info = "����ͳ��ͼ��" + layer.toStdString() + "Ҫ�����";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}

	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("ʸ�����ݼ�Ϊ��");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 2, this, mpoDataset);
	mpAttrWidget->open();
}
	// ͳ���ܳ�
void TigerGIS::on_actionCalculatePerimeter_triggered() {
	QStringList layers = mLayerMap.keys(); // ��ȡ����ͼ��ļ��б�
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // ���ڽ����û������״̬
	// ��ȡͳ��ͼ��
	QString layer = QInputDialog::getItem(this, QStringLiteral("ѡ����Ҫͳ�Ƶ�ͼ��: "), QStringLiteral("ͼ��:"), vectorLayers, 0, false, &ok); // ��ʾ����Ի��򣬻�ȡ�û�ѡ���ͼ��
	// ��־���
	auto info = "����ͳ��ͼ��" + layer.toStdString() + "Ҫ���ܳ�";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}

	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("ʸ�����ݼ�Ϊ��");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 3, this, mpoDataset);
	mpAttrWidget->open();
}
	// ͳ�Ƴ���
void TigerGIS::on_actionCalculateLength_triggered() {
	QStringList layers = mLayerMap.keys(); // ��ȡ����ͼ��ļ��б�
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // ���ڽ����û������״̬
	// ��ȡͳ��ͼ��
	QString layer = QInputDialog::getItem(this, QStringLiteral("ѡ����Ҫͳ�Ƶ�ͼ��: "), QStringLiteral("ͼ��:"), vectorLayers, 0, false, &ok); // ��ʾ����Ի��򣬻�ȡ�û�ѡ���ͼ��
	// ��־���
	auto info = "����ͳ��ͼ��" + layer.toStdString() + "Ҫ�س���";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}
	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("ʸ�����ݼ�Ϊ��");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 4, this, mpoDataset);
	mpAttrWidget->open();
}
//�˵�--����-ʸ������--�ռ����
	// ����������
void TigerGIS::on_actionCalculateBuffer_triggered() {
	mpInput = new DataInput(ui.loggerEdit, 2, mDatasetMap,this);
	mpInput->setOpenLabelVisible_1(false);
	mpInput->setOverlayComboVisible(false);
	mpInput->setFileButtonVisible_1(false);
	mpInput->open();
}
	// ͹������
void TigerGIS::on_actionCalculateConvex_triggered() {
	mpInput = new DataInput(ui.loggerEdit, 1, mDatasetMap, this);
	mpInput->setOpenLabelVisible_1(false);
	mpInput->setOverlayComboVisible(false);
	mpInput->setFileButtonVisible_1(false);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
	// ���ӷ���--����
void TigerGIS::on_actionIntersect_triggered() {
	mpInput = new DataInput(ui.loggerEdit, mDatasetMap, this, 1);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
	// ���ӷ���--����
void TigerGIS::on_actionUnion_triggered() {
	mpInput = new DataInput(ui.loggerEdit, mDatasetMap, this, 2);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
	// ���ӷ���--����
void TigerGIS::on_actionErase_triggered() {
	mpInput = new DataInput(ui.loggerEdit, mDatasetMap, this, 3);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
//�˵�--����--դ�����
	// ���ƻҶ�ֱ��ͼ
void TigerGIS::on_actiondrawGreyHistogram_triggered(){
	QStringList layers = mLayerMap.keys(); // ��ȡ����ͼ��ļ��б�
	QStringList rasterLayers;
	for (const auto& layer : layers) {
		if (layer.endsWith(".tif") || layer.endsWith(".TIF")) {
			rasterLayers << layer;
		}
	}
	bool ok; // ���ڽ����û������״̬
	// ��ȡդ��ͼ��
	QString layer = QInputDialog::getItem(this, QStringLiteral("ѡ��դ��ͼ��: "), QStringLiteral("ͼ��:"), rasterLayers, 0, false, &ok); // ��ʾ����Ի��򣬻�ȡ�û�ѡ���ͼ��
	// ��־���
	auto info = "���ڻ���ͼ��" + layer.toStdString() + "�ĻҶ�ֱ��ͼ";
	mpLog->logInfo(info.c_str());

	if (!ok) return;
	// ��ȡդ�����ݼ�
	mpoDataset = mDatasetMap[layer];
	if (!mpoDataset) {
		QMessageBox::warning(this, "Error", "Empty dataset!");
		mpLog->logErrorInfo("����դ��Ϊ��");
		return;
	}
	GDALRasterBand* band = mpoDataset->GetRasterBand(1);
	int xSize = band->GetXSize();
	int ySize = band->GetYSize();

	std::vector<uint8_t> buffer(xSize * ySize);
	band->RasterIO(GF_Read, 0, 0, xSize, ySize, buffer.data(), xSize, ySize, GDT_Byte, 0, 0);

	std::vector<int> histogram(256, 0); // 256 bins for 8-bit grayscale
	for (int i = 0; i < buffer.size(); ++i) {
		++histogram[buffer[i]];
	}

	std::vector<int> equalHistogram = histogram;
	std::vector<int> cdf(256, 0);
	cdf[0] = equalHistogram[0];
	for (int i = 1; i < equalHistogram.size(); ++i) {
		cdf[i] = cdf[i - 1] + equalHistogram[i];
	}

	int cdf_min = *std::find_if(cdf.begin(), cdf.end(), [](int count) { return count > 0; });
	int total_pixels = xSize * ySize;

	std::vector<uint8_t> equalizedBuffer(buffer.size());
	for (int i = 0; i < buffer.size(); ++i) {
		equalizedBuffer[i] = static_cast<uint8_t>(((cdf[buffer[i]] - cdf_min) * 255.0) / (total_pixels - cdf_min));
	}

	std::fill(equalHistogram.begin(), equalHistogram.end(), 0);
	for (int i = 0; i < equalizedBuffer.size(); ++i) {
		++equalHistogram[equalizedBuffer[i]];
	}
	mpHist = new Histogram(mpLog, histogram, equalHistogram, this);
	mpHist->open();
}
	 //��Ĥ��ȡ
void TigerGIS::on_actionextractByMask_triggered() {
	mpMask = new MaskExtract(ui.loggerEdit, mDatasetMap, this);
	mpMask->open();
}
	// �������
void TigerGIS::on_actiondomainAnalysis_triggered() {
	mpNeighbor = new NeighborhoodStatistics(ui.loggerEdit, mDatasetMap, this);
	mpNeighbor->open();
}
//�˵�--����
void TigerGIS::on_actionHelp_triggered() {
	mpLog->logInfo("��ϵ����");
	QMessageBox::about(this, QStringLiteral("����"), QStringLiteral("�����Ϊ������������ƿ���ɹ�\n�й����ʴ�ѧ�人\n��������Ϣ����ѧԺ\n���䣺2942486336@qq.com"));
}

// ����ʸ��
int TigerGIS::drawShp(QString sFileName) {
	if (!mpoDataset) {
		return -1;	// ���ݼ�����ͼδ��ʼ�����ʧ�ܣ����ش���
	}
	// ����Ŀ������ο�ϵ���������ʹ��WGS84
	OGRSpatialReference oTargetSRS;
	oTargetSRS.SetWellKnownGeogCS("WGS84");
	//��ȡͼ����
	int nLayerCount = mpoDataset->GetLayerCount();
	// �Ŵ�ϵ��
	double scaleFactor = 1000.0;

	//����ÿ��ͼ���ȡҪ����
	for (int i = 0; i < nLayerCount; i++) {
		//����ͼ������
		QVector<QGraphicsItem*> layerVector;
		//��ȡͼ��
		OGRLayer* poLayer = mpoDataset->GetLayer(i);

		// ��ȡԴͶӰ
		OGRSpatialReference* sourceSRS = poLayer->GetSpatialRef();

		// ����Ŀ��ͶӰ��WGS84��
		OGRSpatialReference targetSRS;
		targetSRS.SetWellKnownGeogCS("WGS84");  // ʹ�� SetWellKnownGeogCS ��������Ϊ WGS84

		// ��������ת������
		OGRCoordinateTransformation* coordTransform = OGRCreateCoordinateTransformation(sourceSRS, &targetSRS);
		if (coordTransform == nullptr) {
			std::cerr << "Could not create coordinate transformation" << std::endl;
			GDALClose(mpoDataset);
			return 1;
		}
		//��ȡͼ���е�Ҫ�ز���ӵ�������
		poLayer->ResetReading();
		OGRFeature* feature;

		while ((feature = poLayer->GetNextFeature()) != nullptr) {

			OGRGeometry* geometry = feature->GetGeometryRef();
			if (!geometry) continue;
			if (geometry != nullptr) {
				geometry->transform(coordTransform);  // ת������
			}
			// ������Ҫ���ݼ������ʹ�����Ӧ�� QGraphicsItem
			QGraphicsItem* item = nullptr;
			//ui.graphicsView->mPen.setWidthF(0.4); // ��ʼ�������

			switch (geometry->getGeometryType()) {
			case wkbPoint: {	//��
				OGRPoint* point = geometry->toPoint();
				double x = -point->getX() * scaleFactor;
				double y = point->getY() * scaleFactor; // ��Ҫ��תY����
				auto* ellipseItem = new QGraphicsEllipseItem(y - 2*scaleFactor, x - 2*scaleFactor, 4*scaleFactor, 4*scaleFactor);
				item = ellipseItem;
				break;
			}
			case wkbLineString: {	//��
				OGRLineString* line = geometry->toLineString();
				QPainterPath path;
				if (line->getNumPoints() > 0) {
					path.moveTo(line->getY(0) * scaleFactor, -line->getX(0)* scaleFactor);
					for (int j = 1; j < line->getNumPoints(); ++j) {
						double x = -line->getX(j) * scaleFactor;
						double y = line->getY(j) * scaleFactor; // ��Ҫ��תY����
						path.lineTo(y, x);
					}
				}
				auto* pathItem = new QGraphicsPathItem(path);
				item = pathItem;

				break;
			}
			case wkbPolygon: {	//��
				OGRPolygon* polygon = geometry->toPolygon();
				OGRLinearRing* ring = polygon->getExteriorRing();
				QPolygonF qPolygon;
				for (int j = 0; j < ring->getNumPoints(); ++j) {
					double x = -ring->getX(j) * scaleFactor;
					double y = ring->getY(j) * scaleFactor; // ��Ҫ��תY����
					qPolygon << QPointF(y, x);
				}
				auto* polygonItem = new QGraphicsPolygonItem(qPolygon);
				item = polygonItem;

				break;
			}
			default:
				continue; // ��������������ʱ������
			}

			if (item) {
				// ���ͼ���ͼ����
				layerVector.push_back(item);
				//scene->addItem(item);
			}
			delete feature; // �ͷ�Ҫ�ض���
		}
		// ��ȡ�ļ���Ϣ
		QFileInfo fileInfo(sFileName);
		// ��ȡ�ļ������ļ���·��
		QString folderPath = fileInfo.absolutePath();
		// ��ȡͼ����
		const char* sLayerName = poLayer->GetName();
		mDatasetMap[sLayerName] = mpoDataset;
		mLayerMap[sLayerName] = layerVector;
		// ����ͼ��
		auto info = "���ڻ���ͼ��" + std::string(sLayerName);
		mpLog->logInfo(info.c_str());
		drawLayer(layerVector);
	}
	return 0;
}
// ����դ��
int TigerGIS::drawRaster(QString sFileName, int nRedBand, int nGreenBand, int nBlueBand) {

	if (!mpoDataset) {
		return -1; // ���ݼ�����ͼδ��ʼ�������ش���
	}

	if (mpoDataset->GetRasterCount() == 0) {
		return -1; // ���ݼ���û��դ������
	}

	//// ��ȡԴͶӰ
	//const char* projRef = mpoDataset->GetProjectionRef();
	//OGRSpatialReference sourceSRS;
	//sourceSRS.importFromWkt(projRef);

	//// ����Ŀ��ͶӰ��WGS84��
	//OGRSpatialReference targetSRS;
	//targetSRS.SetWellKnownGeogCS("WGS84");

	//// ��������ת������
	//OGRCoordinateTransformation* coordTransform = OGRCreateCoordinateTransformation(&sourceSRS, &targetSRS);
	//if (coordTransform == nullptr) {
	//	std::cerr << "Could not create coordinate transformation" << std::endl;
	//	GDALClose(mpoDataset);
	//	return 1;
	//}

	//// ����Ŀ�����ݼ�
	//GDALDataset* pWarpedDataset = static_cast<GDALDataset*>(GDALAutoCreateWarpedVRT(mpoDataset, projRef, targetSRS.exportToWkt().c_str(), GRA_NearestNeighbour, 0.0, nullptr));
	//if (pWarpedDataset == nullptr) {
	//	std::cerr << "Could not create warped dataset" << std::endl;
	//	OGRCoordinateTransformation::DestroyCT(coordTransform);
	//	GDALClose(mpoDataset);
	//	return 1;
	//}

	// ��ȡդ��ߴ��벨����
	int width = mpoDataset->GetRasterXSize();
	int height = mpoDataset->GetRasterYSize();
	int bands = mpoDataset->GetRasterCount();

	// ȷ��ͼ���ʽ
	QImage::Format format;
	if (bands == 1) {
		format = QImage::Format_Grayscale8; // ������
	}
	else if (bands >= 3) {
		format = QImage::Format_RGB32; // RGB����
	}
	else {
		return -1; // ��������ݲ�����
	}

	//mpoDataset = pWarpedDataset;

	int redBand = 0;
	int greenBand = 0;
	int blueBand = 0;
	if (bands >= 3) {
		redBand = nRedBand;
		greenBand = nGreenBand;
		blueBand = nBlueBand;
	}

	// ��ȡդ��ĵ���任����
	double geoTransform[6];
	mpoDataset->GetGeoTransform(geoTransform);
	// �������Χ
	double minGeoX = geoTransform[0];
	double maxGeoX = geoTransform[0] + width * geoTransform[1] + height * geoTransform[2];
	double minGeoY = geoTransform[3] + width * geoTransform[4] + height * geoTransform[5];
	double maxGeoY = geoTransform[3];

	// ����һ���յ� QImage
	QImage image(width, height, format);

	// ��ȡ��������
	std::vector<std::vector<double>> bandData(bands, std::vector<double>(width * height));

	for (int i = 0; i < bands; ++i) {
		GDALRasterBand* band = mpoDataset->GetRasterBand(i + 1);
		GDALDataType dataType = band->GetRasterDataType();
		switch (dataType) {
		case GDT_Byte: {	// Byte
			std::vector<unsigned char> buffer(width * height);
			band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(), width, height, GDT_Byte, 0, 0);
			std::transform(buffer.begin(), buffer.end(), bandData[i].begin(), [](unsigned char val) { return static_cast<double>(val); });
			break;
		}
		case GDT_UInt16: {	// UInt16
			std::vector<unsigned short> buffer(width * height);
			band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(), width, height, GDT_UInt16, 0, 0);
			std::transform(buffer.begin(), buffer.end(), bandData[i].begin(), [](unsigned short val) { return static_cast<double>(val); });
			break;
		}
		case GDT_Int16: {	// Int16
			std::vector<short> buffer(width * height);
			band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(), width, height, GDT_Int16, 0, 0);
			std::transform(buffer.begin(), buffer.end(), bandData[i].begin(), [](short val) { return static_cast<double>(val); });
			break;
		}
		case GDT_UInt32: {	// UInt32
			std::vector<unsigned int> buffer(width * height);
			band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(), width, height, GDT_UInt32, 0, 0);
			std::transform(buffer.begin(), buffer.end(), bandData[i].begin(), [](unsigned int val) { return static_cast<double>(val); });
			break;
		}
		case GDT_Int32: {	// Int32
			std::vector<int> buffer(width * height);
			band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(), width, height, GDT_Int32, 0, 0);
			std::transform(buffer.begin(), buffer.end(), bandData[i].begin(), [](int val) { return static_cast<double>(val); });
			break;
		}
		case GDT_Float32: {	// Float32
			std::vector<float> buffer(width * height);
			band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(), width, height, GDT_Float32, 0, 0);
			std::transform(buffer.begin(), buffer.end(), bandData[i].begin(), [](float val) { return static_cast<double>(val); });
			break;
		}
		case GDT_Float64: {	// Float64
			std::vector<double> buffer(width * height);
			band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(), width, height, GDT_Float64, 0, 0);
			bandData[i] = buffer;
			break;
		}
		default:
			return -1; // ��֧�ֵ���������
		}
	}

	// Ѱ�����в��ε�������Сֵ���Ա���й�һ��
	double minValue = *std::min_element(bandData[0].begin(), bandData[0].end());
	double maxValue = *std::max_element(bandData[0].begin(), bandData[0].end());
	for (int i = 1; i < bands; ++i) {
		minValue = (((minValue) < (*std::min_element(bandData[i].begin(), bandData[i].end()))) ? (minValue) : (*std::min_element(bandData[i].begin(), bandData[i].end())));
		maxValue = (((maxValue) > (*std::max_element(bandData[i].begin(), bandData[i].end()))) ? (maxValue) : (*std::max_element(bandData[i].begin(), bandData[i].end())));
	}

	// ���������ݹ�һ���������� QImage
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (bands >= 3) {
				int r = static_cast<int>(255 * (bandData[redBand - 1][y * width + x] - minValue) / (maxValue - minValue));
				int g = static_cast<int>(255 * (bandData[greenBand - 1][y * width + x] - minValue) / (maxValue - minValue));
				int b = static_cast<int>(255 * (bandData[blueBand - 1][y * width + x] - minValue) / (maxValue - minValue));
				image.setPixel(x, y, qRgb(r, g, b));
			}
			else if (bands == 1) {
				int gray = static_cast<int>(255 * (bandData[0][y * width + x] - minValue) / (maxValue - minValue));
				image.setPixel(x, y, qRgb(gray, gray, gray));
			}
		}
	}

	QVector<QGraphicsItem*> rasterLayer;
	QImage newImage = image.scaled((maxGeoX - minGeoX) * 1000, (maxGeoY - minGeoY) * 1000);
	auto item = new QGraphicsPixmapItem(QPixmap::fromImage(newImage));
	item->setPos(minGeoX * 1000, -maxGeoY * 1000);

	rasterLayer.push_back(item);
	// ��ȡ�ļ���Ϣ
	QFileInfo fileInfo(sFileName);
	// ��ȡ�ļ������ļ���·��
	QString folderPath = fileInfo.absolutePath();
	// ��ȡͼ����
	QString sLayerName = fileInfo.fileName();
	mDatasetMap[sLayerName] = mpoDataset;
	mLayerMap[sLayerName] = rasterLayer;
	// ����ͼ��
	auto info = "���ڻ���ͼ��" + sLayerName.toStdString();
	mpLog->logInfo(info.c_str());
	drawLayer(rasterLayer);
	
	return 0; // �ɹ�
}
// ����wkt
int TigerGIS::drawGeometry(OGRGeometry* poGeometry) {
	if (!poGeometry) {
		return -1;  // ��ͼ�򼸺ζ���δ��ʼ������Ч�����ش���
	}

	// �Ŵ�ϵ��
	double scaleFactor = 1000.0;

	QGraphicsItem* item = nullptr;

	switch (poGeometry->getGeometryType()) {
	case wkbPoint: {
		OGRPoint* point = poGeometry->toPoint();
		double x = point->getX() * scaleFactor;
		double y = -point->getY() * scaleFactor; // ��Ҫ��תY����
		auto ellipseItem = new QGraphicsEllipseItem(x - 2, y - 2, 4, 4);
		item = ellipseItem;
		break;
	}
	case wkbLineString: {
		OGRLineString* line = poGeometry->toLineString();
		QPolygonF polygon;
		for (int j = 0; j < line->getNumPoints(); ++j) {
			double x = line->getX(j) * scaleFactor;
			double y = -line->getY(j) * scaleFactor; // ��Ҫ��תY����
			polygon << QPointF(x, y);
		}
		auto polygonItem = new QGraphicsPolygonItem(polygon);
		item = polygonItem;
		break;
	}
	case wkbPolygon: {
		OGRPolygon* polygon = poGeometry->toPolygon();
		OGRLinearRing* ring = polygon->getExteriorRing();
		QPolygonF qPolygon;
		for (int j = 0; j < ring->getNumPoints(); ++j) {
			double x = ring->getX(j) * scaleFactor;
			double y = -ring->getY(j) * scaleFactor; // ��Ҫ��תY����
			qPolygon << QPointF(x, y);
		}
		auto qpolygonItem = new QGraphicsPolygonItem(qPolygon);
		item = qpolygonItem;
		break;
	}
	default:
		return -1; // ��������������ʱ���������ش���
	}

	if (item) {
		// ���ͼ���������
		mpScene->addItem(item);
	}

	// ���ó����ı߽磬��ȷ������Ҫ�ض�����ȷ��ʾ
	QRectF sceneRect = mpScene->itemsBoundingRect();
	mpScene->setSceneRect(sceneRect);

	// ������ͼ��Χ����������
	ui.graphicsView->fitInView(sceneRect, Qt::KeepAspectRatio); // ����ͼ���������ʵķ�Χ

	// ��������ӵ���ͼ����ʾ
	ui.graphicsView->setScene(mpScene);
	ui.graphicsView->show();

	return 0;
}

//���դ��ͼ���������б�
int TigerGIS::addRasterContent(QString sFileName) {
	// ��ȡ�ļ���Ϣ
	QFileInfo fileInfo(sFileName);
	// ��ȡ�ļ������ļ���·��
	QString folderPath = fileInfo.absolutePath();
	//��ȡ�ļ���
	QString fileName = fileInfo.fileName();

	//�ж��Ƿ��Ѿ����ڸ��ļ���·��
	for (int i = 0; i < mpRootNode->rowCount(); ++i) {
		QStandardItem* childItem = mpRootNode->child(i);
		if (fileInfo.absolutePath() == childItem->text()) {
			//���ļ���������ļ�Ŀ¼
			QStandardItem* newSonOfItem = new QStandardItem(fileName);
			//���ýڵ�checkBox
			newSonOfItem->setCheckable(true);
			newSonOfItem->setCheckState(Qt::Checked);
			childItem->appendRow(newSonOfItem);

			auto info = "��ͼ��" + fileName.toStdString() + "����������б�";
			mpLog->logInfo(info.c_str());
			return 0;
		}

	}
	//����ļ�Ŀ¼�������б�
	QStandardItem* newItem = new QStandardItem(folderPath);
	//���ļ���������ļ�Ŀ¼
	QStandardItem* newSonOfItem = new QStandardItem(fileName);
	//���ýڵ�checkBox
	newSonOfItem->setCheckable(true);
	newSonOfItem->setCheckState(Qt::Checked);

	newItem->appendRow(newSonOfItem);
	mpRootNode->appendRow(newItem);

	auto info = "��ͼ��" + fileName.toStdString() + "����������б�";
	mpLog->logInfo(info.c_str());

	return 0;
}
//���ʸ��ͼ���������б�
int TigerGIS::addShpContent(QString sFileName) {
	// ��ȡ�ļ���Ϣ
	QFileInfo fileInfo(sFileName);
	// ��ȡ�ļ������ļ���·��
	QString folderPath = fileInfo.absolutePath();
	// ��ȡͼ����
	QString sLayerName = fileInfo.baseName();
	//�ж��Ƿ��Ѿ����ڸ��ļ���·��
	for (int i = 0; i < mpRootNode->rowCount(); ++i) {
		QStandardItem* childItem = mpRootNode->child(i);
		if (fileInfo.absolutePath() == childItem->text()) {
			//��ͼ��������ļ�Ŀ¼
			QStandardItem* newSonOfItem = new QStandardItem(sLayerName);
			//���ýڵ��checkBox
			newSonOfItem->setCheckable(true);
			newSonOfItem->setCheckState(Qt::Checked);

			childItem->appendRow(newSonOfItem);
			auto info = "��ͼ��" + sLayerName.toStdString() + "����������б�";
			mpLog->logInfo(info.c_str());
			return 0;
		}

	}
	//����ļ�Ŀ¼�������б�
	QStandardItem* newItem = new QStandardItem(folderPath);
	//���ļ���������ļ�Ŀ¼
	QStandardItem* newSonOfItem = new QStandardItem(sLayerName);
	//���ýڵ��checkBox
	newSonOfItem->setCheckable(true);
	newSonOfItem->setCheckState(Qt::Checked);

	newItem->appendRow(newSonOfItem);
	mpRootNode->appendRow(newItem);

	auto info = "��ͼ��" + sLayerName.toStdString() + "����������б�";
	mpLog->logInfo(info.c_str());

	return 0;
}
//����ͼ��
int TigerGIS::drawLayer(QVector<QGraphicsItem*> layer) {
	resetView();
	for (auto& item : layer) {
		if (auto ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(item)) {
			// �޸�����ɫ
			ellipseItem->setPen(ui.graphicsView->mPen);

			// �޸����ɫ
			ellipseItem->setBrush(ui.graphicsView->mBrush);
		}
		else if (auto polygonItem = dynamic_cast<QGraphicsPolygonItem*>(item)) {
			// �޸�����ɫ
			polygonItem->setPen(ui.graphicsView->mPen);

			// �޸����ɫ
			polygonItem->setBrush(ui.graphicsView->mBrush);
		}
		else if (auto pathItem = dynamic_cast<QGraphicsPathItem*>(item)) {
			ui.graphicsView->mPen.setWidth(0.4);
			pathItem->setPen(ui.graphicsView->mPen);
		}
		mpScene->addItem(item);
	}
	// ��־��¼
	mpLog->logInfo("ͼ��������");
	// ���ó����ı߽磬��ȷ������Ҫ�ض�����ȷ��ʾ

	// ��������ӵ���ͼ����ʾ
	ui.graphicsView->setScene(mpScene);

	// ������ͼ��Χ����������
	ui.graphicsView->fitInView(ui.graphicsView->scene()->itemsBoundingRect(), Qt::KeepAspectRatio); // ����ͼ���������ʵķ�Χ

	ui.graphicsView->show();
	return 0;
}
//�Ƴ�ͼ��
int TigerGIS::removeLayer(QVector<QGraphicsItem*> layer) {
	for (const auto& item : layer) {
		mpScene->removeItem(item);
	}
	// ���ó����ı߽磬��ȷ������Ҫ�ض�����ȷ��ʾ
	QRectF sceneRect = mpScene->itemsBoundingRect();
	mpScene->setSceneRect(sceneRect);

	// ������ͼ��Χ����������
	ui.graphicsView->fitInView(sceneRect, Qt::KeepAspectRatio); // ����ͼ���������ʵķ�Χ

	// ��������ӵ���ͼ����ʾ
	ui.graphicsView->setScene(mpScene);
	ui.graphicsView->show();
	return 0;
}

//��λ����
void TigerGIS::resetView() {
	ui.graphicsView->setCurrentScale(1.0);		// ���õ�ǰ����
	ui.graphicsView->resetTransform();			// ������ͼ�ı任����
	if (ui.graphicsView->scene() == nullptr) {
		return;
	}
	ui.graphicsView->fitInView(ui.graphicsView->scene()->itemsBoundingRect(), Qt::KeepAspectRatio); // ������ͼ����Ӧ����
	mpLog->logInfo("��λ");
}

//����Ŀ¼�еĻ�ȡ��ѡ�ڵ�
QVector<QString> TigerGIS::getCheckedItem() {
	QVector<QString> vsCheckedItem;
	for (int i = 0; i < mpRootNode->rowCount(); ++i) {
		QStandardItem* childItem = mpRootNode->child(i);
		for (int j = 0; j < childItem->rowCount(); ++j) {
			QStandardItem* item = childItem->child(j);
			if (item->checkState() == Qt::Checked) {
				vsCheckedItem.push_back(item->text());
			}
		}
	}
	return vsCheckedItem;
}

//��������ͼ������ɫ����ɫ
void TigerGIS::selectAllColor() {
	// ��־���
	mpLog->logInfo("��������ͼ���������");
	// ��ȡ��ɫ
	QColor fillColor = QColorDialog::getColor(Qt::green, this, tr(QString::fromLocal8Bit("�������ɫ").toUtf8().constData()));
	QColor outlineColor = QColorDialog::getColor(Qt::black, this, tr(QString::fromLocal8Bit("��������ɫ").toUtf8().constData()));
	if (fillColor.isValid() && outlineColor.isValid()) {
		ui.graphicsView->mBrush.setColor(fillColor);
		ui.graphicsView->mPen.setColor(outlineColor);
		auto ret = getCheckedItem();
		if (!ret.isEmpty()) {
			for (const auto& item : ret) {
				removeLayer(mLayerMap[item]);
				drawLayer(mLayerMap[item]); // ���»���
			}
		}
	}
}
//����ѡ��ͼ������ɫ����ɫ
void TigerGIS::selectColor(QString sLayerName) {
	// ��־���
	auto info = "����ͼ��" + sLayerName.toStdString() + "��������";
	mpLog->logInfo(info.c_str());
	// ��ȡ��ɫ
	QColor fillColor = QColorDialog::getColor(Qt::green, this, tr(QString::fromLocal8Bit("�������ɫ").toUtf8().constData()));
	QColor outlineColor = QColorDialog::getColor(Qt::black, this, tr(QString::fromLocal8Bit("��������ɫ").toUtf8().constData()));
	if (fillColor.isValid() && outlineColor.isValid()) {
		ui.graphicsView->mBrush.setColor(fillColor);
		ui.graphicsView->mPen.setColor(outlineColor);
		auto selectLayer = mLayerMap[sLayerName];
		if (!selectLayer.isEmpty()) {
			removeLayer(selectLayer);
			drawLayer(selectLayer); // ���»���
		}
	}
}

//����༭Ϊ�ļ�
void TigerGIS::saveChanges() {
	QString filePath = QFileDialog::getSaveFileName(nullptr, QStringLiteral("����Ϊʸ���ļ�"), "./output", "Shapefile (*.shp)");
	// ��־���
	auto info = "���ڱ���༭�ļ���" + filePath.toStdString();
	mpLog->logInfo(info.c_str());
	// �쳣����
	if (filePath.isEmpty()) {
		// �û�ȡ�����ļ�ѡ��
		mpLog->logInfo("ȡ������");
		return;
	}

	// ��ȡͼ��
	OGRLayer* poLayer = mpoDataset->GetLayer(0);
	poLayer->ResetReading();
	if (poLayer == nullptr) {
		mpLog->logErrorInfo("ͼ�㴴��ʧ��");
		GDALClose(mpoDataset);
		return;
	}

	// ��ȡͼ��Ŀռ�ο�
	OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
	if (poSRS == nullptr) {
		mpLog->logWarnInfo("�ռ�ο���ȡʧ��");
		GDALClose(mpoDataset);
		return;
	}

	// ����Ŀ��ͶӰ��WGS84��
	OGRSpatialReference targetSRS;
	targetSRS.SetWellKnownGeogCS("WGS84");  // ʹ�� SetWellKnownGeogCS ��������Ϊ WGS84

	// �����������Դ
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == nullptr) {
		auto info = std::string(pszDriverName) + "����������.";
		mpLog->logErrorInfo(info.c_str());
		GDALClose(mpoDataset);
		return;
	}

	auto temp1 = filePath.toUtf8();
	auto temp2 = temp1.constData();
	const char* pszOutputFilePath = temp2;
	GDALDataset* poDS_out = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, nullptr);
	if (poDS_out == nullptr) {
		mpLog->logErrorInfo("�������Դ����ʧ��.");
		GDALClose(mpoDataset);
		return;
	}

	// ��ȡ��ǰͼ���ͼ�����б�
	auto tempItems = ui.graphicsView->mvTempItems;
	QGraphicsScene* scene = ui.graphicsView->scene();
	QString currentFeatureType = ui.graphicsView->msCurrentFeatureType;

	// ��ȡ�����ı߽���Σ����ڼ��㷭ת��y����
	QRectF sceneRect = scene->sceneRect();

	// ������ͬ���͵�ͼ��
	if (currentFeatureType == "Point") {
		OGRLayer* pointLayer = poDS_out->CreateLayer("point_layer", &targetSRS, wkbPoint, nullptr);
		if (pointLayer == nullptr) {
			mpLog->logErrorInfo("��ͼ�㴴��ʧ��");
			GDALClose(mpoDataset);
			GDALClose(poDS_out);
			return;
		}

		// ��ȡ��ǰ�����ľ���
		QRectF sceneRect = scene->sceneRect();

		// ����ͼ�����б���ӵ�ͼ����
		for (QGraphicsItem* item : tempItems) {
			// �����ͼ����
			QGraphicsEllipseItem* ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(item);
			if (currentFeatureType == "Point") {
				//qDebug() << "Processing Point";
				QRectF rect = ellipseItem->rect();
				QPointF center = rect.center();
				OGRPoint ogrPoint;

				// ���õ�����겢��תy��
				double adjustedY = sceneRect.height() - center.y() - sceneRect.height();
				ogrPoint.setY(center.x() / 1000);
				ogrPoint.setX(adjustedY / 1000);

				// ����Ҫ�ز���ӵ�ͼ��
				OGRFeature* feature = OGRFeature::CreateFeature(pointLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPoint);
				if (pointLayer->CreateFeature(feature) != OGRERR_NONE) {
					// ����Ҫ�ش���ʧ�ܵ����
					mpLog->logErrorInfo("�޷�������Ҫ��");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
		for (QGraphicsItem* item : mvEditItems) {
			// �����ͼ����
			QGraphicsEllipseItem* ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(item);
			if (currentFeatureType == "Point") {
				// qDebug() << "Processing Point";
				QRectF rect = ellipseItem->rect();
				QPointF center = rect.center();
				OGRPoint ogrPoint;

				// ���õ�����겢��תy��
				double adjustedY = sceneRect.height() - center.y() - sceneRect.height();
				ogrPoint.setY(center.x() / 1000);
				ogrPoint.setX(adjustedY / 1000);

				// ����Ҫ�ز���ӵ�ͼ��
				OGRFeature* feature = OGRFeature::CreateFeature(pointLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPoint);
				if (pointLayer->CreateFeature(feature) != OGRERR_NONE) {
					// ����Ҫ�ش���ʧ�ܵ����
					mpLog->logErrorInfo("�޷�������Ҫ��");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
	}
	else if (currentFeatureType == "Line") {
		OGRLayer* lineLayer = poDS_out->CreateLayer("line_layer", &targetSRS, wkbLineString, NULL);
		if (lineLayer == nullptr) {
			mpLog->logErrorInfo("��ͼ�㴴��ʧ��");
			GDALClose(mpoDataset);
			GDALClose(poDS_out);
			return;
		}

		// ��ȡ��ǰ�����ľ���
		QRectF sceneRect = scene->sceneRect();

		// ����ͼ�����б���ӵ�ͼ����
		for (QGraphicsItem* item : tempItems) {
			// ������ͼ����
			QGraphicsLineItem* lineItem = dynamic_cast<QGraphicsLineItem*>(item);
			if (currentFeatureType == "Line") {
				// qDebug() << "Processing Line";
				QLineF line = lineItem->line();
				OGRLineString ogrLine;

				// ����ߵ������յ㣬����תy��
				double adjustedY1 = sceneRect.height() - line.y1() - sceneRect.height();
				double adjustedY2 = sceneRect.height() - line.y2() - sceneRect.height();
				ogrLine.addPoint(line.x1() / 1000, adjustedY1 / 1000);
				ogrLine.addPoint(line.x2() / 1000, adjustedY2 / 1000);

				// ����Ҫ�ز���ӵ�ͼ��
				OGRFeature* feature = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
				feature->SetGeometry(&ogrLine);
				if (lineLayer->CreateFeature(feature) != OGRERR_NONE) {
					// ����Ҫ�ش���ʧ�ܵ����
					mpLog->logErrorInfo("�޷�������Ҫ��");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
		for (QGraphicsItem* item : mvEditItems) {
			// ������ͼ����
			QGraphicsPathItem* pathItem = dynamic_cast<QGraphicsPathItem*>(item);
			if (currentFeatureType == "Line") {
				// qDebug() << "Processing Line";
				auto path = pathItem->path();
				OGRLineString ogrLine;

				// ���·���е����е㣬����תy��
				for (int i = 0; i < path.elementCount(); ++i) {
					auto point = path.elementAt(i);
					double adjustedY = sceneRect.height() - point.y - sceneRect.height();
					ogrLine.addPoint(point.x / 1000, adjustedY / 1000);
				}

				// ����Ҫ�ز���ӵ�ͼ��
				OGRFeature* feature = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
				feature->SetGeometry(&ogrLine);
				if (lineLayer->CreateFeature(feature) != OGRERR_NONE) {
					// ����Ҫ�ش���ʧ�ܵ����
					mpLog->logErrorInfo("�޷�������Ҫ��");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
	}
	else if (currentFeatureType == "Polygon") {
		OGRLayer* polygonLayer = poDS_out->CreateLayer("polygon_layer", &targetSRS, wkbPolygon, NULL);
		if (polygonLayer == NULL) {
			mpLog->logErrorInfo("�����ͼ�㴴��ʧ��.");
			GDALClose(mpoDataset);
			GDALClose(poDS_out);
			return;
		}

		// ��ȡ��ǰ�����ľ���
		QRectF sceneRect = scene->sceneRect();

		// ����ͼ�����б���ӵ�ͼ����
		for (QGraphicsItem* item : tempItems) {
			// ��������ͼ����
			QGraphicsPolygonItem* polygonItem = dynamic_cast<QGraphicsPolygonItem*>(item);
			if (currentFeatureType == "Polygon") {
				// qDebug() << "Processing Polygon";
				QPolygonF polygon = polygonItem->polygon().toPolygon();
				OGRPolygon ogrPolygon;
				OGRLinearRing ogrRing;

				// ��Ӷ���εĵ㵽���Ի�������תy��
				for (int i = 0; i < polygon.size(); ++i) {
					QPointF point = polygon.at(i);
					// ��תy����ټ�ȥ�����߶�
					double adjustedY = sceneRect.height() - point.y() - sceneRect.height();
					ogrRing.addPoint(point.x() / 1000, adjustedY / 1000);
				}

				// �����Ի���ӵ������
				ogrPolygon.addRing(&ogrRing);

				// ����Ҫ�ز���ӵ�ͼ��
				OGRFeature* feature = OGRFeature::CreateFeature(polygonLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPolygon);
				if (polygonLayer->CreateFeature(feature) != OGRERR_NONE) {
					// ����Ҫ�ش���ʧ�ܵ����
					mpLog->logErrorInfo("�޷�������Ҫ��");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
		for (QGraphicsItem* item : mvEditItems) {
			// ��������ͼ����
			QGraphicsPolygonItem* polygonItem = dynamic_cast<QGraphicsPolygonItem*>(item);
			if (currentFeatureType == "Polygon") {
				// qDebug() << "Processing Polygon";
				QPolygonF polygon = polygonItem->polygon().toPolygon();
				OGRPolygon ogrPolygon;
				OGRLinearRing ogrRing;

				// ��Ӷ���εĵ㵽���Ի�������תy��
				for (int i = 0; i < polygon.size(); ++i) {
					QPointF point = polygon.at(i);
					// ��תy����ټ�ȥ�����߶�
					double adjustedY = sceneRect.height() - point.y() - sceneRect.height();
					ogrRing.addPoint(point.x() / 1000, adjustedY / 1000);
				}

				// �����Ի���ӵ������
				ogrPolygon.addRing(&ogrRing);

				// ����Ҫ�ز���ӵ�ͼ��
				OGRFeature* feature = OGRFeature::CreateFeature(polygonLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPolygon);
				if (polygonLayer->CreateFeature(feature) != OGRERR_NONE) {
					// ����Ҫ�ش���ʧ�ܵ����
					mpLog->logErrorInfo("�޷�������Ҫ��");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
	}

	// �ͷ���Դ
	GDALClose(poDS_out);

	for (QGraphicsItem* item : qAsConst(ui.graphicsView->mvTempItems)) { // ������ʱͼ�����б�
		mpScene->removeItem(item); // �ӳ������Ƴ�ͼ����
		delete item; // ɾ��ͼ����
	}
	ui.graphicsView->mvTempItems.clear(); // �����ʱͼ�����б�
	mpLog->logInfo("�༭�ļ�����ɹ�");
}
//ȡ������
void TigerGIS::discardChanges() {
	for (QGraphicsItem* item : qAsConst(ui.graphicsView->mvTempItems)) { // ������ʱͼ�����б�
		mpScene->removeItem(item); // �ӳ������Ƴ�ͼ����
		delete item; // ɾ��ͼ����
	}
	ui.graphicsView->mvTempItems.clear(); // �����ʱͼ�����б�
	mpLog->logInfo("ȡ������༭�ļ�");
}