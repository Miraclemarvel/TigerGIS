#include "Histogram.h"
#include <QPainter>

Histogram::Histogram(logger* pLog, const std::vector<int>&hist , const std::vector<int>& eqHist, QWidget *parent)
	: QDialog(parent), mvHistogram(hist), mvEqualizedHistogram(eqHist)
{
	ui.setupUi(this);

	int margin = 20;  // 边距，用于绘制坐标轴和标签
	int width = (ui.histogramLabel->width() - 3 * margin) / 2; // 固定宽度
	int height = ui.histogramLabel->height() - 2 * margin;     // 固定高度
	
	QPixmap pixmap(width * 2 + 3 * margin, height + 2 * margin);
	pixmap.fill(Qt::white);  // 填充背景为白色

	QPainter painter(&pixmap);
	painter.setPen(Qt::black);

	int maxCount = (((*std::max_element(mvHistogram.begin(), mvHistogram.end())) > (*std::max_element(mvEqualizedHistogram.begin(), mvEqualizedHistogram.end()))) ? (*std::max_element(mvHistogram.begin(), mvHistogram.end())) : (*std::max_element(mvEqualizedHistogram.begin(), mvEqualizedHistogram.end())));

	// 绘制原始直方图
	for (int i = 0; i < mvHistogram.size(); ++i) {
		int barHeight = static_cast<int>(mvHistogram[i] * height / static_cast<double>(maxCount));
		painter.drawRect(margin + i, height + margin - barHeight, 1, barHeight);
	}

	// 绘制均衡化直方图
	for (int i = 0; i < mvEqualizedHistogram.size(); ++i) {
		int barHeight = static_cast<int>(mvEqualizedHistogram[i] * height / static_cast<double>(maxCount));
		painter.drawRect(2 * margin + width + i, height + margin - barHeight, 1, barHeight);
	}

	// 绘制X轴标签
	for (int i = 0; i <= 255; i += 51) {  // 每51个单位一个标签
		painter.drawText(margin + i, height + margin + 10, QString::number(i));
		painter.drawText(2 * margin + width + i, height + margin + 10, QString::number(i));
	}

	// 绘制Y轴标签
	painter.drawLine(margin, 0, margin, height+ margin);
	painter.drawLine(2 * margin + width, 0, 2 * margin + width, height + margin);
	for (int i = 0; i <= maxCount; i += maxCount / 5) {  // 每maxCount / 5个单位一个标签
		int y = height + margin - static_cast<int>(i * height / static_cast<double>(maxCount));
		painter.drawText(0, y, QString::number(i));
		painter.drawText(2 * margin + width - 10, y, QString::number(i));
	}

	ui.histogramLabel->setPixmap(pixmap);
	pLog->logInfo("灰度直方图绘制完成");
}

Histogram::~Histogram()
{}