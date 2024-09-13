#include "DataInput.h"
#include <QFileDialog>
#include <QDebug>
#include <QPushButton>


//����������&͹������
DataInput::DataInput(QPlainTextEdit* pLogger, int nSelect, QMap<QString, GDALDataset*> datasetMap, QWidget* parent)
	: mnSelect(nSelect)
	, mDatasetMap(datasetMap)
	, QDialog(parent)
	, mpLog(nullptr)
	, mpDataset(nullptr)
{
	ui.setupUi(this);
	mpLog = new logger("ʸ������", pLogger);
	ui.fileBtn->setStyleSheet("border:none;");	//ȥ����ť����
	//��������ͼ���б�
	QStringList layers = datasetMap.keys();
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	ui.inputCombo->addItems(vectorLayers);

	//ѡ������ͼ��
	connect(ui.fileButton2, &QPushButton::clicked, [&]() {
		auto sFilePath2 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp)");
		QStringList temp = sFilePath2.split("/");					//ͨ��·���ָ����ָ��ַ���
		QString basename = temp.at(temp.size() - 1).split(".")[0];	//��ȡ�ļ���
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
		});

	//ѡ������ļ���
	connect(ui.fileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("����ļ�").toUtf8().constData()),
			"./output", "Shapefile(*.shp)");
		ui.fileEdit->setText(sFilePath);
		});

	//ȷ����ťִ�л���������
	connect(ui.sureBtn, &QPushButton::clicked, [&]() {
		QString sLayerName = ui.inputCombo->currentText();
		mpDataset = mDatasetMap[sLayerName];
		double dRadius = ui.radiusEdit->text().toUtf8().toDouble();
		QString filePath = ui.fileEdit->text();
		QByteArray byteArray = filePath.toUtf8();
		const char* sFilePath = byteArray.constData();
		if (mpDataset != nullptr) {
			if (mnSelect == 1) {
				auto info = "���ڶ�ͼ��" + sLayerName.toStdString() + "ִ��͹������...";
				mpLog->logInfo(info.c_str());
				if (Convex(mpDataset, sFilePath) == 0) {
					mpLog->logInfo("͹������ɹ�");
				}
				else {
					mpLog->logWarnInfo("͹������ʧ��");
				}
			}
			else if (mnSelect == 2) {
				auto info = "���ڶ�ͼ��" + sLayerName.toStdString() + "ִ�л���������...";
				mpLog->logInfo(info.c_str());
				if (Buffer(dRadius, mpDataset, sFilePath) == 0) {
					mpLog->logInfo("�����������ɹ�");
				}
				else {
					mpLog->logWarnInfo("����������ʧ��");
				}
			}
		}
		this->close();
		});

	//ȡ����ť,����ı���
	connect(ui.cancelBtn, &QPushButton::clicked, [&]() {
		ui.radiusEdit->clear();
		ui.fileEdit->clear();
		mpLog->logInfo("ȡ��");
		this->close();
		});
}
//���ӷ���
DataInput::DataInput(QPlainTextEdit* pLogger, QMap<QString, GDALDataset*> datasetMap, QWidget* parent, int nSelect)
	: QDialog(parent)
	, mnSelect(nSelect)
	, mDatasetMap(datasetMap)
	, mpDataset(nullptr)
	, mpLog(nullptr)
{
	ui.setupUi(this);
	mpLog = new logger("ʸ������", pLogger);
	ui.fileBtn->setStyleSheet("border:none;");
	//���µ���Ҫ��ͼ���б�������ͼ���б�
	QStringList layers = datasetMap.keys();
	QStringList vectorLayers;
	for (const auto& layer : layers) {
		if (!layer.endsWith(".tif") && !layer.endsWith(".TIF")) {
			vectorLayers << layer;
		}
	}
	ui.overlayCambo->addItems(vectorLayers);
	ui.inputCombo->addItems(vectorLayers);
	
	//ѡ������ļ�
	connect(ui.fileBtn, &QPushButton::clicked, [&]() {
		auto sFilePath = QFileDialog::getSaveFileName(this, tr(QString::fromLocal8Bit("����ļ�").toUtf8().constData()),
			"./output", "Shapefile (*.shp)");
		ui.fileEdit->setText(sFilePath);
		});
	//ѡ�����ͼ��
	connect(ui.fileButton1, &QPushButton::clicked, [&]() {
		auto sFilePath1 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp)");
		QStringList temp = sFilePath1.split("/");					//ͨ��·���ָ����ָ��ַ���
		QString basename = temp.at(temp.size() - 1).split(".")[0];	//��ȡ�ļ���
		ui.overlayCambo->addItem(basename);
		ui.overlayCambo->setCurrentText(basename);
		});
	//ѡ������ͼ��
	connect(ui.fileButton2, &QPushButton::clicked, [&]() {
		auto sFilePath2 = QFileDialog::getOpenFileName(this, "OpenFile", "./data", "(*.shp)");
		QStringList temp = sFilePath2.split("/");					//ͨ��·���ָ����ָ��ַ���
		QString basename = temp.at(temp.size() - 1).split(".")[0];	//��ȡ�ļ���
		ui.inputCombo->addItem(basename);
		ui.inputCombo->setCurrentText(basename);
		});
	//ִ�е��ӷ���
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

		//����
		if (mnSelect == 1) {
			mpLog->logInfo("����ִ�н�������...");
			if (IntersectLayers(sFilePath2, sFilePath3, sFilePath1) == 0) {
				mpLog->logInfo("��������ɹ�");
			}
			else {
				mpLog->logInfo("��������ʧ��");
			}
		}
		//����
		else if (mnSelect == 2) {
			mpLog->logInfo("����ִ�в�������...");
			if (UnionLayers(sFilePath2, sFilePath3, sFilePath1) == 0) {
				mpLog->logInfo("��������ɹ�");
			}
			else {
				mpLog->logInfo("��������ʧ��");
			}
		}
		//����
		else if (mnSelect == 3) {
			mpLog->logInfo("����ִ�в�������...");
			if (EraseLayers(sFilePath2, sFilePath3, sFilePath1) == 0) {
				mpLog->logInfo("��������ɹ�");
			}
			else {
				mpLog->logInfo("��������ʧ��");
			}
		}

		this->close();
		});
	//ȡ��
	connect(ui.cancelBtn, &QPushButton::clicked, [&]() {
		ui.overlayCambo->clear();
		ui.inputCombo->clear();
		ui.fileEdit->clear();
		mpLog->logInfo("ȡ��");
		this->close();
		});
}
DataInput::~DataInput() {}

