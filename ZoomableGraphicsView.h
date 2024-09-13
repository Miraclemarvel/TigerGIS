#pragma once
#ifndef ZOOMABLEGRAPHICSVIEW_H
#define ZOOMABLEGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QLabel>

class ZoomableGraphicsView : public QGraphicsView{
	Q_OBJECT

public:
	ZoomableGraphicsView(QWidget* parent = nullptr);

	QPen mPen;                              //轮廓笔
	QBrush mBrush;                          //填充笔
	void setMinScale(double dScale);		//设置最小比例
	void setMaxScale(double dScale);		//设置最大比例
	void setCurrentScale(double dScale);	//设置当前比例
	//编辑要素
	bool mbIsEditing;						  //编辑状态标志
	QString msCurrentLayer;                   //当前正在编辑的图层
	QString msCurrentFeatureType;             //当前图层的要素类型
	QVector<QGraphicsItem*> mvTempItems;      //临时存储当前绘制的图形项
	QVector<QPointF> mvTempPoints;            //临时存储当前绘制的点
	QLabel* mpStatusLabel;					  //鼠标坐标信息
	QGraphicsPolygonItem* mpPolygonItem;	  //当前绘制的图像
	QVector<QGraphicsEllipseItem*> mvUndoPointVector; //撤销的点要素
	QVector<QGraphicsLineItem*> mvUndoLineVector; //撤销的线要素
	QVector<QGraphicsPolygonItem*> mvUndoPolygonVector; //撤销的面要素


protected:
	void wheelEvent(QWheelEvent* event) override;		//重载滚轮事件
	void mousePressEvent(QMouseEvent* event) override;	//重载鼠标按压事件
	void mouseReleaseEvent(QMouseEvent* event) override;//重载鼠标松开事件
	void mouseMoveEvent(QMouseEvent* event) override;	//重载鼠标移动事件
	//编辑要素
	void handleLeftClick(const QPoint& pos);  //处理左键键点击事件的函数
	void handleRightClick(const QPoint& pos); //处理右键点击事件的函数

private:
	double mdScaleFactor;		//缩放系数
	double mdCurrentScale;		//当前缩放比例
	double mdMinScale;			//最小缩放比例
	double mdMaxScale;			//最大缩放比例
	bool   mbdragging;			//鼠标拖拽
	QPoint mLastMousePos;
	

public:
	void updateSceneScale();

signals:
	void scaleChanged(); // 信号，用于通知缩放变化
	void mouseMoved(const QPointF& pos);
};

#endif // ZOOMABLEGRAPHICSVIEW_H