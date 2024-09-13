#pragma once
#ifndef TIGERGIS_H
#define TIGERGIS_H

#include <QtWidgets/QMainWindow>
#include "ui_TigerGIS.h"
#include "ZoomableGraphicsView.h"
#include "AttributeWidget.h"
#include "DataInput.h"
#include "Histogram.h"
#include "NeighborhoodStatistics.h"
#include "MaskExtract.h"
#include "logger.h"
#include <QStandardItemModel>
#include <QVector>
#include <QMap>


class TigerGIS : public QMainWindow
{
    Q_OBJECT

public:
    TigerGIS(QWidget *parent = nullptr);
    ~TigerGIS();

public slots:
	void resetView();							 //��λ
	void selectAllColor();						 //��������ͼ������ɫ������ɫ
	void selectColor(QString sLayerName);		 //����ѡ��ͼ������ɫ������ɫ
	void updateStatusBar(const QPointF& pos);	 //����״̬����ʾ�������
	void on_actionStartEditing_triggered();      //��ʼ�༭
	void on_actionUndo_triggered();				 //����
	void on_actionRedo_triggered();				 //����
	void on_actionStopEditing_triggered();       //ֹͣ�༭
	void on_actionShpFile_triggered();			 //��ʸ���ļ�
	void on_actionRasterFile_triggered();		 //��դ���ļ�
	void on_actionbigRaster_triggered();	     //�򿪴�������դ���ļ�
	void on_actionsaveProject_triggered();		 //���湤���ļ�
	void on_actionreadProject_triggered();		 //���빤���ļ�
	void on_actionCalculateNum_triggered();		 //ͳ������
	void on_actionCalculateArea_triggered();	 //ͳ�����
	void on_actionCalculatePerimeter_triggered();//ͳ���ܳ�
	void on_actionCalculateLength_triggered();	 //ͳ�Ƴ���
	void on_actionCalculateBuffer_triggered();	 //����������
	void on_actionCalculateConvex_triggered();	 //͹������
	void on_actionIntersect_triggered();		 //���ӷ���--����
	void on_actionUnion_triggered();		     //���ӷ���--����
	void on_actionErase_triggered();			 //���ӷ���--����
	void on_actiondrawGreyHistogram_triggered(); //���ƻҶ�ֱ��ͼ
	void on_actionextractByMask_triggered();     //��Ĥ��ȡ
	void on_actiondomainAnalysis_triggered();	 //�������
	void on_actionHelp_triggered();				 //����

protected:
	int drawShp(QString sFileName);				//����ʸ��
	int drawRaster(QString sFileName, int nRedBand = 1, int nGreenBand = 2, int nBlueBand = 3); //����դ��
	int drawGeometry(OGRGeometry* poGeometry);	//����wkt�ļ�
	int addRasterContent(QString sFileName);	//���դ�������������б�
	int addShpContent(QString sFileName);		//ʸ��Ҫ���������б�
	int drawLayer(QVector<QGraphicsItem*> layer);	//����ͼ��
	int removeLayer(QVector<QGraphicsItem*> layer);	//�Ƴ�ͼ��
	QVector<QString> getCheckedItem();			//����Ŀ¼���Ĺ�ѡ�ڵ�
	void saveChanges();							//������ĵĺ���
	void discardChanges();						//�������ĵĺ���

private:
	// ������
    Ui::TigerGISClass ui;
	QGraphicsScene* mpScene;                //����
	QStandardItemModel* mpFileList;         //�����б�
	QStandardItem* mpRootNode;              //�����б�ĸ��ڵ�
	// ����
	AttributeWidget* mpAttrWidget;          //������ʾ�˵�
	DataInput* mpInput;                     //��������˵�
	Histogram* mpHist;						//���ƻҶ�ֱ��ͼ�Ľ���
	NeighborhoodStatistics* mpNeighbor;		//��������˵�
	MaskExtract* mpMask;					//��Ĥ��ȡ�˵�
	// ����֮�䴫�ݵ���Ϣ
	GDALDataset* mpoDataset;                //GDAL���ݼ�
	QMap<QString, QVector<QGraphicsItem*>> mLayerMap;  //ʸ��ͼ�㼯��
	QMap<QString, GDALDataset*> mDatasetMap;//�򿪵����ݼ�����
	QVector<QGraphicsItem*> mvEditItems;	//�༭��Ҫ�ؼ�
	logger* mpLog;							//��־
};

#endif //TIGERGIS_H