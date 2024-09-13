#include "logger.h"
#include <QDebug>
#include <QCoreApplication>
#include <QString>

// TigerGIS log
logger::logger(QPlainTextEdit* pLoggerEdit) 
	: mpFileLayout(new log4cpp::PatternLayout())
	, mpLoggerEdit(pLoggerEdit)
	, mpFileAppender(nullptr)
	, mpStreamAppender(nullptr)
	, mpRoot(nullptr)
{
	// 获取根Category
	mpRoot = &log4cpp::Category::getRoot().getInstance("TigerGIS");
	// 设置日志输出格式
	mpFileLayout->setConversionPattern("%d: %p %c%x: %m%n");	
	// 设置文件流和输出流Appender
	mpFileAppender = new log4cpp::FileAppender("fileAppender", "./log/tigerGIS.log");
	mpStreamAppender = new log4cpp::OstreamAppender("streamAppender", &mLogStream);
	// 添加日志格式
	mpFileAppender->setLayout(mpFileLayout);
	mpStreamAppender->setLayout(mpFileLayout);
	// 将Appender依附于根Category
	mpRoot->addAppender(mpFileAppender);
	mpRoot->addAppender(mpStreamAppender);
	// 之前在这一行闪退的，现在呢还没试那你试试 还是这行 能在精确点不 注释了会怎么样 程序运行的时候不显示这句话 能用吧
	mpRoot->info("欢迎使用TigerGIS");
	QString capLog;
	auto info = mLogStream.str();
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	mLogStream.str("");  // 清空日志流
	mLogStream.clear();  // 重置错误状态
}
// Analysis log
logger::logger(const char* sInfo, QPlainTextEdit* pLoggerEdit)
	: mpFileLayout(new log4cpp::PatternLayout())
	, mpLoggerEdit(pLoggerEdit)
	, mpFileAppender(nullptr)
	, mpStreamAppender(nullptr)
	, mpRoot(nullptr)
{
	// 获取根Category
	mpRoot = &log4cpp::Category::getRoot().getInstance("Analysis");
	// 设置日志输出格式
	mpFileLayout->setConversionPattern("%d: %p %c%x: %m%n");
	// 设置文件流和输出流Appender
	mpFileAppender = new log4cpp::FileAppender("fileAppender", "./log/tigerGIS.log");
	mpStreamAppender = new log4cpp::OstreamAppender("streamAppender", &mLogStream);
	// 添加日志格式
	mpFileAppender->setLayout(mpFileLayout);
	mpStreamAppender->setLayout(mpFileLayout);
	// 将Appender依附于根Category
	mpRoot->addAppender(mpFileAppender);
	mpRoot->addAppender(mpStreamAppender);
	// 设置优先级
	mpRoot->info(sInfo);
	QString capLog;
	auto info = mLogStream.str();
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	mLogStream.str("");  // 清空日志流
	mLogStream.clear();  // 重置错误状态
}

logger::~logger() {}

// 输出日志
// 一般运行信息
void logger::logInfo(const char* sInfo) {
	mpRoot->info(sInfo);
	//捕获日志信息输出到Qt控件
	QString capLog;
	auto info = mLogStream.str();
	// 去掉末尾的换行符
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	// 将光标移动到文本末尾，自动滚动到最下方
	mpLoggerEdit->moveCursor(QTextCursor::End);
	QCoreApplication::processEvents();  // 强制处理事件，立即更新 UI
	mLogStream.str("");  // 清空日志流
	mLogStream.clear();  // 重置错误状态
}
// 错误信息
void logger::logErrorInfo(const char* sInfo) {
	mpRoot->error(sInfo);
	//捕获日志信息输出到Qt控件
	QString capLog;
	auto info = mLogStream.str();
	// 去掉末尾的换行符
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	// 将光标移动到文本末尾，自动滚动到最下方
	mpLoggerEdit->moveCursor(QTextCursor::End);
	QCoreApplication::processEvents();  // 强制处理事件，立即更新 UI
	mLogStream.str("");  // 清空日志流
	mLogStream.clear();  // 重置错误状态
}
// 警告信息
void logger::logWarnInfo(const char* sInfo) {
	mpRoot->warn(sInfo);
	//捕获日志信息输出到Qt控件
	QString capLog;
	auto info = mLogStream.str();
	// 去掉末尾的换行符
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	// 将光标移动到文本末尾，自动滚动到最下方
	mpLoggerEdit->moveCursor(QTextCursor::End);
	QCoreApplication::processEvents();  // 强制处理事件，立即更新 UI
	mLogStream.str("");  // 清空日志流
	mLogStream.clear();  // 重置错误状态
}