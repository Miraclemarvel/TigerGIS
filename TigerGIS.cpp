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
// 构造函数
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
	// 设置应用程序图标
	setWindowIcon(QIcon("./res/Tiger.png"));
	mpLog = new logger(ui.loggerEdit);
	ui.statusBar->setStyleSheet("QStatusBar::item{border:0px}");
	// 连接鼠标移动信号到槽函数
	connect(ui.graphicsView, &ZoomableGraphicsView::mouseMoved, this, &TigerGIS::updateStatusBar);
	// 设置内容列表的列标题（根目录：图层）
	mpFileList->setHorizontalHeaderLabels(QStringList() << QStringLiteral("图层"));
	ui.contentTree->setModel(mpFileList);
	mpRootNode = mpFileList->invisibleRootItem();

	// 内容列表图层勾选显示
	connect(mpFileList, &QStandardItemModel::itemChanged, [=](QStandardItem* item) {
		if (item->isCheckable()) {
			Qt::CheckState state = item->checkState();
			// 根据状态进行交互
			if (state == Qt::Checked) {
				auto info = "显示图层" + item->text().toStdString();
				mpLog->logInfo(info.c_str());
				drawLayer(mLayerMap[item->text()]);
			}
			else {
				// 执行未选中时的操作
				auto info = "隐藏图层" + item->text().toStdString();
				mpLog->logInfo(info.c_str());
				removeLayer(mLayerMap[item->text()]);
			}
		}
	});
	// 内容列表添加右键菜单
	connect(ui.contentTree, &QTreeView::customContextMenuRequested, [=](const QPoint& pos) {
		QModelIndex index = ui.contentTree->indexAt(pos);	//获取点击的节点
			if (index.isValid()) {
				QStandardItemModel* model = static_cast<QStandardItemModel*>(ui.contentTree->model());
				QStandardItem* item = model->itemFromIndex(index);

				if (item && item->rowCount() == 0) { // 获取打开的文件节点
					QMenu contextMenu;	//右键菜单
					QAction* actionDel = new QAction(QStringLiteral("移除图层"), this);	//移除图层
					QAction* actionSym = new QAction(QStringLiteral("符号属性"), this); //符号系统
					QAction* actionTFD = new QAction(QStringLiteral("真/假彩色显示"), this); //栅格真假彩色显示
					//设置图标
					actionDel->setIcon(QIcon("./res/remove.png")); 
					actionSym->setIcon(QIcon("./res/symbol.png"));
					actionTFD->setIcon(QIcon("./res/RGB.png"));
					//连接菜单项与交互功能
					connect(actionDel, &QAction::triggered, this, [&]() {
						//移除视图显示
						removeLayer(mLayerMap[item->text()]);
						//释放内存
						for (auto& item : mLayerMap[item->text()]) {
							delete item;
							item = nullptr;
						}
						mLayerMap.remove(item->text());
						// 日志输出
						auto info = "移除图层" + item->text().toStdString();
						mpLog->logInfo(info.c_str());
						//关闭数据集
						GDALClose(mDatasetMap[item->text()]);
						//移除数据集
						mDatasetMap.remove(item->text());
						//移除内容列表
						QModelIndex parentIndex = index.parent();
						model->removeRow(index.row(), parentIndex);
						//检查打开的文件夹是否为空
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
						// 日志输出
						auto info = "选择图层" + layerName.toStdString() + "的波段组合";
						mpLog->logInfo(info.c_str());
						if (!mpoDataset) {
							return; // 数据集或视图未初始化，返回错误
						}

						if (mpoDataset->GetRasterCount() == 0) {
							return; // 数据集中没有栅格数据
						}
						//获取波段数
						int bands = mpoDataset->GetRasterCount();
						// 获取所有波段的名字
						QStringList bandNames;
						for (int i = 1; i <= bands; ++i) {
							GDALRasterBand* band = mpoDataset->GetRasterBand(i);
							const char* description = band->GetDescription();
							if (description && description[0] != '\0') {
								bandNames << QString::fromUtf8(description);
							}
							else {
								// 如果描述为空，可以使用波段索引作为名称
								bandNames << QString("Band %1").arg(i);
							}
						}
						int redBand = 0;
						int greenBand = 0;
						int blueBand = 0;
						if (bands >= 3) {
							// 获取用户输入的波段组合
							bool ok;
							QString redBandName = QInputDialog::getItem(nullptr, QStringLiteral("选择红色波段"), QStringLiteral("从波段列表中选择红色波段"), bandNames, 0, false, &ok);
							if (!ok) return;
							QString greenBandName = QInputDialog::getItem(nullptr, QStringLiteral("选择绿色波段"), QStringLiteral("从波段列表中选择绿色波段"), bandNames, 0, false, &ok);
							if (!ok) return;
							QString blueBandName = QInputDialog::getItem(nullptr, QStringLiteral("选择蓝色波段"), QStringLiteral("从波段列表中选择蓝色波段"), bandNames, 0, false, &ok);
							if (!ok) return;
							redBand = bandNames.indexOf(redBandName) + 1;
							greenBand = bandNames.indexOf(greenBandName) + 1;
							blueBand = bandNames.indexOf(blueBandName) + 1;
						}
						else {
							mpLog->logWarnInfo("波段数为1，无法进行通道组合");
							return;
						}
						removeLayer(mLayerMap[layerName]);
						//释放内存
						for (auto& item : mLayerMap[layerName]) {
							delete item;
							item = nullptr;
						}
						mLayerMap.remove(item->text());
						drawRaster(mpoDataset->GetDescription(), redBand, greenBand, blueBand);
					});
					//添加菜单项
					contextMenu.addAction(actionDel); //移除图层
					contextMenu.addAction(actionSym); //符号系统
					contextMenu.addAction(actionTFD); //栅格真假彩色显示
					//限定功能范围
					if (index.data().toString().endsWith(".tif") || index.data().toString().endsWith(".TIF")) {
						actionSym->setEnabled(false);
					}
					else {
						actionTFD->setEnabled(false);
					}
					// 显示上下文菜单在鼠标点击的位置
					contextMenu.exec(ui.contentTree->viewport()->mapToGlobal(pos));
				}
			}
	});

	// 设置最小和最大缩放比例
	ui.graphicsView->setMinScale(0.5);	// 最小缩放比例
	ui.graphicsView->setMaxScale(20);	// 最大缩放比例

	// 连接缩放变化信号到槽
	connect(ui.graphicsView, &ZoomableGraphicsView::scaleChanged, [this]() {
		ui.graphicsView->updateSceneScale();
		});
	// 连接复位按钮到槽
	connect(ui.resetBtn, &QPushButton::clicked, this, &TigerGIS::resetView);

	OGRRegisterAll();	//注册GDAL驱动

	//打开wkt文件
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

