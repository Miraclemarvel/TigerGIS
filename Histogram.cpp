#include "Histogram.h"
#include <QPainter>

Histogram::Histogram(logger* pLog, const std::vector<int>&hist , const std::vector<int>& eqHist, QWidget *parent)
	: QDialog(parent), mvHistogram(hist), mvEqualizedHistogram(eqHist)
{
	ui.setupUi(this);

	int margin = 20;  // �߾࣬���ڻ���������ͱ�ǩ
	int width = (ui.histogramLabel->width() - 3 * margin) / 2; // �̶����
	int height = ui.histogramLabel->height() - 2 * margin;     // �̶��߶�
	
	QPixmap pixmap(width * 2 + 3 * margin, height + 2 * margin);
	pixmap.fill(Qt::white);  // ��䱳��Ϊ��ɫ

	QPainter painter(&pixmap);
	painter.setPen(Qt::black);

	int maxCount = (((*std::max_element(mvHistogram.begin(), mvHistogram.end())) > (*std::max_element(mvEqualizedHistogram.begin(), mvEqualizedHistogram.end()))) ? (*std::max_element(mvHistogram.begin(), mvHistogram.end())) : (*std::max_element(mvEqualizedHistogram.begin(), mvEqualizedHistogram.end())));

	// ����ԭʼֱ��ͼ
	for (int i = 0; i < mvHistogram.size(); ++i) {
		int barHeight = static_cast<int>(mvHistogram[i] * height / static_cast<double>(maxCount));
		painter.drawRect(margin + i, height + margin - barHeight, 1, barHeight);
	}

	// ���ƾ��⻯ֱ��ͼ
	for (int i = 0; i < mvEqualizedHistogram.size(); ++i) {
		int barHeight = static_cast<int>(mvEqualizedHistogram[i] * height / static_cast<double>(maxCount));
		painter.drawRect(2 * margin + width + i, height + margin - barHeight, 1, barHeight);
	}

	// ����X���ǩ
	for (int i = 0; i <= 255; i += 51) {  // ÿ51����λһ����ǩ
		painter.drawText(margin + i, height + margin + 10, QString::number(i));
		painter.drawText(2 * margin + width + i, height + margin + 10, QString::number(i));
	}

	// ����Y���ǩ
	painter.drawLine(margin, 0, margin, height+ margin);
	painter.drawLine(2 * margin + width, 0, 2 * margin + width, height + margin);
	for (int i = 0; i <= maxCount; i += maxCount / 5) {  // ÿmaxCount / 5����λһ����ǩ
		int y = height + margin - static_cast<int>(i * height / static_cast<double>(maxCount));
		painter.drawText(0, y, QString::number(i));
		painter.drawText(2 * margin + width - 10, y, QString::number(i));
	}

	ui.histogramLabel->setPixmap(pixmap);
	pLog->logInfo("�Ҷ�ֱ��ͼ�������");
}

Histogram::~Histogram()
{}