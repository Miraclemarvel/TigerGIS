#pragma once
#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <QDialog>
#include "ui_Histogram.h"
#include "logger.h"
//�Ҷ�ֱ��ͼ
class Histogram : public QDialog
{
	Q_OBJECT

public:
	//���캯��
	Histogram(logger* pLog, const std::vector<int>& hist, const std::vector<int>& eqHist, QWidget *parent = nullptr);
	~Histogram();

private:
	Ui::HistogramClass ui;
	std::vector<int> mvHistogram;			//�Ҷ�ֱ��ͼ
	std::vector<int> mvEqualizedHistogram;	//���⻯�Ҷ�ֱ��ͼ
};
#endif // HISTOGRAM_H