//符号系统
	//设置填充轮廓颜色
	connect(ui.symbolBtn, &QPushButton::clicked, this, &TigerGIS::selectAllColor);
}

// 析构函数 
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

// 更新状态栏显示鼠标坐标
void TigerGIS::updateStatusBar(const QPointF& pos) {
	// 更新状态栏
	ui.graphicsView->mpStatusLabel->setText(QString("%1, %2 %3")
		.arg(QString::number(pos.x() / 1000.0, 'f', 3))
		.arg(QString::number(-pos.y() / 1000.0, 'f', 3))
		.arg(QStringLiteral("十进制度")));
	ui.statusBar->addPermanentWidget(ui.graphicsView->mpStatusLabel);

}
// 菜单--文件
	//打开矢量数据
void TigerGIS::on_actionShpFile_triggered() {
	// 获取文件路径(文件名)
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		"./data",
		tr("Images (*.shp)"));
	// 输出日志
	auto info = "打开矢量文件" + sFileName.toStdString();
	mpLog->logInfo(info.c_str());
	// 打开矢量数据集
	mpoDataset = static_cast<GDALDataset*>(GDALOpenEx(sFileName.toUtf8().constData(), GDAL_OF_VECTOR, NULL, NULL, NULL));
	if (mpoDataset == nullptr) {
		// 输出日志
		mpLog->logErrorInfo("文件打开失败");
		return;
	}
	else {
		//输出日志
		mpLog->logInfo("文件打开成功，正在绘制图层...");
		addShpContent(sFileName);
	}
	ui.graphicsView->setCurrentScale(1.0);
	drawShp(sFileName);
}
	//打开栅格数据
void TigerGIS::on_actionRasterFile_triggered() {
	// 获取文件路径(文件名）
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		"./data",
		tr("Images (*.tif *.TIF)"));
	// 输出日志
	auto info = "打开栅格文件" + sFileName.toStdString();
	mpLog->logInfo(info.c_str());
	// 打开栅格数据集
	mpoDataset = (GDALDataset*)GDALOpenEx(sFileName.toUtf8().constData(), GDAL_OF_READONLY, NULL, NULL, NULL);
	if (mpoDataset == nullptr) {
		// 输出日志
		mpLog->logErrorInfo("文件打开失败");
		return;
	}
	else {
		//输出日志
		mpLog->logInfo("文件打开成功，正在绘制图层...");
		addRasterContent(sFileName);
	}
	ui.graphicsView->setCurrentScale(1.0);
	drawRaster(sFileName);
}
	// 打开大数据量栅格文件
void TigerGIS::on_actionbigRaster_triggered() {
	// 获取文件路径
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		"./data",
		tr("Images (*.tif *.TIF)"));
	// 输出日志
	auto info = "打开超大数据量栅格文件" + sFileName.toUtf8();
	mpLog->logInfo(info);
	// 打开栅格数据集
	mpoDataset = (GDALDataset*)GDALOpenEx(sFileName.toUtf8().constData(), GDAL_OF_READONLY, NULL, NULL, NULL);
	if (mpoDataset == nullptr) {
		// 输出日志
		mpLog->logErrorInfo("文件打开失败");
		return;
	}
	else {
		//输出日志
		mpLog->logInfo("文件打开成功，正在绘制图层...");
		addRasterContent(sFileName);
	}
	//获取栅格文件基本数据
	int width = mpoDataset->GetRasterXSize();
	int height = mpoDataset->GetRasterYSize();
	int bands = mpoDataset->GetRasterCount();

	// 确定图像格式
	QImage::Format format;
	if (bands == 1) {
		format = QImage::Format_Grayscale8; // 单波段
	}
	else if (bands >= 3) {
		format = QImage::Format_RGB32; // RGB波段
	}
	else {
		return; // 其他情况暂不处理
	}

	// 获取栅格的地理变换参数
	double geoTransform[6];
	mpoDataset->GetGeoTransform(geoTransform);
	// 计算地理范围
	double minGeoX = geoTransform[0];
	double maxGeoX = geoTransform[0] + width * geoTransform[1] + height * geoTransform[2];
	double minGeoY = geoTransform[3] + width * geoTransform[4] + height * geoTransform[5];
	double maxGeoY = geoTransform[3];

	qDebug() << "minGeoX = " << minGeoX << " " << "minGeoY = " << minGeoY << endl;
	qDebug() << "maxGeoX = " << maxGeoX << " " << "maxGeoY = " << maxGeoY << endl;

	int blockSizeX = 4096;	//瓦片长
	int blockSizeY = 4096;	//瓦片宽

	if (bands == 1) {
		GDALRasterBand* band = mpoDataset->GetRasterBand(1); // 读取第一个波段
		//全局归一化
		/*double minValue, maxValue, mean, stddev;
		band->GetStatistics(FALSE, TRUE, &minValue, &maxValue, &mean, &stddev);*/

		for (int y = 0; y < height; y += blockSizeY) {
			for (int x = 0; x < width; x += blockSizeX) {
				int blockWidth = (((blockSizeX) < (width - x)) ? (blockSizeX) : (width - x));
				int blockHeight = (((blockSizeY) < (height - y)) ? (blockSizeY) : (height - y));

				std::vector<unsigned short> buffer(blockWidth * blockHeight);
				band->RasterIO(GF_Read, x, y, blockWidth, blockHeight, buffer.data(), blockWidth, blockHeight, GDT_UInt16, 0, 0);
				//分块归一化
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
		GDALRasterBand* rBand = mpoDataset->GetRasterBand(1); // 红色波段
		GDALRasterBand* gBand = mpoDataset->GetRasterBand(2); // 绿色波段
		GDALRasterBand* bBand = mpoDataset->GetRasterBand(3); // 蓝色波段

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
		
		// 获取文件信息
		QFileInfo fileInfo(sFileName);
		// 获取文件所在文件夹路径
		QString folderPath = fileInfo.absolutePath();
		// 获取图层名
		QString sLayerName = fileInfo.fileName();
		mLayerMap[sLayerName] = bigRaster;
		mDatasetMap[sLayerName] = mpoDataset;
		drawLayer(bigRaster);
	}
	ui.graphicsView->setCurrentScale(1.0);
	ui.graphicsView->setScene(mpScene);
}
	// 保存工程文件
void TigerGIS::on_actionsaveProject_triggered() {
	// 获取保存文件的路径
	QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("保存工程文件"), "./output", QStringLiteral("TigerGIS工程文档 (*.tgs)"));
	// 输出日志
	auto info = "保存工程文件到" + filePath.toStdString();
	mpLog->logInfo(info.c_str());
	// 异常处理
	if (filePath.isEmpty()) {
		mpLog->logWarnInfo("文件路径有误！");
		return;
	}
	if (!filePath.endsWith(".tgs")) {
		filePath += ".tgs";
	}

	QJsonObject project;	//工程文件
	QJsonArray layers;		//图层

	//读取内容列表获取打开的图层
	QStringList layersName = mDatasetMap.keys();
	if (layersName.isEmpty()) {
		mpLog->logWarnInfo("图层为空！无法保存工程文件！");
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
	//添加图层
	project["layers"] = layers;
	project["projection"] = "EPSG : 4326";
	//保存文件
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly)) {
		mpLog->logWarnInfo("无法写入文件！");
		return;
	}

	file.write(QJsonDocument(project).toJson());
	file.close();
	mpLog->logInfo("工程文件保存成功");
}
	// 导入工程文件
