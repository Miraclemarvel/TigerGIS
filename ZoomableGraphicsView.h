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

	QPen mPen;                              //������
	QBrush mBrush;                          //����
	void setMinScale(double dScale);		//������С����
	void setMaxScale(double dScale);		//����������
	void setCurrentScale(double dScale);	//���õ�ǰ����
	//�༭Ҫ��
	bool mbIsEditing;						  //�༭״̬��־
	QString msCurrentLayer;                   //��ǰ���ڱ༭��ͼ��
	QString msCurrentFeatureType;             //��ǰͼ���Ҫ������
	QVector<QGraphicsItem*> mvTempItems;      //��ʱ�洢��ǰ���Ƶ�ͼ����
	QVector<QPointF> mvTempPoints;            //��ʱ�洢��ǰ���Ƶĵ�
	QLabel* mpStatusLabel;					  //���������Ϣ
	QGraphicsPolygonItem* mpPolygonItem;	  //��ǰ���Ƶ�ͼ��
	QVector<QGraphicsEllipseItem*> mvUndoPointVector; //�����ĵ�Ҫ��
	QVector<QGraphicsLineItem*> mvUndoLineVector; //��������Ҫ��
	QVector<QGraphicsPolygonItem*> mvUndoPolygonVector; //��������Ҫ��


protected:
	void wheelEvent(QWheelEvent* event) override;		//���ع����¼�
	void mousePressEvent(QMouseEvent* event) override;	//������갴ѹ�¼�
	void mouseReleaseEvent(QMouseEvent* event) override;//��������ɿ��¼�
	void mouseMoveEvent(QMouseEvent* event) override;	//��������ƶ��¼�
	//�༭Ҫ��
	void handleLeftClick(const QPoint& pos);  //�������������¼��ĺ���
	void handleRightClick(const QPoint& pos); //�����Ҽ�����¼��ĺ���

private:
	double mdScaleFactor;		//����ϵ��
	double mdCurrentScale;		//��ǰ���ű���
	double mdMinScale;			//��С���ű���
	double mdMaxScale;			//������ű���
	bool   mbdragging;			//�����ק
	QPoint mLastMousePos;
	

public:
	void updateSceneScale();

signals:
	void scaleChanged(); // �źţ�����֪ͨ���ű仯
	void mouseMoved(const QPointF& pos);
};

#endif // ZOOMABLEGRAPHICSVIEW_H