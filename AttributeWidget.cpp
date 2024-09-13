#include "AttributeWidget.h"
#include <QDebug>

AttributeWidget::AttributeWidget(logger* pLog, int nSelect, QWidget* parent, GDALDataset* dataset)
	: QDialog(parent)
{
	ui.setupUi(this);

	switch (nSelect) {
	case 1:	//ִ������ͳ��
		if (calculateNum(dataset) == 0) {
			pLog->logInfo("ִ������ͳ�Ƴɹ�!");
		}
		else {	//��ձ��
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	case 2:	//ִ�����ͳ��
		if (calculateArea(dataset) == 0) {
			pLog->logInfo("ִ�����ͳ�Ƴɹ�!");
		}
		else {	//��ձ��
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	case 3:	//ִ���ܳ�ͳ��
		if (calculatePerimeter(dataset) == 0) {
			pLog->logInfo("ִ���ܳ�ͳ�Ƴɹ�!");
		}
		else {	//��ձ��
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	case 4:	//ִ�г���ͳ��
		if (calculateLength(dataset) == 0) {
			pLog->logInfo("ִ������ͳ�Ƴɹ�!");
		}
		else {	//��ձ��
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	}
}

AttributeWidget::~AttributeWidget()
{}
//ͳ��Ҫ������
int AttributeWidget::calculateNum(GDALDataset* dataset) {
	//��ȡͼ������
	int layerCount = dataset->GetLayerCount();

	//����һ��map���洢Ҫ�����ͺ�����
	std::map<OGRwkbGeometryType, int> featureCount;

	//����������
	ui.tableWidget->setColumnCount(2);
	ui.tableWidget->setRowCount(20);
	//���ñ�ͷ
	QStringList headers;
	headers << QString::fromLocal8Bit("Ҫ������") << QString::fromLocal8Bit("����");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//����ÿ��ͼ��
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("�޷���ȡͼ��: ") << i << "\n";
			return -1;
		}

		//����ͼ���е�ÿ��Ҫ��
		layer->ResetReading();
		OGRFeature* feature = nullptr;
		while ((feature = layer->GetNextFeature()) != nullptr)
		{
			OGRGeometry* geometry = feature->GetGeometryRef();
			if (geometry != nullptr)
			{
				OGRwkbGeometryType geomType = geometry->getGeometryType();
				featureCount[geomType]++;
			}

			//�ͷ�Ҫ��
			OGRFeature::DestroyFeature(feature);
		}
	}
	//�������Ҫ������
	for (const auto& entry : featureCount)
	{
		QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
		QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
		ui.tableWidget->setItem(nR, nC++, pTypeItem);  // �ڵ�nR�е�nC�в�������
		ui.tableWidget->setItem(nR++, nC, pCountItem); // �ڵ�nR�е�nC+1�в�������
		nC = 0;
		qDebug().noquote() << QString::fromLocal8Bit("Ҫ�����ͣ�") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit("��������") << entry.second << "\n";
	}
	return 0;
}
//ͳ����Ҫ�ص����
int AttributeWidget::calculateArea(GDALDataset* dataset) {
	//��ȡͼ������
	int layerCount = dataset->GetLayerCount();
	qDebug().noquote() << QString::fromLocal8Bit("ͼ������: ") << layerCount << "\n";

	//����һ��map���洢Ҫ�����ͺ������
	std::map<OGRwkbGeometryType, double> areaSum;

	//����������
	ui.tableWidget->setColumnCount(3);
	ui.tableWidget->setRowCount(10000);
	//���ñ�ͷ
	QStringList headers;
	headers << QString::fromLocal8Bit("ͼ���") << QString::fromLocal8Bit("Ҫ������") << QString::fromLocal8Bit("���");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//����ÿ��ͼ��
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("�޷���ȡͼ��: ") << i << "\n";
			return -1;
		}

		//����ͼ���е�ÿ��Ҫ��
		layer->ResetReading();
		OGRFeature* feature = nullptr;


		while ((feature = layer->GetNextFeature()) != nullptr)
		{
			OGRGeometry* geometry = feature->GetGeometryRef();
			if (geometry != nullptr)
			{
				//����Ҫ�����
				double area = 0.0;
				OGRwkbGeometryType geomType = geometry->getGeometryType();
				if (wkbFlatten(geomType) == wkbPolygon || wkbFlatten(geomType) == wkbMultiPolygon)
				{
					area = geometry->toPolygon()->get_Area();
					QTableWidgetItem* pLayerItem = new QTableWidgetItem(QString("%1").arg(i + 1));
					ui.tableWidget->setItem(nR, nC++, pLayerItem); // �ڵ�nR�е�nC�в�������
					QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(geomType));
					ui.tableWidget->setItem(nR, nC++, pTypeItem); // �ڵ�nR�е�nC+1�в�������
					QTableWidgetItem* pAreaItem = new QTableWidgetItem(QString("%1").arg(area));
					ui.tableWidget->setItem(nR++, nC, pAreaItem); // �ڵ�nR�е�nC+2�в�������
					nC = 0;
				}
				//�ۼ�Ҫ�����͵������
				areaSum[geomType] += area;
			}

			//�ͷ�Ҫ��
			OGRFeature::DestroyFeature(feature);
		}
	}

	//�������Ҫ�ص������
	for (const auto& entry : areaSum)
	{
		QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
		QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
		ui.tableWidget->setItem(nR, ++nC, pTypeItem); // �ڵ�nR�е�nC�в�������
		ui.tableWidget->setItem(nR, ++nC, pCountItem); // �ڵ�nR�е�nC+1�в�������
		nR++;
		nC = 0;
		qDebug().noquote() << QString::fromLocal8Bit("Ҫ������: ") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit(", �����: ") << entry.second << "\n";
	}
	return 0;
}
//ͳ����Ҫ�ص��ܳ�
int AttributeWidget::calculatePerimeter(GDALDataset* dataset) {
	//��ȡͼ������
	int layerCount = dataset->GetLayerCount();
	qDebug().noquote() << QString::fromLocal8Bit("ͼ������: ") << layerCount << endl;

	//����һ��map���洢Ҫ�����ͺ����ܳ�
	std::map<OGRwkbGeometryType, double> perimeterSum;

	//����������
	ui.tableWidget->setColumnCount(3);
	ui.tableWidget->setRowCount(10000);
	//���ñ�ͷ
	QStringList headers;
	headers << QString::fromLocal8Bit("ͼ���") << QString::fromLocal8Bit("Ҫ������") << QString::fromLocal8Bit("�ܳ�");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//����ÿ��ͼ��
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("�޷���ȡͼ��: ") << i << endl;
			return -1;
		}

		//����ͼ���е�ÿ��Ҫ��
		layer->ResetReading();
		OGRFeature* feature = nullptr;

		while ((feature = layer->GetNextFeature()) != nullptr)
		{
			OGRGeometry* geometry = feature->GetGeometryRef();
			if (geometry != nullptr)
			{
				//����Ҫ���ܳ�
				double perimeter = 0.0;
				OGRwkbGeometryType geomType = geometry->getGeometryType();
				if (wkbFlatten(geomType) == wkbPolygon)				//��
				{
					OGRPolygon* polygon = dynamic_cast<OGRPolygon*>(geometry);
					if (polygon != nullptr)
					{
						OGRLinearRing* ring = polygon->getExteriorRing();
						if (ring != nullptr)
						{
							perimeter = ring->get_Length();
						}
					}
				}
				else if (wkbFlatten(geomType) == wkbMultiPolygon)	//����
				{
					OGRMultiPolygon* multiPolygon = dynamic_cast<OGRMultiPolygon*>(geometry);
					if (multiPolygon != nullptr)
					{
						for (int j = 0; j < multiPolygon->getNumGeometries(); ++j)
						{
							OGRPolygon* subPolygon = dynamic_cast<OGRPolygon*>(multiPolygon->getGeometryRef(j));
							if (subPolygon != nullptr)
							{
								OGRLinearRing* ring = subPolygon->getExteriorRing();
								if (ring != nullptr)
								{
									perimeter += ring->get_Length();
								}
							}
						}
					}
				}

				//������
				QTableWidgetItem* pLayerItem = new QTableWidgetItem(QString("%1").arg(i + 1));
				QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(geomType));
				QTableWidgetItem* pPerimeterItem = new QTableWidgetItem(QString("%1").arg(perimeter));
				ui.tableWidget->setItem(nR, nC++, pLayerItem);		// �ڵ�nR�е�nC�в���ͼ���
				ui.tableWidget->setItem(nR, nC++, pTypeItem);		// �ڵ�nR�е�nC+1�в���Ҫ������
				ui.tableWidget->setItem(nR, nC++, pPerimeterItem);  // �ڵ�nR�е�nC+2�в���Ҫ���ܳ�
				nR++;
				nC = 0;
				//�ۼ�Ҫ�����͵����ܳ�
				perimeterSum[geomType] += perimeter;

			}
			//�ͷ�Ҫ��
			OGRFeature::DestroyFeature(feature);
		}
		//�������Ҫ�ص����ܳ�
		for (const auto& entry : perimeterSum)
		{
			QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
			QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
			ui.tableWidget->setItem(nR, ++nC, pTypeItem);		// �ڵ�nR�е�nC�в�������
			ui.tableWidget->setItem(nR++, ++nC, pCountItem);	// �ڵ�nR�е�nC+1�в�������
			nC = 0;
			qDebug().noquote() << QString::fromLocal8Bit("Ҫ������: ") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit(", ���ܳ�: ") << entry.second << "\n";
		}
	}
	return 0;
}
//ͳ����Ҫ�س���
int AttributeWidget::calculateLength(GDALDataset* dataset) {
	//��ȡͼ������
	int layerCount = dataset->GetLayerCount();

	//����һ��map���洢Ҫ�����ͺ��ܳ���
	std::map<OGRwkbGeometryType, double> lengthSum;

	//����������
	ui.tableWidget->setColumnCount(3);
	ui.tableWidget->setRowCount(20000);
	//���ñ�ͷ
	QStringList headers;
	headers << QString::fromLocal8Bit("ͼ���") << QString::fromLocal8Bit("Ҫ������") << QString::fromLocal8Bit("����");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//����ÿ��ͼ��
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("�޷���ȡͼ��: ") << i << endl;
			return -1;
		}

		//����ͼ���е�ÿ��Ҫ��
		layer->ResetReading();
		OGRFeature* feature = nullptr;
		while ((feature = layer->GetNextFeature()) != nullptr)
		{
			OGRGeometry* geometry = feature->GetGeometryRef();
			if (geometry != nullptr)
			{
				//����Ҫ�س���
				double length = 0.0;
				OGRwkbGeometryType geomType = geometry->getGeometryType();
				if (wkbFlatten(geomType) == wkbLineString)			 //��
				{
					OGRLineString* lineString = dynamic_cast<OGRLineString*>(geometry);
					if (lineString != nullptr)
					{
						length = lineString->get_Length();
					}
				}
				else if (wkbFlatten(geomType) == wkbMultiLineString) //����
				{
					OGRMultiLineString* multiLineString = dynamic_cast<OGRMultiLineString*>(geometry);
					if (multiLineString != nullptr)
					{
						for (int j = 0; j < multiLineString->getNumGeometries(); ++j)
						{
							OGRLineString* subLineString = dynamic_cast<OGRLineString*>(multiLineString->getGeometryRef(j));
							if (subLineString != nullptr)
							{
								length += subLineString->get_Length();
							}
						}
					}
				}

				//������
				QTableWidgetItem* pLayerItem = new QTableWidgetItem(QString("%1").arg(i + 1));
				QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(geomType));
				QTableWidgetItem* pLengthItem = new QTableWidgetItem(QString("%1").arg(length));
				ui.tableWidget->setItem(nR, nC++, pLayerItem);      // �ڵ�nR�е�nC�в���ͼ���
				ui.tableWidget->setItem(nR, nC++, pTypeItem);		// �ڵ�nR�е�nC+1�в���Ҫ������
				ui.tableWidget->setItem(nR++, nC++, pLengthItem);   // �ڵ�nR�е�nC+2�в���Ҫ���ܳ�
				nC = 0;
				//�ۼ�Ҫ�����͵����ܳ�
				lengthSum[geomType] += length;
			}
			//�ͷ�Ҫ��
			OGRFeature::DestroyFeature(feature);
		}
	}

	//�������Ҫ������
	if (lengthSum.empty()) {
		qDebug().noquote() << QString::fromLocal8Bit("û����Ҫ��!") << endl;
		return -1;
	}
	for (const auto& entry : lengthSum)
	{
		QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
		QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
		ui.tableWidget->setItem(nR, ++nC, pTypeItem);  //�ڵ�nR�е�nC�в�������
		ui.tableWidget->setItem(nR++, ++nC, pCountItem); //�ڵ�nR�е�nC+1�в�������
		nC = 0;
		qDebug().noquote() << QString::fromLocal8Bit("Ҫ�����ͣ�") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit("���ܳ��ȣ�") << entry.second << endl;
	}
	return 0;
}