void TigerGIS::on_actionreadProject_triggered() {
	// 打开工程文件
	QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("导入工程文件"), "./output", QStringLiteral("TigerGIS工程文档(*.tgs)"));
	// 输出日志
	auto info = "导入工程文件" + filePath.toStdString();
	mpLog->logInfo(info.c_str());
	// 异常处理
	if (filePath.isEmpty()) {	// 处理空工程文件
		mpLog->logWarnInfo("工程文件为空！");
		return;
	}
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {	// 处理工程文件读取失败
		mpLog->logWarnInfo("工程文件读取失败！");
		return;
	}
	// 读取工程文件内容
	QByteArray fileData = file.readAll();
	QJsonDocument document = QJsonDocument::fromJson(fileData);
	QJsonObject project = document.object();
	// 读取图层信息
	QJsonArray layers = project["layers"].toArray();
	for (const auto& layer : layers) {
		QJsonObject layerObj = layer.toObject();
		QString layerPath =  layerObj["path"].toString();
		if (layerObj["type"].toString() == "vector") {	//处理矢量数据
			mpoDataset = static_cast<GDALDataset*>(GDALOpenEx(layerPath.toUtf8().constData(), GDAL_OF_VECTOR, NULL, NULL, NULL));
			addShpContent(layerPath);
			drawShp(layerPath);
		}
		else {	//处理栅格数据
			mpoDataset = (GDALDataset*)GDALOpenEx(layerPath.toUtf8().constData(), GDAL_OF_READONLY, NULL, NULL, NULL);
			addRasterContent(layerPath);
			drawRaster(layerPath);
		}
	}
	mpLog->logInfo("工程文件导入成功！");
}
// 菜单--编辑--编辑矢量要素
	// 开始编辑要素
void TigerGIS::on_actionStartEditing_triggered() {
	QStringList layers = mLayerMap.keys(); // 获取所有图层的键列表
	bool ok; // 用于接收用户输入的状态
	QString layer = QInputDialog::getItem(this, QStringLiteral("选择需要编辑的图层: "), QStringLiteral("图层:"), layers, 0, false, &ok); // 显示输入对话框，获取用户选择的图层
	// 日志输出
	auto info = "正在编辑图层" + layer.toStdString();
	mpLog->logInfo(info.c_str());
	// 判断图层合法性
	if (mLayerMap.contains(layer)) {
		mvEditItems = mLayerMap.value(layer);
	}

	if (ok && !layer.isEmpty()) { // 如果用户确认输入且输入不为空
		QStringList featureTypes = { "Point", "Line", "Polygon" }; // 定义要素类型列表
		QString featureType = QInputDialog::getItem(this, QStringLiteral("选择需要编辑的要素类型:"), QStringLiteral("要素类型:"), featureTypes, 0, false, &ok); // 显示输入对话框，获取用户选择的要素类型
		// 日志输出
		auto info = "编辑图层要素类型" + featureType.toStdString();
		mpLog->logInfo(info.c_str());

		if (ok && !featureType.isEmpty()) { // 如果用户确认输入且输入不为空
			ui.graphicsView->msCurrentLayer = layer; // 设置当前图层
			ui.graphicsView->msCurrentFeatureType = featureType; // 设置当前要素类型
			ui.graphicsView->mbIsEditing = true; // 设置编辑状态为 true
		}
	}
}
	// 撤回
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
			mpLog->logWarnInfo("没有可以撤回的操作了");
			return;
		}
		mpLog->logInfo("撤回成功");
		ui.graphicsView->mvTempItems.pop_back();
	}
	else {
		mpLog->logWarnInfo("没有图层处于编辑状态，无法执行撤回操作");
		return;
	}
}
	// 重做
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
			mpLog->logWarnInfo("没有可以重做的操作了");
			return;
		}
		mpLog->logInfo("重做成功");
	}
	else {
		mpLog->logWarnInfo("没有图层处于编辑状态，无法执行重做操作");
		return;
	}
}
	// 停止编辑
void TigerGIS::on_actionStopEditing_triggered() {
	if (ui.graphicsView->mbIsEditing) { // 如果处于编辑状态
		ui.graphicsView->mvUndoPolygonVector.clear();
		ui.graphicsView->mvUndoLineVector.clear();
		ui.graphicsView->mvUndoPointVector.clear();
		ui.graphicsView->mbIsEditing = false; // 设置编辑状态为 false
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, QStringLiteral("保存编辑内容"), QStringLiteral("是否将编辑内容保存为矢量文件"), // 显示保存更改的确认对话框
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) { // 如果用户选择保存
			saveChanges(); // 调用保存更改的函数
		}
		else { // 如果用户选择不保存
			discardChanges(); // 调用丢弃更改的函数
		}
	}
}
//菜单--分析--矢量分析--统计分析
	// 统计数量
void TigerGIS::on_actionCalculateNum_triggered() {
	QStringList layers = mLayerMap.keys(); // 获取所有图层的键列表
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // 用于接收用户输入的状态
	// 获取统计图层
	QString layer = QInputDialog::getItem(this, QStringLiteral("选择需要统计的图层: "), QStringLiteral("图层:"), vectorLayers, 0, false, &ok); // 显示输入对话框，获取用户选择的图层
	// 日志输出
	auto info = "正在统计图层" + layer.toStdString() + "要素数量";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}

	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("矢量数据集为空");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 1, this, mpoDataset);
	mpAttrWidget->open();
}
	// 统计面积	
void TigerGIS::on_actionCalculateArea_triggered() {
	QStringList layers = mLayerMap.keys(); // 获取所有图层的键列表
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // 用于接收用户输入的状态
	// 获取统计图层
	QString layer = QInputDialog::getItem(this, QStringLiteral("选择需要统计的图层: "), QStringLiteral("图层:"), vectorLayers, 0, false, &ok); // 显示输入对话框，获取用户选择的图层
	// 日志输出
	auto info = "正在统计图层" + layer.toStdString() + "要素面积";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}

	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("矢量数据集为空");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 2, this, mpoDataset);
	mpAttrWidget->open();
}
	// 统计周长
