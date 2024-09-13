#include "DataInput.h"
#include <QFileDialog>
#include <QDebug>
#include <QPushButton>


//缓冲区分析&凸包计算
DataInput::DataInput(QPlainTextEdit* pLogger, int nSelect, QMap<QString, GDALDataset*> datasetMap, QWidget* parent)
	: mnSelect(nSelect)
	, mDatasetMap(datasetMap)
	, QDialog(parent)
	, mpLog(nullptr)
	, mpDataset(nullptr)
{
	ui.setupUi(this);
	mpLog = new logger("矢量分析", pLogger);
	ui.fileBtn->setStyleSheet("border:none;");	//去掉按钮轮廓
	//更新输入图层列表
	QStringList layers = datasetMap.keys();
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	ui.inputCombo->addItems(vectorLayers);

	//选择输入图层
	connect(ui.fileButton2, &QPushButton::clicked, [&]() {
		auto sFilePath2 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp)");
		QStringList temp = sFilePath2.split("/");					//通过路径分隔符分隔字符串
		QString basename = temp.at(temp.size() - 1).split(".")[0];	//获取文件名
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
		});

	//选择输出文件夹
	connect(ui.fileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("输出文件").toUtf8().constData()),
			"./output", "Shapefile(*.shp)");
		ui.fileEdit->setText(sFilePath);
		});

	//确定按钮执行缓冲区分析
	connect(ui.sureBtn, &QPushButton::clicked, [&]() {
		QString sLayerName = ui.inputCombo->currentText();
		mpDataset = mDatasetMap[sLayerName];
		double dRadius = ui.radiusEdit->text().toUtf8().toDouble();
		QString filePath = ui.fileEdit->text();
		QByteArray byteArray = filePath.toUtf8();
		const char* sFilePath = byteArray.constData();
		if (mpDataset != nullptr) {
			if (mnSelect == 1) {
				auto info = "正在对图层" + sLayerName.toStdString() + "执行凸包计算...";
				mpLog->logInfo(info.c_str());
				if (Convex(mpDataset, sFilePath) == 0) {
					mpLog->logInfo("凸包计算成功");
				}
				else {
					mpLog->logWarnInfo("凸包计算失败");
				}
			}
			else if (mnSelect == 2) {
				auto info = "正在对图层" + sLayerName.toStdString() + "执行缓冲区分析...";
				mpLog->logInfo(info.c_str());
				if (Buffer(dRadius, mpDataset, sFilePath) == 0) {
					mpLog->logInfo("缓冲区分析成功");
				}
				else {
					mpLog->logWarnInfo("缓冲区分析失败");
				}
			}
		}
		this->close();
		});

	//取消按钮,清空文本框
	connect(ui.cancelBtn, &QPushButton::clicked, [&]() {
		ui.radiusEdit->clear();
		ui.fileEdit->clear();
		mpLog->logInfo("取消");
		this->close();
		});
}
//叠加分析
DataInput::DataInput(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent, int nSelect)
	: QDialog(parent)
	, mnSelect(nSelect)
	, mDatasetMap(datasetMap)
	, mpDataset(nullptr)
	, mpLog(nullptr)
{
	ui.setupUi(this);
	mpLog = new logger("矢量分析", pLogger);
	ui.fileBtn->setStyleSheet("border:none;");
	//更新叠加要素图层列表与输入图层列表
	QStringList layers = datasetMap.keys();
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	ui.overlayCambo->addItems(vectorLayers);
	ui.inputCombo->addItems(vectorLayers);
	
	//选择输出文件
	connect(ui.fileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("输出文件").toUtf8().constData()),
			"./output", "Shapefile (*.shp)");
		ui.fileEdit->setText(sFilePath);
		});
	//选择叠加图层
	connect(ui.fileButton1, &QPushButton::clicked, [&]() {
		auto sFilePath1 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp)");
		QStringList temp = sFilePath1.split("/");					//通过路径分隔符分隔字符串
		QString basename = temp.at(temp.size() - 1).split(".")[0];	//获取文件名
		ui.overlayCambo->addItem(basename);
		ui.overlayCambo->setCurrentText(basename);
		});
	//选择输入图层
	connect(ui.fileButton2, &QPushButton::clicked, [&]() {
		auto sFilePath2 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp)");
		QStringList temp = sFilePath2.split("/");					//通过路径分隔符分隔字符串
		QString basename = temp.at(temp.size() - 1).split(".")[0];	//获取文件名
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
		});
	//执行叠加分析
	connect(ui.sureBtn, &QPushButton::clicked, [&]() {
		QString filePath1 = ui.fileEdit->text();
		QByteArray byteArray1 = filePath1.toUtf8();
		const char* sFilePath1 = byteArray1.constData();

		QString filePath2 = mDatasetMap[ui.overlayCambo->currentText()]->GetDescription();
		QByteArray byteArray2 = filePath2.toUtf8();
		const char* sFilePath2 = byteArray2.constData();

		QString filePath3 = mDatasetMap[ui.inputCombo->currentText()]->GetDescription();
		QByteArray byteArray3 = filePath3.toUtf8();
		const char* sFilePath3 = byteArray3.constData();

		//交集
		if (mnSelect == 1) {
			mpLog->logInfo("正在执行交集运算...");
			if (IntersectLayers(sFilePath2, sFilePath3, sFilePath1) == 0) {
				mpLog->logInfo("交集运算成功");
			}
			else {
				mpLog->logInfo("交集运算失败");
			}
		}
		//并集
		else if (mnSelect == 2) {
			mpLog->logInfo("正在执行并集运算...");
			if (UnionLayers(sFilePath2, sFilePath3, sFilePath1) == 0) {
				mpLog->logInfo("并集运算成功");
			}
			else {
				mpLog->logInfo("并集运算失败");
			}
		}
		//擦除
		else if (mnSelect == 3) {
			mpLog->logInfo("正在执行擦除运算...");
			if (EraseLayers(sFilePath2, sFilePath3, sFilePath1) == 0) {
				mpLog->logInfo("擦除运算成功");
			}
			else {
				mpLog->logInfo("并集运算失败");
			}
		}

		this->close();
		});
	//取消
	connect(ui.cancelBtn, &QPushButton::clicked, [&]() {
		ui.overlayCambo->clear();
		ui.inputCombo->clear();
		ui.fileEdit->clear();
		mpLog->logInfo("取消");
		this->close();
		});
}
DataInput::~DataInput() {}

