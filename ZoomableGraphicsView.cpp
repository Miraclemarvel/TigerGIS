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
	setRenderHint(QPainter::Antialiasing, true); //开启抗锯齿
	setMouseTracking(true);	//开启鼠标追踪
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
			pen.setWidthF(1.0 / currentScale); // 动态调整线条宽度
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
	else { // 如果处于编辑状态
		if (event->button() == Qt::LeftButton) { // 如果按下的是左键
			handleLeftClick(event->pos()); // 调用处理左键点击的函数
		}
		else if (event->button() == Qt::RightButton) { // 如果按下的是右键
			handleRightClick(event->pos()); // 调用处理右键点击的函数
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
	QPointF scenePos = this->mapToScene(pos); // 将视图坐标转换为场景坐标
	if (msCurrentFeatureType == "Point") { // 如果当前要素类型是点
		QGraphicsEllipseItem* point = new QGraphicsEllipseItem(scenePos.x(), scenePos.y(), 0.5, 0.5); // 创建一个新的点图形项
		mPen.setWidth(0.4);
		point->setPen(mPen);
		this->scene()->addItem(point); // 将点添加到场景中
		mvTempItems.append(point); // 将点添加到临时图形项列表中
	}
	else if (msCurrentFeatureType == "Line") { // 如果当前要素类型是线
		if (mvTempPoints.isEmpty() || mvTempPoints.last() != scenePos) { // 如果临时点列表为空或最后一个点不是当前点
			mvTempPoints.append(scenePos); // 将当前点添加到临时点列表中
			if (mvTempPoints.size() > 1) { // 如果临时点列表中有多个点
				QGraphicsLineItem* line = new QGraphicsLineItem(QLineF(mvTempPoints[mvTempPoints.size() - 2], scenePos)); // 创建一个新的线图形项
				line->setPen(mPen);
				this->scene()->addItem(line); // 将线添加到场景中
				mvTempItems.append(line); // 将线添加到临时图形项列表中	
			}
		}
	}
	else if (msCurrentFeatureType == "Polygon") { // 如果当前要素类型是面
		mvTempPoints.append(scenePos); // 将当前点添加到临时点列表中
		if (mvTempPoints.size() > 1) { // 至少需要两个点才能形成线段
			if (mpPolygonItem == nullptr) { // 如果当前没有正在绘制的多边形
				QPolygonF polygonPoints(mvTempPoints);
				mpPolygonItem = new QGraphicsPolygonItem(polygonPoints); // 创建一个新的多边形图形项
				mPen.setWidth(2); // 设置笔宽度
				mpPolygonItem->setPen(mPen); // 设置多边形的边框颜色和样式
				mpPolygonItem->setBrush(mBrush);
				//mpPolygonItem->setBrush(Qt::blue); // 设置多边形填充颜色
				this->scene()->addItem(mpPolygonItem); // 将多边形添加到场景中
				mvTempItems.append(mpPolygonItem); // 将多边形添加到临时图形项列表中
			}
			else {
				QPolygonF polygonPoints(mvTempPoints);
				mpPolygonItem->setPolygon(polygonPoints); // 更新多边形的顶点
			}
		}
	}
}

void ZoomableGraphicsView::handleRightClick(const QPoint& pos) {
	if (msCurrentFeatureType == "Line" || msCurrentFeatureType == "Polygon") { // 如果当前要素类型是线或面
		if (mpPolygonItem != nullptr) { // 如果存在正在绘制的多边形
			if (mvTempPoints.size() > 2) { // 至少需要三个点才能形成一个有效的多边形
				QPolygonF polygonPoints(mvTempPoints);
				mpPolygonItem->setPolygon(polygonPoints); // 更新多边形的顶点
			}
			else {
				this->scene()->removeItem(mpPolygonItem); // 移除无效的多边形
				delete mpPolygonItem; // 删除多边形图形项
			}
			mpPolygonItem = nullptr; // 重置多边形图形项
		}
		mvTempPoints.clear(); // 清空临时点列表
	}
}