void TigerGIS::on_actionCalculatePerimeter_triggered() {
	QStringList layers = mLayerMap.keys(); // 获取所有图层的键列表
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // 用于接收用户输入的状态
	// 获取统计图层
	QString layer = QInputDialog::getItem(this, QStringLiteral("选择需要统计的图层: "), QStringLiteral("图层:"), vectorLayers, 0, false, &ok); // 显示输入对话框，获取用户选择的图层
	// 日志输出
	auto info = "正在统计图层" + layer.toStdString() + "要素周长";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}

	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("矢量数据集为空");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 3, this, mpoDataset);
	mpAttrWidget->open();
}
	// 统计长度
void TigerGIS::on_actionCalculateLength_triggered() {
	QStringList layers = mLayerMap.keys(); // 获取所有图层的键列表
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	bool ok; // 用于接收用户输入的状态
	// 获取统计图层
	QString layer = QInputDialog::getItem(this, QStringLiteral("选择需要统计的图层: "), QStringLiteral("图层:"), vectorLayers, 0, false, &ok); // 显示输入对话框，获取用户选择的图层
	// 日志输出
	auto info = "正在统计图层" + layer.toStdString() + "要素长度";
	mpLog->logInfo(info.c_str());

	if (ok) {
		mpoDataset = mDatasetMap[layer];
	}
	if (mpoDataset == nullptr) {
		mpLog->logErrorInfo("矢量数据集为空");
		return;
	}
	if (mpAttrWidget != nullptr) {
		mpAttrWidget = nullptr;
	}
	mpAttrWidget = new AttributeWidget(mpLog, 4, this, mpoDataset);
	mpAttrWidget->open();
}
//菜单--分析-矢量分析--空间分析
	// 缓冲区分析
void TigerGIS::on_actionCalculateBuffer_triggered() {
	mpInput = new DataInput(ui.loggerEdit, 2, mDatasetMap,this);
	mpInput->setOpenLabelVisible_1(false);
	mpInput->setOverlayComboVisible(false);
	mpInput->setFileButtonVisible_1(false);
	mpInput->open();
}
	// 凸包计算
