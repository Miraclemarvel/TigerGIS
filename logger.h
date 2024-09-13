#pragma once
#include <iostream>
#include <log4cpp/Category.hh>
#include <log4cpp/Appender.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/OstreamAppender.hh>
#include <QString>
#include <QPlainTextEdit>

class logger{
public:
	// TigerGIS log
	logger(QPlainTextEdit* pLoggerEdit);
	// DataInput log
	logger(const char* sInfo, QPlainTextEdit* pLoggerEdit);
	~logger();
// 输出日志
	// 记录一般信息
	void logInfo(const char* sInfo);
	// 记录错误信息
	void logErrorInfo(const char* sInfo);
	// 记录警告信息
	void logWarnInfo(const char* sInfo);

private:
	QPlainTextEdit* mpLoggerEdit;		 // Qt输出日志的控件
	log4cpp::PatternLayout* mpFileLayout;// 日志格式
	log4cpp::Appender* mpFileAppender;	 // 文件流Appender
	std::ostringstream mLogStream;		 // 输出流对象
	log4cpp::Appender* mpStreamAppender; // 输出流对象Appender		
	log4cpp::Category* mpRoot;			 // 根Category
};