//设置控件可见度
void DataInput::setRadiusLabelVisible(bool visible) {
	ui.label->setVisible(visible);
}
void DataInput::setBufferEditVisible(bool visible) {
	ui.radiusEdit->setVisible(visible);
}
void DataInput::setOpenLabelVisible_1(bool visible) {
	ui.openLabel1->setVisible(visible);
}
void DataInput::setOpenLabelVisible_2(bool visible) {
	ui.openLabel2->setVisible(visible);
}
void DataInput::setOverlayComboVisible(bool visible) {
	ui.overlayCambo->setVisible(visible);
}
void DataInput::setinputComboVisible(bool visible) {
	ui.inputCombo->setVisible(visible);
}
void DataInput::setFileButtonVisible_1(bool visible) {
	ui.fileButton1->setVisible(visible);
}
void DataInput::setFileButtonVisible_2(bool visible) {
	ui.fileButton2->setVisible(visible);
}


//缓冲区分析
int DataInput::Buffer(double dRadius, GDALDataset* poDataset, const char* sFilePath) {
	int nCount = poDataset->GetLayerCount();
	qDebug() << nCount << endl;
	//获取图层
	OGRLayer* poLayer = poDataset->GetLayer(0);
	poLayer->ResetReading();
	if (poLayer == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("图层获取失败.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//获取图层的空间参考
	OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
	if (poSRS == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("无法获取图层的空间参考.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//获取坐标系单位
	const char* pszUnitName = poSRS->GetAttrValue("UNIT", 0);
	if (pszUnitName == NULL) {
		pszUnitName = "未知单位";
	}

	//创建输出数据源
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" 驱动不可用.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//修改这里以设置输出文件的路径和名称
	const char* pszOutputFilePath = sFilePath;
	GDALDataset* poDS_out = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDS_out == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出数据源创建失败.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//创建输出图层
	OGRLayer* poLayer_out = poDS_out->CreateLayer("buffered_layer", poSRS, wkbPolygon, NULL);
	if (poLayer_out == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出图层创建失败.") << endl;
		GDALClose(poDataset);
		GDALClose(poDS_out);
		return -1;
	}

	//遍历输入图层中的每个要素并进行缓冲区分析
	OGRFeature* poFeature;
	while ((poFeature = poLayer->GetNextFeature()) != NULL) {
		OGRGeometry* poGeometry = poFeature->GetGeometryRef();
		if (poGeometry != NULL) {
			OGRwkbGeometryType geomType = wkbFlatten(poGeometry->getGeometryType());
			OGRGeometry* poBuffer = NULL;

			//判断几何类型并生成对应的缓冲区
			switch (geomType) {
			case wkbPoint:
			case wkbMultiPoint:
				poBuffer = poGeometry->Buffer(dRadius, 30); //对点生成圆形缓冲区
				break;
			case wkbLineString:
			case wkbMultiLineString:
				poBuffer = poGeometry->Buffer(dRadius, 30); //对线生成缓冲区
				break;
			case wkbPolygon:
			case wkbMultiPolygon:
				poBuffer = poGeometry->Buffer(dRadius, 30); //对面生成缓冲区
				break;
			default:
				qDebug().noquote() << QString::fromLocal8Bit("不支持的几何类型.") << endl;
				break;
			}

			if (poBuffer != NULL) {
				//创建新的要素
				OGRFeature* poFeature_out = OGRFeature::CreateFeature(poLayer_out->GetLayerDefn());
				poFeature_out->SetGeometry(poBuffer);
				if (poLayer_out->CreateFeature(poFeature_out) != OGRERR_NONE) {
					qDebug().noquote() << QString::fromLocal8Bit("要素创建失败.") << endl;
				}
				OGRFeature::DestroyFeature(poFeature_out);
				//销毁缓冲区几何
				delete poBuffer;
			}
		}
		OGRFeature::DestroyFeature(poFeature);
	}
	
	GDALClose(poDS_out);

	return 0;
}
//凸包计算
int DataInput::Convex(GDALDataset* poDataset, const char* sFilePath) {
	//获取图层
	OGRLayer* poLayer = poDataset->GetLayer(0);
	if (poLayer == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("图层获取失败.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//获取图层的空间参考
	OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
	if (poSRS == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("无法获取图层的空间参考.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	// 创建一个几何集合
	OGRGeometryCollection* poGeometryCollection = (OGRGeometryCollection*)OGRGeometryFactory::createGeometry(wkbGeometryCollection);

	OGRFeature* poFeature;
	poLayer->ResetReading();
	while ((poFeature = poLayer->GetNextFeature()) != NULL) {
		OGRGeometry* poGeometry = poFeature->GetGeometryRef();
		if (poGeometry != NULL) {
			poGeometryCollection->addGeometry(poGeometry);
		}
		OGRFeature::DestroyFeature(poFeature);
	}

	//计算凸包
	OGRGeometry* poConvexHull = poGeometryCollection->ConvexHull();

	//创建输出数据源
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (poDriver == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("获取驱动失败.") << endl;
		OGRGeometryFactory::destroyGeometry(poGeometryCollection);
		OGRGeometryFactory::destroyGeometry(poConvexHull);
		GDALClose(poDataset);
		return -1;
	}

	const char* pszOutputFilePath = sFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("创建输出数据源失败.") << endl;
		OGRGeometryFactory::destroyGeometry(poGeometryCollection);
		OGRGeometryFactory::destroyGeometry(poConvexHull);
		GDALClose(poDataset);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("convex_hull", poSRS, wkbPolygon, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("创建输出图层失败.") << endl;
		OGRGeometryFactory::destroyGeometry(poGeometryCollection);
		OGRGeometryFactory::destroyGeometry(poConvexHull);
		GDALClose(poDataset);
		GDALClose(poDSOut);
		return -1;
	}

	OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
	poFeatureOut->SetGeometry(poConvexHull);
	if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
		qDebug().noquote() << QString::fromLocal8Bit("创建要素失败.") << endl;
	}

	OGRFeature::DestroyFeature(poFeatureOut);
	OGRGeometryFactory::destroyGeometry(poGeometryCollection);
	OGRGeometryFactory::destroyGeometry(poConvexHull);

	GDALClose(poDSOut);
	return 0;
}

//叠加分析
//交集
int DataInput::IntersectLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath) {
	//注册矢量驱动
	GDALAllRegister();

	//打开矢量数据文件
	const char* filename1 = inputFeatureFilePath_1;
	GDALDataset* poDataset1;
	poDataset1 = static_cast<GDALDataset*>(GDALOpenEx(filename1, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset1 == nullptr)
	{
		qDebug() << "无法打开文件: " << filename1 << endl;
		return -1;
	}

	const char* filename2 = inputFeatureFilePath_2;
	GDALDataset* poDataset2;
	poDataset2 = static_cast<GDALDataset*>(GDALOpenEx(filename2, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset2 == nullptr)
	{
		qDebug() << "无法打开文件: " << filename2 << endl;
		return -1;
	}

	OGRLayer* poLayer1 = poDataset1->GetLayer(0);
	OGRLayer* poLayer2 = poDataset2->GetLayer(0);

	if (poLayer1 == NULL || poLayer2 == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("图层获取失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//创建输出数据源
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" 驱动不可用.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//修改这里以设置输出文件的路径和名称
	const char* pszOutputFilePath = outputFeatureFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出数据源创建失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("intersect", poLayer1->GetSpatialRef(), wkbUnknown, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出图层创建失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		GDALClose(poDSOut);
		return -1;
	}

	OGRFeature* poFeature1;
	poLayer1->ResetReading();

	while ((poFeature1 = poLayer1->GetNextFeature()) != NULL) {
		OGRGeometry* poGeom1 = poFeature1->GetGeometryRef();
		if (poGeom1 == NULL) {
			OGRFeature::DestroyFeature(poFeature1);
			continue;
		}

		OGRFeature* poFeature2;
		poLayer2->ResetReading();

		while ((poFeature2 = poLayer2->GetNextFeature()) != NULL) {
			OGRGeometry* poGeom2 = poFeature2->GetGeometryRef();
			if (poGeom2 == NULL) {
				OGRFeature::DestroyFeature(poFeature2);
				continue;
			}

			OGRGeometry* poIntersection = poGeom1->Intersection(poGeom2);
			if (poIntersection != NULL && !poIntersection->IsEmpty()) {
				OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
				poFeatureOut->SetGeometry(poIntersection);
				if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
					qDebug().noquote() << QString::fromLocal8Bit("无法在输出图层中创建要素.") << endl;
				}
				OGRFeature::DestroyFeature(poFeatureOut);
				delete poIntersection;
			}
			OGRFeature::DestroyFeature(poFeature2);
		}
		OGRFeature::DestroyFeature(poFeature1);
	}

	GDALClose(poDataset1);
	GDALClose(poDataset2);
	GDALClose(poDSOut);

	return 0;
}
//并集
int DataInput::UnionLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath) {
	//注册矢量驱动
	GDALAllRegister();

	//打开矢量数据文件
	const char* filename1 = inputFeatureFilePath_1;
	GDALDataset* poDataset1;
	poDataset1 = static_cast<GDALDataset*>(GDALOpenEx(filename1, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset1 == nullptr)
	{
		qDebug() << "无法打开文件: " << filename1 << endl;
		return -1;
	}

	const char* filename2 = inputFeatureFilePath_2;
	GDALDataset* poDataset2;
	poDataset2 = static_cast<GDALDataset*>(GDALOpenEx(filename2, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset2 == nullptr)
	{
		qDebug() << "无法打开文件: " << filename2 << endl;
		return -1;
	}

	OGRLayer* poLayer1 = poDataset1->GetLayer(0);
	OGRLayer* poLayer2 = poDataset2->GetLayer(0);

	if (poLayer1 == NULL || poLayer2 == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("图层获取失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//创建输出数据源
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" 驱动不可用.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//修改这里以设置输出文件的路径和名称
	const char* pszOutputFilePath = outputFeatureFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出数据源创建失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("intersect", poLayer1->GetSpatialRef(), wkbUnknown, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出图层创建失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		GDALClose(poDSOut);
		return -1;
	}

	OGRFeature* poFeature1;
	poLayer1->ResetReading();

	//将第一个图层的所有要素添加到输出图层
	while ((poFeature1 = poLayer1->GetNextFeature()) != NULL) {
		OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
		poFeatureOut->SetGeometry(poFeature1->GetGeometryRef());
		if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
			qDebug().noquote() << QString::fromLocal8Bit("无法在输出图层中创建要素.") << endl;
		}
		OGRFeature::DestroyFeature(poFeatureOut);
		OGRFeature::DestroyFeature(poFeature1);
	}

	OGRFeature* poFeature2;
	poLayer2->ResetReading();

	//将第二个图层的所有要素添加到输出图层
	while ((poFeature2 = poLayer2->GetNextFeature()) != NULL) {
		OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
		poFeatureOut->SetGeometry(poFeature2->GetGeometryRef());
		if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
			qDebug().noquote() << QString::fromLocal8Bit("无法在输出图层中创建要素.") << endl;
		}
		OGRFeature::DestroyFeature(poFeatureOut);
		OGRFeature::DestroyFeature(poFeature2);
	}

	GDALClose(poDataset1);
	GDALClose(poDataset2);
	GDALClose(poDSOut);

	return 0;
}
//擦除
int DataInput::EraseLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath) {
	//注册矢量驱动
	GDALAllRegister();

	//打开矢量数据文件
	const char* filename1 = inputFeatureFilePath_1;
	GDALDataset* poDataset1;
	poDataset1 = static_cast<GDALDataset*>(GDALOpenEx(filename1, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset1 == nullptr)
	{
		qDebug() << "无法打开文件: " << filename1 << endl;
		return -1;
	}

	const char* filename2 = inputFeatureFilePath_2;
	GDALDataset* poDataset2;
	poDataset2 = static_cast<GDALDataset*>(GDALOpenEx(filename2, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset2 == nullptr)
	{
		qDebug() << "无法打开文件: " << filename2 << endl;
		return -1;
	}

	OGRLayer* poLayer1 = poDataset1->GetLayer(0);
	OGRLayer* poLayer2 = poDataset2->GetLayer(0);

	if (poLayer1 == NULL || poLayer2 == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("图层获取失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//创建输出数据源
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" 驱动不可用.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//修改这里以设置输出文件的路径和名称
	const char* pszOutputFilePath = outputFeatureFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出数据源创建失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("intersect", poLayer1->GetSpatialRef(), wkbUnknown, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("输出图层创建失败.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		GDALClose(poDSOut);
		return -1;
	}

	OGRFeature* poFeatureInput;
	poLayer1->ResetReading();

	while ((poFeatureInput = poLayer1->GetNextFeature()) != NULL) {
		OGRGeometry* poGeomInput = poFeatureInput->GetGeometryRef();
		if (poGeomInput == NULL) {
			OGRFeature::DestroyFeature(poFeatureInput);
			continue;
		}

		OGRGeometry* poGeomResult = poGeomInput->clone();
		OGRFeature* poFeatureErase;
		poLayer2->ResetReading();

		while ((poFeatureErase = poLayer2->GetNextFeature()) != NULL) {
			OGRGeometry* poGeomErase = poFeatureErase->GetGeometryRef();
			if (poGeomErase == NULL) {
				OGRFeature::DestroyFeature(poFeatureErase);
				continue;
			}

			OGRGeometry* poGeomDiff = poGeomResult->Difference(poGeomErase);
			if (poGeomDiff != NULL && !poGeomDiff->IsEmpty()) {
				delete poGeomResult;
				poGeomResult = poGeomDiff;
			}
			else {
				delete poGeomDiff;
			}
			OGRFeature::DestroyFeature(poFeatureErase);
		}

		if (!poGeomResult->IsEmpty()) {
			OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
			poFeatureOut->SetGeometry(poGeomResult);
			if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
				qDebug().noquote() << QString::fromLocal8Bit("无法在输出图层中创建要素.") << endl;
			}
			OGRFeature::DestroyFeature(poFeatureOut);
		}
		delete poGeomResult;
		OGRFeature::DestroyFeature(poFeatureInput);
	}

	GDALClose(poDataset1);
	GDALClose(poDataset2);
	GDALClose(poDSOut);

	return 0;
}