void TigerGIS::on_actionCalculateConvex_triggered() {
	mpInput = new DataInput(ui.loggerEdit, 1, mDatasetMap, this);
	mpInput->setOpenLabelVisible_1(false);
	mpInput->setOverlayComboVisible(false);
	mpInput->setFileButtonVisible_1(false);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
	// 叠加分析--交集
void TigerGIS::on_actionIntersect_triggered() {
	mpInput = new DataInput(ui.loggerEdit, mDatasetMap, this, 1);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
	// 叠加分析--并集
void TigerGIS::on_actionUnion_triggered() {
	mpInput = new DataInput(ui.loggerEdit, mDatasetMap, this, 2);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
	// 叠加分析--擦除
void TigerGIS::on_actionErase_triggered() {
	mpInput = new DataInput(ui.loggerEdit, mDatasetMap, this, 3);
	mpInput->setRadiusLabelVisible(false);
	mpInput->setBufferEditVisible(false);
	mpInput->open();
}
//菜单--分析--栅格分析
	// 绘制灰度直方图
void TigerGIS::on_actiondrawGreyHistogram_triggered(){
	QStringList layers = mLayerMap.keys(); // 获取所有图层的键列表
	QStringList rasterLayers;
	for (const auto& layer : layers) {
		if (layer.endsWith(".tif") || layer.endsWith(".TIF")) {
			rasterLayers << layer;
		}
	}
	bool ok; // 用于接收用户输入的状态
	// 获取栅格图层
	QString layer = QInputDialog::getItem(this, QStringLiteral("选择栅格图层: "), QStringLiteral("图层:"), rasterLayers, 0, false, &ok); // 显示输入对话框，获取用户选择的图层
	// 日志输出
	auto info = "正在绘制图层" + layer.toStdString() + "的灰度直方图";
	mpLog->logInfo(info.c_str());

	if (!ok) return;
	// 获取栅格数据集
	mpoDataset = mDatasetMap[layer];
	if (!mpoDataset) {
		QMessageBox::warning(this, "Error", "Empty dataset!");
		mpLog->logErrorInfo("分析栅格为空");
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
	 //掩膜提取
void TigerGIS::on_actionextractByMask_triggered() {
	mpMask = new MaskExtract(ui.loggerEdit, mDatasetMap, this);
	mpMask->open();
}
	// 领域分析
void TigerGIS::on_actiondomainAnalysis_triggered() {
	mpNeighbor = new NeighborhoodStatistics(ui.loggerEdit, mDatasetMap, this);
	mpNeighbor->open();
}
//菜单--帮助
void TigerGIS::on_actionHelp_triggered() {
	mpLog->logInfo("联系我们");
	QMessageBox::about(this, QStringLiteral("帮助"), QStringLiteral("本软件为面向对象程序设计课设成果\n中国地质大学武汉\n地理与信息工程学院\n邮箱：2942486336@qq.com"));
}

// 绘制矢量
int TigerGIS::drawShp(QString sFileName) {
	if (!mpoDataset) {
		return -1;	// 数据集或视图未初始化或打开失败，返回错误
	}
	// 定义目标坐标参考系，这里假设使用WGS84
	OGRSpatialReference oTargetSRS;
	oTargetSRS.SetWellKnownGeogCS("WGS84");
	//获取图层数
	int nLayerCount = mpoDataset->GetLayerCount();
	// 放大系数
	double scaleFactor = 1000.0;

	//遍历每个图层获取要素类
	for (int i = 0; i < nLayerCount; i++) {
		//创建图层容器
		QVector<QGraphicsItem*> layerVector;
		//获取图层
		OGRLayer* poLayer = mpoDataset->GetLayer(i);

		// 获取源投影
		OGRSpatialReference* sourceSRS = poLayer->GetSpatialRef();

		// 创建目标投影（WGS84）
		OGRSpatialReference targetSRS;
		targetSRS.SetWellKnownGeogCS("WGS84");  // 使用 SetWellKnownGeogCS 方法设置为 WGS84

		// 创建坐标转换对象
		OGRCoordinateTransformation* coordTransform = OGRCreateCoordinateTransformation(sourceSRS, &targetSRS);
		if (coordTransform == nullptr) {
			std::cerr << "Could not create coordinate transformation" << std::endl;
			GDALClose(mpoDataset);
			return 1;
		}
		//读取图层中的要素并添加到场景中
		poLayer->ResetReading();
		OGRFeature* feature;

		while ((feature = poLayer->GetNextFeature()) != nullptr) {

			OGRGeometry* geometry = feature->GetGeometryRef();
			if (!geometry) continue;
			if (geometry != nullptr) {
				geometry->transform(coordTransform);  // 转换坐标
			}
			// 这里需要根据几何类型创建对应的 QGraphicsItem
			QGraphicsItem* item = nullptr;
			//ui.graphicsView->mPen.setWidthF(0.4); // 初始线条宽度

			switch (geometry->getGeometryType()) {
			case wkbPoint: {	//点
				OGRPoint* point = geometry->toPoint();
				double x = -point->getX() * scaleFactor;
				double y = point->getY() * scaleFactor; // 需要反转Y坐标
				auto* ellipseItem = new QGraphicsEllipseItem(y - 2*scaleFactor, x - 2*scaleFactor, 4*scaleFactor, 4*scaleFactor);
				item = ellipseItem;
				break;
			}
			case wkbLineString: {	//线
				OGRLineString* line = geometry->toLineString();
				QPainterPath path;
				if (line->getNumPoints() > 0) {
					path.moveTo(line->getY(0) * scaleFactor, -line->getX(0)* scaleFactor);
					for (int j = 1; j < line->getNumPoints(); ++j) {
						double x = -line->getX(j) * scaleFactor;
						double y = line->getY(j) * scaleFactor; // 需要反转Y坐标
						path.lineTo(y, x);
					}
				}
				auto* pathItem = new QGraphicsPathItem(path);
				item = pathItem;

				break;
			}
			case wkbPolygon: {	//面
				OGRPolygon* polygon = geometry->toPolygon();
				OGRLinearRing* ring = polygon->getExteriorRing();
				QPolygonF qPolygon;
				for (int j = 0; j < ring->getNumPoints(); ++j) {
					double x = -ring->getX(j) * scaleFactor;
					double y = ring->getY(j) * scaleFactor; // 需要反转Y坐标
					qPolygon << QPointF(y, x);
				}
				auto* polygonItem = new QGraphicsPolygonItem(qPolygon);
				item = polygonItem;

				break;
			}
			default:
				continue; // 其他几何类型暂时不处理
			}

			if (item) {
				// 添加图形项到图层中
				layerVector.push_back(item);
				//scene->addItem(item);
			}
			delete feature; // 释放要素对象
		}
		// 获取文件信息
		QFileInfo fileInfo(sFileName);
		// 获取文件所在文件夹路径
		QString folderPath = fileInfo.absolutePath();
		// 获取图层名
		const char* sLayerName = poLayer->GetName();
		mDatasetMap[sLayerName] = mpoDataset;
		mLayerMap[sLayerName] = layerVector;
		// 绘制图层
		auto info = "正在绘制图层" + std::string(sLayerName);
		mpLog->logInfo(info.c_str());
		drawLayer(layerVector);
	}
	return 0;
}
// 绘制栅格
int TigerGIS::drawRaster(QString sFileName, int nRedBand, int nGreenBand, int nBlueBand) {

	if (!mpoDataset) {
		return -1; // 数据集或视图未初始化，返回错误
	}

	if (mpoDataset->GetRasterCount() == 0) {
		return -1; // 数据集中没有栅格数据
	}

	//// 获取源投影
	//const char* projRef = mpoDataset->GetProjectionRef();
	//OGRSpatialReference sourceSRS;
	//sourceSRS.importFromWkt(projRef);

	//// 创建目标投影（WGS84）
	//OGRSpatialReference targetSRS;
	//targetSRS.SetWellKnownGeogCS("WGS84");

	//// 创建坐标转换对象
	//OGRCoordinateTransformation* coordTransform = OGRCreateCoordinateTransformation(&sourceSRS, &targetSRS);
	//if (coordTransform == nullptr) {
	//	std::cerr << "Could not create coordinate transformation" << std::endl;
	//	GDALClose(mpoDataset);
	//	return 1;
	//}

	//// 创建目标数据集
	//GDALDataset* pWarpedDataset = static_cast<GDALDataset*>(GDALAutoCreateWarpedVRT(mpoDataset, projRef, targetSRS.exportToWkt().c_str(), GRA_NearestNeighbour, 0.0, nullptr));
	//if (pWarpedDataset == nullptr) {
	//	std::cerr << "Could not create warped dataset" << std::endl;
	//	OGRCoordinateTransformation::DestroyCT(coordTransform);
	//	GDALClose(mpoDataset);
	//	return 1;
	//}

	// 获取栅格尺寸与波段数
	int width = mpoDataset->GetRasterXSize();
	int height = mpoDataset->GetRasterYSize();
	int bands = mpoDataset->GetRasterCount();

	// 确定图像格式
	QImage::Format format;
	if (bands == 1) {
		format = QImage::Format_Grayscale8; // 单波段
	}
	else if (bands >= 3) {
		format = QImage::Format_RGB32; // RGB波段
	}
	else {
		return -1; // 其他情况暂不处理
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

	// 获取栅格的地理变换参数
	double geoTransform[6];
	mpoDataset->GetGeoTransform(geoTransform);
	// 计算地理范围
	double minGeoX = geoTransform[0];
	double maxGeoX = geoTransform[0] + width * geoTransform[1] + height * geoTransform[2];
	double minGeoY = geoTransform[3] + width * geoTransform[4] + height * geoTransform[5];
	double maxGeoY = geoTransform[3];

	// 创建一个空的 QImage
	QImage image(width, height, format);

	// 读取波段数据
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
			return -1; // 不支持的数据类型
		}
	}

	// 寻找所有波段的最大和最小值，以便进行归一化
	double minValue = *std::min_element(bandData[0].begin(), bandData[0].end());
	double maxValue = *std::max_element(bandData[0].begin(), bandData[0].end());
	for (int i = 1; i < bands; ++i) {
		minValue = (((minValue) < (*std::min_element(bandData[i].begin(), bandData[i].end()))) ? (minValue) : (*std::min_element(bandData[i].begin(), bandData[i].end())));
		maxValue = (((maxValue) > (*std::max_element(bandData[i].begin(), bandData[i].end()))) ? (maxValue) : (*std::max_element(bandData[i].begin(), bandData[i].end())));
	}

	// 将波段数据归一化并拷贝到 QImage
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
	// 获取文件信息
	QFileInfo fileInfo(sFileName);
	// 获取文件所在文件夹路径
	QString folderPath = fileInfo.absolutePath();
	// 获取图层名
	QString sLayerName = fileInfo.fileName();
	mDatasetMap[sLayerName] = mpoDataset;
	mLayerMap[sLayerName] = rasterLayer;
	// 绘制图层
	auto info = "正在绘制图层" + sLayerName.toStdString();
	mpLog->logInfo(info.c_str());
	drawLayer(rasterLayer);
	
	return 0; // 成功
}
// 绘制wkt
int TigerGIS::drawGeometry(OGRGeometry* poGeometry) {
	if (!poGeometry) {
		return -1;  // 视图或几何对象未初始化或无效，返回错误
	}

	// 放大系数
	double scaleFactor = 1000.0;

	QGraphicsItem* item = nullptr;

	switch (poGeometry->getGeometryType()) {
	case wkbPoint: {
		OGRPoint* point = poGeometry->toPoint();
		double x = point->getX() * scaleFactor;
		double y = -point->getY() * scaleFactor; // 需要反转Y坐标
		auto ellipseItem = new QGraphicsEllipseItem(x - 2, y - 2, 4, 4);
		item = ellipseItem;
		break;
	}
	case wkbLineString: {
		OGRLineString* line = poGeometry->toLineString();
		QPolygonF polygon;
		for (int j = 0; j < line->getNumPoints(); ++j) {
			double x = line->getX(j) * scaleFactor;
			double y = -line->getY(j) * scaleFactor; // 需要反转Y坐标
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
			double y = -ring->getY(j) * scaleFactor; // 需要反转Y坐标
			qPolygon << QPointF(x, y);
		}
		auto qpolygonItem = new QGraphicsPolygonItem(qPolygon);
		item = qpolygonItem;
		break;
	}
	default:
		return -1; // 其他几何类型暂时不处理，返回错误
	}

	if (item) {
		// 添加图形项到场景中
		mpScene->addItem(item);
	}

	// 设置场景的边界，以确保所有要素都能正确显示
	QRectF sceneRect = mpScene->itemsBoundingRect();
	mpScene->setSceneRect(sceneRect);

	// 调整视图范围以铺满窗口
	ui.graphicsView->fitInView(sceneRect, Qt::KeepAspectRatio); // 将视图调整到合适的范围

	// 将场景添加到视图中显示
	ui.graphicsView->setScene(mpScene);
	ui.graphicsView->show();

	return 0;
}

//添加栅格图层至内容列表
int TigerGIS::addRasterContent(QString sFileName) {
	// 获取文件信息
	QFileInfo fileInfo(sFileName);
	// 获取文件所在文件夹路径
	QString folderPath = fileInfo.absolutePath();
	//获取文件名
	QString fileName = fileInfo.fileName();

	//判断是否已经存在该文件夹路径
	for (int i = 0; i < mpRootNode->rowCount(); ++i) {
		QStandardItem* childItem = mpRootNode->child(i);
		if (fileInfo.absolutePath() == childItem->text()) {
			//将文件名添加至文件目录
			QStandardItem* newSonOfItem = new QStandardItem(fileName);
			//设置节点checkBox
			newSonOfItem->setCheckable(true);
			newSonOfItem->setCheckState(Qt::Checked);
			childItem->appendRow(newSonOfItem);

			auto info = "将图层" + fileName.toStdString() + "添加至内容列表";
			mpLog->logInfo(info.c_str());
			return 0;
		}

	}
	//添加文件目录至内容列表
	QStandardItem* newItem = new QStandardItem(folderPath);
	//将文件名添加至文件目录
	QStandardItem* newSonOfItem = new QStandardItem(fileName);
	//设置节点checkBox
	newSonOfItem->setCheckable(true);
	newSonOfItem->setCheckState(Qt::Checked);

	newItem->appendRow(newSonOfItem);
	mpRootNode->appendRow(newItem);

	auto info = "将图层" + fileName.toStdString() + "添加至内容列表";
	mpLog->logInfo(info.c_str());

	return 0;
}
//添加矢量图层至内容列表
int TigerGIS::addShpContent(QString sFileName) {
	// 获取文件信息
	QFileInfo fileInfo(sFileName);
	// 获取文件所在文件夹路径
	QString folderPath = fileInfo.absolutePath();
	// 获取图层名
	QString sLayerName = fileInfo.baseName();
	//判断是否已经存在该文件夹路径
	for (int i = 0; i < mpRootNode->rowCount(); ++i) {
		QStandardItem* childItem = mpRootNode->child(i);
		if (fileInfo.absolutePath() == childItem->text()) {
			//将图层添加至文件目录
			QStandardItem* newSonOfItem = new QStandardItem(sLayerName);
			//设置节点带checkBox
			newSonOfItem->setCheckable(true);
			newSonOfItem->setCheckState(Qt::Checked);

			childItem->appendRow(newSonOfItem);
			auto info = "将图层" + sLayerName.toStdString() + "添加至内容列表";
			mpLog->logInfo(info.c_str());
			return 0;
		}

	}
	//添加文件目录至内容列表
	QStandardItem* newItem = new QStandardItem(folderPath);
	//将文件名添加至文件目录
	QStandardItem* newSonOfItem = new QStandardItem(sLayerName);
	//设置节点带checkBox
	newSonOfItem->setCheckable(true);
	newSonOfItem->setCheckState(Qt::Checked);

	newItem->appendRow(newSonOfItem);
	mpRootNode->appendRow(newItem);

	auto info = "将图层" + sLayerName.toStdString() + "添加至内容列表";
	mpLog->logInfo(info.c_str());

	return 0;
}
//绘制图层
int TigerGIS::drawLayer(QVector<QGraphicsItem*> layer) {
	resetView();
	for (auto& item : layer) {
		if (auto ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(item)) {
			// 修改轮廓色
			ellipseItem->setPen(ui.graphicsView->mPen);

			// 修改填充色
			ellipseItem->setBrush(ui.graphicsView->mBrush);
		}
		else if (auto polygonItem = dynamic_cast<QGraphicsPolygonItem*>(item)) {
			// 修改轮廓色
			polygonItem->setPen(ui.graphicsView->mPen);

			// 修改填充色
			polygonItem->setBrush(ui.graphicsView->mBrush);
		}
		else if (auto pathItem = dynamic_cast<QGraphicsPathItem*>(item)) {
			ui.graphicsView->mPen.setWidth(0.4);
			pathItem->setPen(ui.graphicsView->mPen);
		}
		mpScene->addItem(item);
	}
	// 日志记录
	mpLog->logInfo("图层绘制完成");
	// 设置场景的边界，以确保所有要素都能正确显示

	// 将场景添加到视图中显示
	ui.graphicsView->setScene(mpScene);

	// 调整视图范围以铺满窗口
	ui.graphicsView->fitInView(ui.graphicsView->scene()->itemsBoundingRect(), Qt::KeepAspectRatio); // 将视图调整到合适的范围

	ui.graphicsView->show();
	return 0;
}
//移除图层
int TigerGIS::removeLayer(QVector<QGraphicsItem*> layer) {
	for (const auto& item : layer) {
		mpScene->removeItem(item);
	}
	// 设置场景的边界，以确保所有要素都能正确显示
	QRectF sceneRect = mpScene->itemsBoundingRect();
	mpScene->setSceneRect(sceneRect);

	// 调整视图范围以铺满窗口
	ui.graphicsView->fitInView(sceneRect, Qt::KeepAspectRatio); // 将视图调整到合适的范围

	// 将场景添加到视图中显示
	ui.graphicsView->setScene(mpScene);
	ui.graphicsView->show();
	return 0;
}

//复位函数
void TigerGIS::resetView() {
	ui.graphicsView->setCurrentScale(1.0);		// 重置当前缩放
	ui.graphicsView->resetTransform();			// 重置视图的变换矩阵
	if (ui.graphicsView->scene() == nullptr) {
		return;
	}
	ui.graphicsView->fitInView(ui.graphicsView->scene()->itemsBoundingRect(), Qt::KeepAspectRatio); // 调整视图以适应场景
	mpLog->logInfo("复位");
}

//内容目录中的获取勾选节点
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

//设置所有图层的填充色轮廓色
void TigerGIS::selectAllColor() {
	// 日志输出
	mpLog->logInfo("设置所有图层符号属性");
	// 获取颜色
	QColor fillColor = QColorDialog::getColor(Qt::green, this, tr(QString::fromLocal8Bit("设置填充色").toUtf8().constData()));
	QColor outlineColor = QColorDialog::getColor(Qt::black, this, tr(QString::fromLocal8Bit("设置轮廓色").toUtf8().constData()));
	if (fillColor.isValid() && outlineColor.isValid()) {
		ui.graphicsView->mBrush.setColor(fillColor);
		ui.graphicsView->mPen.setColor(outlineColor);
		auto ret = getCheckedItem();
		if (!ret.isEmpty()) {
			for (const auto& item : ret) {
				removeLayer(mLayerMap[item]);
				drawLayer(mLayerMap[item]); // 重新绘制
			}
		}
	}
}
//设置选定图层的填充色轮廓色
void TigerGIS::selectColor(QString sLayerName) {
	// 日志输出
	auto info = "设置图层" + sLayerName.toStdString() + "符号属性";
	mpLog->logInfo(info.c_str());
	// 获取颜色
	QColor fillColor = QColorDialog::getColor(Qt::green, this, tr(QString::fromLocal8Bit("设置填充色").toUtf8().constData()));
	QColor outlineColor = QColorDialog::getColor(Qt::black, this, tr(QString::fromLocal8Bit("设置轮廓色").toUtf8().constData()));
	if (fillColor.isValid() && outlineColor.isValid()) {
		ui.graphicsView->mBrush.setColor(fillColor);
		ui.graphicsView->mPen.setColor(outlineColor);
		auto selectLayer = mLayerMap[sLayerName];
		if (!selectLayer.isEmpty()) {
			removeLayer(selectLayer);
			drawLayer(selectLayer); // 重新绘制
		}
	}
}

//保存编辑为文件
void TigerGIS::saveChanges() {
	QString filePath = QFileDialog::getSaveFileName(nullptr, QStringLiteral("保存为矢量文件"), "./output", "Shapefile (*.shp)");
	// 日志输出
	auto info = "正在保存编辑文件至" + filePath.toStdString();
	mpLog->logInfo(info.c_str());
	// 异常处理
	if (filePath.isEmpty()) {
		// 用户取消了文件选择
		mpLog->logInfo("取消保存");
		return;
	}

	// 获取图层
	OGRLayer* poLayer = mpoDataset->GetLayer(0);
	poLayer->ResetReading();
	if (poLayer == nullptr) {
		mpLog->logErrorInfo("图层创建失败");
		GDALClose(mpoDataset);
		return;
	}

	// 获取图层的空间参考
	OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
	if (poSRS == nullptr) {
		mpLog->logWarnInfo("空间参考获取失败");
		GDALClose(mpoDataset);
		return;
	}

	// 创建目标投影（WGS84）
	OGRSpatialReference targetSRS;
	targetSRS.SetWellKnownGeogCS("WGS84");  // 使用 SetWellKnownGeogCS 方法设置为 WGS84

	// 创建输出数据源
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == nullptr) {
		auto info = std::string(pszDriverName) + "驱动不可用.";
		mpLog->logErrorInfo(info.c_str());
		GDALClose(mpoDataset);
		return;
	}

	auto temp1 = filePath.toUtf8();
	auto temp2 = temp1.constData();
	const char* pszOutputFilePath = temp2;
	GDALDataset* poDS_out = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, nullptr);
	if (poDS_out == nullptr) {
		mpLog->logErrorInfo("输出数据源创建失败.");
		GDALClose(mpoDataset);
		return;
	}

	// 获取当前图层和图形项列表
	auto tempItems = ui.graphicsView->mvTempItems;
	QGraphicsScene* scene = ui.graphicsView->scene();
	QString currentFeatureType = ui.graphicsView->msCurrentFeatureType;

	// 获取场景的边界矩形，用于计算翻转的y坐标
	QRectF sceneRect = scene->sceneRect();

	// 创建不同类型的图层
	if (currentFeatureType == "Point") {
		OGRLayer* pointLayer = poDS_out->CreateLayer("point_layer", &targetSRS, wkbPoint, nullptr);
		if (pointLayer == nullptr) {
			mpLog->logErrorInfo("点图层创建失败");
			GDALClose(mpoDataset);
			GDALClose(poDS_out);
			return;
		}

		// 获取当前场景的矩形
		QRectF sceneRect = scene->sceneRect();

		// 遍历图形项列表并添加到图层中
		for (QGraphicsItem* item : tempItems) {
			// 处理点图形项
			QGraphicsEllipseItem* ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(item);
			if (currentFeatureType == "Point") {
				//qDebug() << "Processing Point";
				QRectF rect = ellipseItem->rect();
				QPointF center = rect.center();
				OGRPoint ogrPoint;

				// 设置点的坐标并翻转y轴
				double adjustedY = sceneRect.height() - center.y() - sceneRect.height();
				ogrPoint.setY(center.x() / 1000);
				ogrPoint.setX(adjustedY / 1000);

				// 创建要素并添加到图层
				OGRFeature* feature = OGRFeature::CreateFeature(pointLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPoint);
				if (pointLayer->CreateFeature(feature) != OGRERR_NONE) {
					// 处理要素创建失败的情况
					mpLog->logErrorInfo("无法创建点要素");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
		for (QGraphicsItem* item : mvEditItems) {
			// 处理点图形项
			QGraphicsEllipseItem* ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(item);
			if (currentFeatureType == "Point") {
				// qDebug() << "Processing Point";
				QRectF rect = ellipseItem->rect();
				QPointF center = rect.center();
				OGRPoint ogrPoint;

				// 设置点的坐标并翻转y轴
				double adjustedY = sceneRect.height() - center.y() - sceneRect.height();
				ogrPoint.setY(center.x() / 1000);
				ogrPoint.setX(adjustedY / 1000);

				// 创建要素并添加到图层
				OGRFeature* feature = OGRFeature::CreateFeature(pointLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPoint);
				if (pointLayer->CreateFeature(feature) != OGRERR_NONE) {
					// 处理要素创建失败的情况
					mpLog->logErrorInfo("无法创建点要素");
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
			mpLog->logErrorInfo("线图层创建失败");
			GDALClose(mpoDataset);
			GDALClose(poDS_out);
			return;
		}

		// 获取当前场景的矩形
		QRectF sceneRect = scene->sceneRect();

		// 遍历图形项列表并添加到图层中
		for (QGraphicsItem* item : tempItems) {
			// 处理线图形项
			QGraphicsLineItem* lineItem = dynamic_cast<QGraphicsLineItem*>(item);
			if (currentFeatureType == "Line") {
				// qDebug() << "Processing Line";
				QLineF line = lineItem->line();
				OGRLineString ogrLine;

				// 添加线的起点和终点，并翻转y轴
				double adjustedY1 = sceneRect.height() - line.y1() - sceneRect.height();
				double adjustedY2 = sceneRect.height() - line.y2() - sceneRect.height();
				ogrLine.addPoint(line.x1() / 1000, adjustedY1 / 1000);
				ogrLine.addPoint(line.x2() / 1000, adjustedY2 / 1000);

				// 创建要素并添加到图层
				OGRFeature* feature = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
				feature->SetGeometry(&ogrLine);
				if (lineLayer->CreateFeature(feature) != OGRERR_NONE) {
					// 处理要素创建失败的情况
					mpLog->logErrorInfo("无法创建线要素");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
		for (QGraphicsItem* item : mvEditItems) {
			// 处理线图形项
			QGraphicsPathItem* pathItem = dynamic_cast<QGraphicsPathItem*>(item);
			if (currentFeatureType == "Line") {
				// qDebug() << "Processing Line";
				auto path = pathItem->path();
				OGRLineString ogrLine;

				// 添加路径中的所有点，并翻转y轴
				for (int i = 0; i < path.elementCount(); ++i) {
					auto point = path.elementAt(i);
					double adjustedY = sceneRect.height() - point.y - sceneRect.height();
					ogrLine.addPoint(point.x / 1000, adjustedY / 1000);
				}

				// 创建要素并添加到图层
				OGRFeature* feature = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
				feature->SetGeometry(&ogrLine);
				if (lineLayer->CreateFeature(feature) != OGRERR_NONE) {
					// 处理要素创建失败的情况
					mpLog->logErrorInfo("无法创建线要素");
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
			mpLog->logErrorInfo("多边形图层创建失败.");
			GDALClose(mpoDataset);
			GDALClose(poDS_out);
			return;
		}

		// 获取当前场景的矩形
		QRectF sceneRect = scene->sceneRect();

		// 遍历图形项列表并添加到图层中
		for (QGraphicsItem* item : tempItems) {
			// 处理多边形图形项
			QGraphicsPolygonItem* polygonItem = dynamic_cast<QGraphicsPolygonItem*>(item);
			if (currentFeatureType == "Polygon") {
				// qDebug() << "Processing Polygon";
				QPolygonF polygon = polygonItem->polygon().toPolygon();
				OGRPolygon ogrPolygon;
				OGRLinearRing ogrRing;

				// 添加多边形的点到线性环，并翻转y轴
				for (int i = 0; i < polygon.size(); ++i) {
					QPointF point = polygon.at(i);
					// 翻转y轴后再减去场景高度
					double adjustedY = sceneRect.height() - point.y() - sceneRect.height();
					ogrRing.addPoint(point.x() / 1000, adjustedY / 1000);
				}

				// 将线性环添加到多边形
				ogrPolygon.addRing(&ogrRing);

				// 创建要素并添加到图层
				OGRFeature* feature = OGRFeature::CreateFeature(polygonLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPolygon);
				if (polygonLayer->CreateFeature(feature) != OGRERR_NONE) {
					// 处理要素创建失败的情况
					mpLog->logErrorInfo("无法创建面要素");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
		for (QGraphicsItem* item : mvEditItems) {
			// 处理多边形图形项
			QGraphicsPolygonItem* polygonItem = dynamic_cast<QGraphicsPolygonItem*>(item);
			if (currentFeatureType == "Polygon") {
				// qDebug() << "Processing Polygon";
				QPolygonF polygon = polygonItem->polygon().toPolygon();
				OGRPolygon ogrPolygon;
				OGRLinearRing ogrRing;

				// 添加多边形的点到线性环，并翻转y轴
				for (int i = 0; i < polygon.size(); ++i) {
					QPointF point = polygon.at(i);
					// 翻转y轴后再减去场景高度
					double adjustedY = sceneRect.height() - point.y() - sceneRect.height();
					ogrRing.addPoint(point.x() / 1000, adjustedY / 1000);
				}

				// 将线性环添加到多边形
				ogrPolygon.addRing(&ogrRing);

				// 创建要素并添加到图层
				OGRFeature* feature = OGRFeature::CreateFeature(polygonLayer->GetLayerDefn());
				feature->SetGeometry(&ogrPolygon);
				if (polygonLayer->CreateFeature(feature) != OGRERR_NONE) {
					// 处理要素创建失败的情况
					mpLog->logErrorInfo("无法创建面要素");
					OGRFeature::DestroyFeature(feature);
					continue;
				}
				OGRFeature::DestroyFeature(feature);
			}
		}
	}

	// 释放资源
	GDALClose(poDS_out);

	for (QGraphicsItem* item : qAsConst(ui.graphicsView->mvTempItems)) { // 遍历临时图形项列表
		mpScene->removeItem(item); // 从场景中移除图形项
		delete item; // 删除图形项
	}
	ui.graphicsView->mvTempItems.clear(); // 清空临时图形项列表
	mpLog->logInfo("编辑文件保存成功");
}
//取消保存
void TigerGIS::discardChanges() {
	for (QGraphicsItem* item : qAsConst(ui.graphicsView->mvTempItems)) { // 遍历临时图形项列表
		mpScene->removeItem(item); // 从场景中移除图形项
		delete item; // 删除图形项
	}
	ui.graphicsView->mvTempItems.clear(); // 清空临时图形项列表
	mpLog->logInfo("取消保存编辑文件");
}