//���ÿؼ��ɼ���
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


//����������
int DataInput::Buffer(double dRadius, GDALDataset* poDataset, const char* sFilePath) {
	int nCount = poDataset->GetLayerCount();
	qDebug() << nCount << endl;
	//��ȡͼ��
	OGRLayer* poLayer = poDataset->GetLayer(0);
	poLayer->ResetReading();
	if (poLayer == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("ͼ���ȡʧ��.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//��ȡͼ��Ŀռ�ο�
	OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
	if (poSRS == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�޷���ȡͼ��Ŀռ�ο�.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//��ȡ����ϵ��λ
	const char* pszUnitName = poSRS->GetAttrValue("UNIT", 0);
	if (pszUnitName == NULL) {
		pszUnitName = "δ֪��λ";
	}

	//�����������Դ
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" ����������.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//�޸���������������ļ���·��������
	const char* pszOutputFilePath = sFilePath;
	GDALDataset* poDS_out = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDS_out == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�������Դ����ʧ��.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//�������ͼ��
	OGRLayer* poLayer_out = poDS_out->CreateLayer("buffered_layer", poSRS, wkbPolygon, NULL);
	if (poLayer_out == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("���ͼ�㴴��ʧ��.") << endl;
		GDALClose(poDataset);
		GDALClose(poDS_out);
		return -1;
	}

	//��������ͼ���е�ÿ��Ҫ�ز����л���������
	OGRFeature* poFeature;
	while ((poFeature = poLayer->GetNextFeature()) != NULL) {
		OGRGeometry* poGeometry = poFeature->GetGeometryRef();
		if (poGeometry != NULL) {
			OGRwkbGeometryType geomType = wkbFlatten(poGeometry->getGeometryType());
			OGRGeometry* poBuffer = NULL;

			//�жϼ������Ͳ����ɶ�Ӧ�Ļ�����
			switch (geomType) {
			case wkbPoint:
			case wkbMultiPoint:
				poBuffer = poGeometry->Buffer(dRadius, 30); //�Ե�����Բ�λ�����
				break;
			case wkbLineString:
			case wkbMultiLineString:
				poBuffer = poGeometry->Buffer(dRadius, 30); //�������ɻ�����
				break;
			case wkbPolygon:
			case wkbMultiPolygon:
				poBuffer = poGeometry->Buffer(dRadius, 30); //�������ɻ�����
				break;
			default:
				qDebug().noquote() << QString::fromLocal8Bit("��֧�ֵļ�������.") << endl;
				break;
			}

			if (poBuffer != NULL) {
				//�����µ�Ҫ��
				OGRFeature* poFeature_out = OGRFeature::CreateFeature(poLayer_out->GetLayerDefn());
				poFeature_out->SetGeometry(poBuffer);
				if (poLayer_out->CreateFeature(poFeature_out) != OGRERR_NONE) {
					qDebug().noquote() << QString::fromLocal8Bit("Ҫ�ش���ʧ��.") << endl;
				}
				OGRFeature::DestroyFeature(poFeature_out);
				//���ٻ���������
				delete poBuffer;
			}
		}
		OGRFeature::DestroyFeature(poFeature);
	}
	
	GDALClose(poDS_out);

	return 0;
}
//͹������
int DataInput::Convex(GDALDataset* poDataset, const char* sFilePath) {
	//��ȡͼ��
	OGRLayer* poLayer = poDataset->GetLayer(0);
	if (poLayer == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("ͼ���ȡʧ��.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	//��ȡͼ��Ŀռ�ο�
	OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
	if (poSRS == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�޷���ȡͼ��Ŀռ�ο�.") << endl;
		GDALClose(poDataset);
		return -1;
	}

	// ����һ�����μ���
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

	//����͹��
	OGRGeometry* poConvexHull = poGeometryCollection->ConvexHull();

	//�����������Դ
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (poDriver == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("��ȡ����ʧ��.") << endl;
		OGRGeometryFactory::destroyGeometry(poGeometryCollection);
		OGRGeometryFactory::destroyGeometry(poConvexHull);
		GDALClose(poDataset);
		return -1;
	}

	const char* pszOutputFilePath = sFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�����������Դʧ��.") << endl;
		OGRGeometryFactory::destroyGeometry(poGeometryCollection);
		OGRGeometryFactory::destroyGeometry(poConvexHull);
		GDALClose(poDataset);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("convex_hull", poSRS, wkbPolygon, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�������ͼ��ʧ��.") << endl;
		OGRGeometryFactory::destroyGeometry(poGeometryCollection);
		OGRGeometryFactory::destroyGeometry(poConvexHull);
		GDALClose(poDataset);
		GDALClose(poDSOut);
		return -1;
	}

	OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
	poFeatureOut->SetGeometry(poConvexHull);
	if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
		qDebug().noquote() << QString::fromLocal8Bit("����Ҫ��ʧ��.") << endl;
	}

	OGRFeature::DestroyFeature(poFeatureOut);
	OGRGeometryFactory::destroyGeometry(poGeometryCollection);
	OGRGeometryFactory::destroyGeometry(poConvexHull);

	GDALClose(poDSOut);
	return 0;
}

//���ӷ���
//����
int DataInput::IntersectLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath) {
	//ע��ʸ������
	GDALAllRegister();

	//��ʸ�������ļ�
	const char* filename1 = inputFeatureFilePath_1;
	GDALDataset* poDataset1;
	poDataset1 = static_cast<GDALDataset*>(GDALOpenEx(filename1, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset1 == nullptr)
	{
		qDebug() << "�޷����ļ�: " << filename1 << endl;
		return -1;
	}

	const char* filename2 = inputFeatureFilePath_2;
	GDALDataset* poDataset2;
	poDataset2 = static_cast<GDALDataset*>(GDALOpenEx(filename2, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset2 == nullptr)
	{
		qDebug() << "�޷����ļ�: " << filename2 << endl;
		return -1;
	}

	OGRLayer* poLayer1 = poDataset1->GetLayer(0);
	OGRLayer* poLayer2 = poDataset2->GetLayer(0);

	if (poLayer1 == NULL || poLayer2 == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("ͼ���ȡʧ��.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//�����������Դ
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" ����������.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//�޸���������������ļ���·��������
	const char* pszOutputFilePath = outputFeatureFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�������Դ����ʧ��.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("intersect", poLayer1->GetSpatialRef(), wkbUnknown, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("���ͼ�㴴��ʧ��.") << endl;
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
					qDebug().noquote() << QString::fromLocal8Bit("�޷������ͼ���д���Ҫ��.") << endl;
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
//����
int DataInput::UnionLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath) {
	//ע��ʸ������
	GDALAllRegister();

	//��ʸ�������ļ�
	const char* filename1 = inputFeatureFilePath_1;
	GDALDataset* poDataset1;
	poDataset1 = static_cast<GDALDataset*>(GDALOpenEx(filename1, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset1 == nullptr)
	{
		qDebug() << "�޷����ļ�: " << filename1 << endl;
		return -1;
	}

	const char* filename2 = inputFeatureFilePath_2;
	GDALDataset* poDataset2;
	poDataset2 = static_cast<GDALDataset*>(GDALOpenEx(filename2, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset2 == nullptr)
	{
		qDebug() << "�޷����ļ�: " << filename2 << endl;
		return -1;
	}

	OGRLayer* poLayer1 = poDataset1->GetLayer(0);
	OGRLayer* poLayer2 = poDataset2->GetLayer(0);

	if (poLayer1 == NULL || poLayer2 == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("ͼ���ȡʧ��.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//�����������Դ
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" ����������.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//�޸���������������ļ���·��������
	const char* pszOutputFilePath = outputFeatureFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�������Դ����ʧ��.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("intersect", poLayer1->GetSpatialRef(), wkbUnknown, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("���ͼ�㴴��ʧ��.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		GDALClose(poDSOut);
		return -1;
	}

	OGRFeature* poFeature1;
	poLayer1->ResetReading();

	//����һ��ͼ�������Ҫ����ӵ����ͼ��
	while ((poFeature1 = poLayer1->GetNextFeature()) != NULL) {
		OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
		poFeatureOut->SetGeometry(poFeature1->GetGeometryRef());
		if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
			qDebug().noquote() << QString::fromLocal8Bit("�޷������ͼ���д���Ҫ��.") << endl;
		}
		OGRFeature::DestroyFeature(poFeatureOut);
		OGRFeature::DestroyFeature(poFeature1);
	}

	OGRFeature* poFeature2;
	poLayer2->ResetReading();

	//���ڶ���ͼ�������Ҫ����ӵ����ͼ��
	while ((poFeature2 = poLayer2->GetNextFeature()) != NULL) {
		OGRFeature* poFeatureOut = OGRFeature::CreateFeature(poLayerOut->GetLayerDefn());
		poFeatureOut->SetGeometry(poFeature2->GetGeometryRef());
		if (poLayerOut->CreateFeature(poFeatureOut) != OGRERR_NONE) {
			qDebug().noquote() << QString::fromLocal8Bit("�޷������ͼ���д���Ҫ��.") << endl;
		}
		OGRFeature::DestroyFeature(poFeatureOut);
		OGRFeature::DestroyFeature(poFeature2);
	}

	GDALClose(poDataset1);
	GDALClose(poDataset2);
	GDALClose(poDSOut);

	return 0;
}
//����
int DataInput::EraseLayers(const char* inputFeatureFilePath_1, const char* inputFeatureFilePath_2, const char* outputFeatureFilePath) {
	//ע��ʸ������
	GDALAllRegister();

	//��ʸ�������ļ�
	const char* filename1 = inputFeatureFilePath_1;
	GDALDataset* poDataset1;
	poDataset1 = static_cast<GDALDataset*>(GDALOpenEx(filename1, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset1 == nullptr)
	{
		qDebug() << "�޷����ļ�: " << filename1 << endl;
		return -1;
	}

	const char* filename2 = inputFeatureFilePath_2;
	GDALDataset* poDataset2;
	poDataset2 = static_cast<GDALDataset*>(GDALOpenEx(filename2, GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

	if (poDataset2 == nullptr)
	{
		qDebug() << "�޷����ļ�: " << filename2 << endl;
		return -1;
	}

	OGRLayer* poLayer1 = poDataset1->GetLayer(0);
	OGRLayer* poLayer2 = poDataset2->GetLayer(0);

	if (poLayer1 == NULL || poLayer2 == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("ͼ���ȡʧ��.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//�����������Դ
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		qDebug().noquote() << pszDriverName << QString::fromLocal8Bit(" ����������.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	//�޸���������������ļ���·��������
	const char* pszOutputFilePath = outputFeatureFilePath;
	GDALDataset* poDSOut = poDriver->Create(pszOutputFilePath, 0, 0, 0, GDT_Unknown, NULL);
	if (poDSOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("�������Դ����ʧ��.") << endl;
		GDALClose(poDataset1);
		GDALClose(poDataset2);
		return -1;
	}

	OGRLayer* poLayerOut = poDSOut->CreateLayer("intersect", poLayer1->GetSpatialRef(), wkbUnknown, NULL);
	if (poLayerOut == NULL) {
		qDebug().noquote() << QString::fromLocal8Bit("���ͼ�㴴��ʧ��.") << endl;
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
				qDebug().noquote() << QString::fromLocal8Bit("�޷������ͼ���д���Ҫ��.") << endl;
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

