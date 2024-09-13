#include "ZoomableGraphicsView.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QScrollBar>
#include <QPen>

ZoomableGraphicsView::ZoomableGraphicsView(QWidget* parent)
	: QGraphicsView(parent)
	, mPen(Qt::black)
	, mBrush(Qt::cyan)
	, mdScaleFactor(1.15)
	, mdCurrentScale(1.0)
	, mdMinScale(0.5)
	, mdMaxScale(5.0)
	, mbdragging(false)
	, mbIsEditing(false)
	, mpPolygonItem(nullptr)
	, mpStatusLabel(new QLabel(this))
{
	setRenderHint(QPainter::Antialiasing, true); //���������
	setMouseTracking(true);	//�������׷��
}

void ZoomableGraphicsView::setMinScale(double dScale) {
	mdMinScale = dScale;
}

void ZoomableGraphicsView::setMaxScale(double dScale) {
	mdMaxScale = dScale;
}
void ZoomableGraphicsView::setCurrentScale(double dScale) {
	mdCurrentScale = dScale;
}

void ZoomableGraphicsView::wheelEvent(QWheelEvent* event) {
	if (event->angleDelta().y() > 0) {
		// Zoom in
		if (mdCurrentScale < mdMaxScale) {
			scale(mdScaleFactor, mdScaleFactor);
			mdCurrentScale *= mdScaleFactor;
		}
	}
	else {
		// Zoom out
		if (mdCurrentScale > mdMinScale) {
			scale(1.0 / mdScaleFactor, 1.0 / mdScaleFactor);
			mdCurrentScale /= mdScaleFactor;
		}
	}

	emit scaleChanged();
}

void ZoomableGraphicsView::updateSceneScale() {
	QGraphicsScene* scene = this->scene();
	if (!scene) return;

	double currentScale = transform().m11();
	QList<QGraphicsItem*> items = scene->items();
	for (QGraphicsItem* item : items) {
		if (auto* ellipseItem = dynamic_cast<QGraphicsEllipseItem*>(item)) {
			QPen pen = ellipseItem->pen();
			pen.setWidthF(1.0 / currentScale); // ��̬�����������
			ellipseItem->setPen(pen);
		}
		else if (auto* lineItem = dynamic_cast<QGraphicsPathItem*>(item)) {
			QPen pen = lineItem->pen();
			pen.setWidthF(1.0 / currentScale);
			lineItem->setPen(pen);
		}
		else if (auto* polygonItem = dynamic_cast<QGraphicsPolygonItem*>(item)) {
			QPen pen = polygonItem->pen();
			pen.setWidthF(1.0 / currentScale);
			polygonItem->setPen(pen);
		}
	}
}

void ZoomableGraphicsView::mousePressEvent(QMouseEvent* event) {
	if (!mbIsEditing) {
		if (event->button() == Qt::LeftButton) {
			mbdragging = true;
			mLastMousePos = event->pos();
			setCursor(Qt::ClosedHandCursor);
		}
	}
	else { // ������ڱ༭״̬
		if (event->button() == Qt::LeftButton) { // ������µ������
			handleLeftClick(event->pos()); // ���ô����������ĺ���
		}
		else if (event->button() == Qt::RightButton) { // ������µ����Ҽ�
			handleRightClick(event->pos()); // ���ô����Ҽ�����ĺ���
		}
	}
	QGraphicsView::mousePressEvent(event);
}

void ZoomableGraphicsView::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		mbdragging = false;
		setCursor(Qt::ArrowCursor);
	}
	QGraphicsView::mouseReleaseEvent(event);
}

void ZoomableGraphicsView::mouseMoveEvent(QMouseEvent* event) {
	if (mbdragging) {
		QPoint delta = event->pos() - mLastMousePos;
		mLastMousePos = event->pos();
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
		verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
	}
	QGraphicsView::mouseMoveEvent(event);

	QPointF scenePos = this->mapToScene(event->pos());
	emit mouseMoved(scenePos);
}

void ZoomableGraphicsView::handleLeftClick(const QPoint& pos) {
	QPointF scenePos = this->mapToScene(pos); // ����ͼ����ת��Ϊ��������
	if (msCurrentFeatureType == "Point") { // �����ǰҪ�������ǵ�
		QGraphicsEllipseItem* point = new QGraphicsEllipseItem(scenePos.x(), scenePos.y(), 0.5, 0.5); // ����һ���µĵ�ͼ����
		mPen.setWidth(0.4);
		point->setPen(mPen);
		this->scene()->addItem(point); // ������ӵ�������
		mvTempItems.append(point); // ������ӵ���ʱͼ�����б���
	}
	else if (msCurrentFeatureType == "Line") { // �����ǰҪ����������
		if (mvTempPoints.isEmpty() || mvTempPoints.last() != scenePos) { // �����ʱ���б�Ϊ�ջ����һ���㲻�ǵ�ǰ��
			mvTempPoints.append(scenePos); // ����ǰ����ӵ���ʱ���б���
			if (mvTempPoints.size() > 1) { // �����ʱ���б����ж����
				QGraphicsLineItem* line = new QGraphicsLineItem(QLineF(mvTempPoints[mvTempPoints.size() - 2], scenePos)); // ����һ���µ���ͼ����
				line->setPen(mPen);
				this->scene()->addItem(line); // ������ӵ�������
				mvTempItems.append(line); // ������ӵ���ʱͼ�����б���	
			}
		}
	}
	else if (msCurrentFeatureType == "Polygon") { // �����ǰҪ����������
		mvTempPoints.append(scenePos); // ����ǰ����ӵ���ʱ���б���
		if (mvTempPoints.size() > 1) { // ������Ҫ����������γ��߶�
			if (mpPolygonItem == nullptr) { // �����ǰû�����ڻ��ƵĶ����
				QPolygonF polygonPoints(mvTempPoints);
				mpPolygonItem = new QGraphicsPolygonItem(polygonPoints); // ����һ���µĶ����ͼ����
				mPen.setWidth(2); // ���ñʿ��
				mpPolygonItem->setPen(mPen); // ���ö���εı߿���ɫ����ʽ
				mpPolygonItem->setBrush(mBrush);
				//mpPolygonItem->setBrush(Qt::blue); // ���ö���������ɫ
				this->scene()->addItem(mpPolygonItem); // ���������ӵ�������
				mvTempItems.append(mpPolygonItem); // ���������ӵ���ʱͼ�����б���
			}
			else {
				QPolygonF polygonPoints(mvTempPoints);
				mpPolygonItem->setPolygon(polygonPoints); // ���¶���εĶ���
			}
		}
	}
}

void ZoomableGraphicsView::handleRightClick(const QPoint& pos) {
	if (msCurrentFeatureType == "Line" || msCurrentFeatureType == "Polygon") { // �����ǰҪ���������߻���
		if (mpPolygonItem != nullptr) { // ����������ڻ��ƵĶ����
			if (mvTempPoints.size() > 2) { // ������Ҫ����������γ�һ����Ч�Ķ����
				QPolygonF polygonPoints(mvTempPoints);
				mpPolygonItem->setPolygon(polygonPoints); // ���¶���εĶ���
			}
			else {
				this->scene()->removeItem(mpPolygonItem); // �Ƴ���Ч�Ķ����
				delete mpPolygonItem; // ɾ�������ͼ����
			}
			mpPolygonItem = nullptr; // ���ö����ͼ����
		}
		mvTempPoints.clear(); // �����ʱ���б�
	}
}