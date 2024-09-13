#include "AttributeWidget.h"
#include <QDebug>

AttributeWidget::AttributeWidget(logger* pLog, int nSelect, QWidget* parent, GDALDataset* dataset)
	: QDialog(parent)
{
	ui.setupUi(this);

	switch (nSelect) {
	case 1:	//执行数量统计
		if (calculateNum(dataset) == 0) {
			pLog->logInfo("执行数量统计成功!");
		}
		else {	//清空表格
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	case 2:	//执行面积统计
		if (calculateArea(dataset) == 0) {
			pLog->logInfo("执行面积统计成功!");
		}
		else {	//清空表格
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	case 3:	//执行周长统计
		if (calculatePerimeter(dataset) == 0) {
			pLog->logInfo("执行周长统计成功!");
		}
		else {	//清空表格
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	case 4:	//执行长度统计
		if (calculateLength(dataset) == 0) {
			pLog->logInfo("执行数量统计成功!");
		}
		else {	//清空表格
			ui.tableWidget->setRowCount(0);
			ui.tableWidget->setColumnCount(0);
		}
		break;
	}
}

AttributeWidget::~AttributeWidget()
{}
//统计要素数量
int AttributeWidget::calculateNum(GDALDataset* dataset) {
	//获取图层数量
	int layerCount = dataset->GetLayerCount();

	//定义一个map来存储要素类型和数量
	std::map<OGRwkbGeometryType, int> featureCount;

	//设置行列数
	ui.tableWidget->setColumnCount(2);
	ui.tableWidget->setRowCount(20);
	//设置表头
	QStringList headers;
	headers << QString::fromLocal8Bit("要素类型") << QString::fromLocal8Bit("数量");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//遍历每个图层
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("无法获取图层: ") << i << "\n";
			return -1;
		}

		//遍历图层中的每个要素
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

			//释放要素
			OGRFeature::DestroyFeature(feature);
		}
	}
	//输出各类要素数量
	for (const auto& entry : featureCount)
	{
		QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
		QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
		ui.tableWidget->setItem(nR, nC++, pTypeItem);  // 在第nR行第nC列插入数据
		ui.tableWidget->setItem(nR++, nC, pCountItem); // 在第nR行第nC+1列插入数据
		nC = 0;
		qDebug().noquote() << QString::fromLocal8Bit("要素类型：") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit("，数量：") << entry.second << "\n";
	}
	return 0;
}
//统计面要素的面积
int AttributeWidget::calculateArea(GDALDataset* dataset) {
	//获取图层数量
	int layerCount = dataset->GetLayerCount();
	qDebug().noquote() << QString::fromLocal8Bit("图层数量: ") << layerCount << "\n";

	//定义一个map来存储要素类型和总面积
	std::map<OGRwkbGeometryType, double> areaSum;

	//设置行列数
	ui.tableWidget->setColumnCount(3);
	ui.tableWidget->setRowCount(10000);
	//设置表头
	QStringList headers;
	headers << QString::fromLocal8Bit("图层号") << QString::fromLocal8Bit("要素类型") << QString::fromLocal8Bit("面积");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//遍历每个图层
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("无法获取图层: ") << i << "\n";
			return -1;
		}

		//遍历图层中的每个要素
		layer->ResetReading();
		OGRFeature* feature = nullptr;


		while ((feature = layer->GetNextFeature()) != nullptr)
		{
			OGRGeometry* geometry = feature->GetGeometryRef();
			if (geometry != nullptr)
			{
				//计算要素面积
				double area = 0.0;
				OGRwkbGeometryType geomType = geometry->getGeometryType();
				if (wkbFlatten(geomType) == wkbPolygon || wkbFlatten(geomType) == wkbMultiPolygon)
				{
					area = geometry->toPolygon()->get_Area();
					QTableWidgetItem* pLayerItem = new QTableWidgetItem(QString("%1").arg(i + 1));
					ui.tableWidget->setItem(nR, nC++, pLayerItem); // 在第nR行第nC列插入数据
					QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(geomType));
					ui.tableWidget->setItem(nR, nC++, pTypeItem); // 在第nR行第nC+1列插入数据
					QTableWidgetItem* pAreaItem = new QTableWidgetItem(QString("%1").arg(area));
					ui.tableWidget->setItem(nR++, nC, pAreaItem); // 在第nR行第nC+2列插入数据
					nC = 0;
				}
				//累加要素类型的总面积
				areaSum[geomType] += area;
			}

			//释放要素
			OGRFeature::DestroyFeature(feature);
		}
	}

	//输出各类要素的总面积
	for (const auto& entry : areaSum)
	{
		QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
		QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
		ui.tableWidget->setItem(nR, ++nC, pTypeItem); // 在第nR行第nC列插入数据
		ui.tableWidget->setItem(nR, ++nC, pCountItem); // 在第nR行第nC+1列插入数据
		nR++;
		nC = 0;
		qDebug().noquote() << QString::fromLocal8Bit("要素类型: ") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit(", 总面积: ") << entry.second << "\n";
	}
	return 0;
}
//统计面要素的周长
int AttributeWidget::calculatePerimeter(GDALDataset* dataset) {
	//获取图层数量
	int layerCount = dataset->GetLayerCount();
	qDebug().noquote() << QString::fromLocal8Bit("图层数量: ") << layerCount << endl;

	//定义一个map来存储要素类型和总周长
	std::map<OGRwkbGeometryType, double> perimeterSum;

	//设置行列数
	ui.tableWidget->setColumnCount(3);
	ui.tableWidget->setRowCount(10000);
	//设置表头
	QStringList headers;
	headers << QString::fromLocal8Bit("图层号") << QString::fromLocal8Bit("要素类型") << QString::fromLocal8Bit("周长");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//遍历每个图层
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("无法获取图层: ") << i << endl;
			return -1;
		}

		//遍历图层中的每个要素
		layer->ResetReading();
		OGRFeature* feature = nullptr;

		while ((feature = layer->GetNextFeature()) != nullptr)
		{
			OGRGeometry* geometry = feature->GetGeometryRef();
			if (geometry != nullptr)
			{
				//计算要素周长
				double perimeter = 0.0;
				OGRwkbGeometryType geomType = geometry->getGeometryType();
				if (wkbFlatten(geomType) == wkbPolygon)				//面
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
				else if (wkbFlatten(geomType) == wkbMultiPolygon)	//多面
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

				//插入表格
				QTableWidgetItem* pLayerItem = new QTableWidgetItem(QString("%1").arg(i + 1));
				QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(geomType));
				QTableWidgetItem* pPerimeterItem = new QTableWidgetItem(QString("%1").arg(perimeter));
				ui.tableWidget->setItem(nR, nC++, pLayerItem);		// 在第nR行第nC列插入图层号
				ui.tableWidget->setItem(nR, nC++, pTypeItem);		// 在第nR行第nC+1列插入要素类型
				ui.tableWidget->setItem(nR, nC++, pPerimeterItem);  // 在第nR行第nC+2列插入要素周长
				nR++;
				nC = 0;
				//累加要素类型的总周长
				perimeterSum[geomType] += perimeter;

			}
			//释放要素
			OGRFeature::DestroyFeature(feature);
		}
		//输出各类要素的总周长
		for (const auto& entry : perimeterSum)
		{
			QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
			QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
			ui.tableWidget->setItem(nR, ++nC, pTypeItem);		// 在第nR行第nC列插入数据
			ui.tableWidget->setItem(nR++, ++nC, pCountItem);	// 在第nR行第nC+1列插入数据
			nC = 0;
			qDebug().noquote() << QString::fromLocal8Bit("要素类型: ") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit(", 总周长: ") << entry.second << "\n";
		}
	}
	return 0;
}
//统计线要素长度
int AttributeWidget::calculateLength(GDALDataset* dataset) {
	//获取图层数量
	int layerCount = dataset->GetLayerCount();

	//定义一个map来存储要素类型和总长度
	std::map<OGRwkbGeometryType, double> lengthSum;

	//设置行列数
	ui.tableWidget->setColumnCount(3);
	ui.tableWidget->setRowCount(20000);
	//设置表头
	QStringList headers;
	headers << QString::fromLocal8Bit("图层号") << QString::fromLocal8Bit("要素类型") << QString::fromLocal8Bit("长度");
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	int nR = 0, nC = 0;

	//遍历每个图层
	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* layer = dataset->GetLayer(i);
		if (layer == nullptr)
		{
			qDebug().noquote() << QString::fromLocal8Bit("无法获取图层: ") << i << endl;
			return -1;
		}

		//遍历图层中的每个要素
		layer->ResetReading();
		OGRFeature* feature = nullptr;
		while ((feature = layer->GetNextFeature()) != nullptr)
		{
			OGRGeometry* geometry = feature->GetGeometryRef();
			if (geometry != nullptr)
			{
				//计算要素长度
				double length = 0.0;
				OGRwkbGeometryType geomType = geometry->getGeometryType();
				if (wkbFlatten(geomType) == wkbLineString)			 //线
				{
					OGRLineString* lineString = dynamic_cast<OGRLineString*>(geometry);
					if (lineString != nullptr)
					{
						length = lineString->get_Length();
					}
				}
				else if (wkbFlatten(geomType) == wkbMultiLineString) //多线
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

				//插入表格
				QTableWidgetItem* pLayerItem = new QTableWidgetItem(QString("%1").arg(i + 1));
				QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(geomType));
				QTableWidgetItem* pLengthItem = new QTableWidgetItem(QString("%1").arg(length));
				ui.tableWidget->setItem(nR, nC++, pLayerItem);      // 在第nR行第nC列插入图层号
				ui.tableWidget->setItem(nR, nC++, pTypeItem);		// 在第nR行第nC+1列插入要素类型
				ui.tableWidget->setItem(nR++, nC++, pLengthItem);   // 在第nR行第nC+2列插入要素周长
				nC = 0;
				//累加要素类型的总周长
				lengthSum[geomType] += length;
			}
			//释放要素
			OGRFeature::DestroyFeature(feature);
		}
	}

	//输出各类要素数量
	if (lengthSum.empty()) {
		qDebug().noquote() << QString::fromLocal8Bit("没有线要素!") << endl;
		return -1;
	}
	for (const auto& entry : lengthSum)
	{
		QTableWidgetItem* pTypeItem = new QTableWidgetItem(OGRGeometryTypeToName(entry.first));
		QTableWidgetItem* pCountItem = new QTableWidgetItem(QString("%1").arg(entry.second));
		ui.tableWidget->setItem(nR, ++nC, pTypeItem);  //在第nR行第nC列插入数据
		ui.tableWidget->setItem(nR++, ++nC, pCountItem); //在第nR行第nC+1列插入数据
		nC = 0;
		qDebug().noquote() << QString::fromLocal8Bit("要素类型：") << OGRGeometryTypeToName(entry.first) << QString::fromLocal8Bit("，总长度：") << entry.second << endl;
	}
	return 0;
}