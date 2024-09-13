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
	void resetView();							 //复位
	void selectAllColor();						 //设置所有图层的填充色与轮廓色
	void selectColor(QString sLayerName);		 //设置选定图层的填充色与轮廓色
	void updateStatusBar(const QPointF& pos);	 //更新状态栏显示鼠标坐标
	void on_actionStartEditing_triggered();      //开始编辑
	void on_actionUndo_triggered();				 //撤回
	void on_actionRedo_triggered();				 //重做
	void on_actionStopEditing_triggered();       //停止编辑
	void on_actionShpFile_triggered();			 //打开矢量文件
	void on_actionRasterFile_triggered();		 //打开栅格文件
	void on_actionbigRaster_triggered();	     //打开大数据量栅格文件
	void on_actionsaveProject_triggered();		 //保存工程文件
	void on_actionreadProject_triggered();		 //导入工程文件
	void on_actionCalculateNum_triggered();		 //统计数量
	void on_actionCalculateArea_triggered();	 //统计面积
	void on_actionCalculatePerimeter_triggered();//统计周长
	void on_actionCalculateLength_triggered();	 //统计长度
	void on_actionCalculateBuffer_triggered();	 //缓冲区分析
	void on_actionCalculateConvex_triggered();	 //凸包计算
	void on_actionIntersect_triggered();		 //叠加分析--交集
	void on_actionUnion_triggered();		     //叠加分析--并集
	void on_actionErase_triggered();			 //叠加分析--擦除
	void on_actiondrawGreyHistogram_triggered(); //绘制灰度直方图
	void on_actionextractByMask_triggered();     //掩膜提取
	void on_actiondomainAnalysis_triggered();	 //领域分析
	void on_actionHelp_triggered();				 //帮助

protected:
	int drawShp(QString sFileName);				//绘制矢量
	int drawRaster(QString sFileName, int nRedBand = 1, int nGreenBand = 2, int nBlueBand = 3); //绘制栅格
	int drawGeometry(OGRGeometry* poGeometry);	//绘制wkt文件
	int addRasterContent(QString sFileName);	//添加栅格数据至内容列表
	int addShpContent(QString sFileName);		//矢量要素至内容列表
	int drawLayer(QVector<QGraphicsItem*> layer);	//绘制图层
	int removeLayer(QVector<QGraphicsItem*> layer);	//移除图层
	QVector<QString> getCheckedItem();			//遍历目录树的勾选节点
	void saveChanges();							//保存更改的函数
	void discardChanges();						//丢弃更改的函数

private:
	// 主界面
    Ui::TigerGISClass ui;
	QGraphicsScene* mpScene;                //画布
	QStandardItemModel* mpFileList;         //内容列表
	QStandardItem* mpRootNode;              //内容列表的根节点
	// 界面
	AttributeWidget* mpAttrWidget;          //属性显示菜单
	DataInput* mpInput;                     //数据输入菜单
	Histogram* mpHist;						//绘制灰度直方图的界面
	NeighborhoodStatistics* mpNeighbor;		//领域分析菜单
	MaskExtract* mpMask;					//掩膜提取菜单
	// 界面之间传递的信息
	GDALDataset* mpoDataset;                //GDAL数据集
	QMap<QString, QVector<QGraphicsItem*>> mLayerMap;  //矢量图层集合
	QMap<QString, GDALDataset*> mDatasetMap;//打开的数据集集合
	QVector<QGraphicsItem*> mvEditItems;	//编辑的要素集
	logger* mpLog;							//日志
};

#endif //TIGERGIS_H