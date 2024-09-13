#pragma once
#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <QDialog>
#include "ui_Histogram.h"
#include "logger.h"
//灰度直方图
class Histogram : public QDialog
{
	Q_OBJECT

public:
	//构造函数
	Histogram(logger* pLog, const std::vector<int>& hist, const std::vector<int>& eqHist, QWidget *parent = nullptr);
	~Histogram();

private:
	Ui::HistogramClass ui;
	std::vector<int> mvHistogram;			//灰度直方图
	std::vector<int> mvEqualizedHistogram;	//均衡化灰度直方图
};
#endif // HISTOGRAM_H