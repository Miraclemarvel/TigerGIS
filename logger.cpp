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
	// ��ȡ��Category
	mpRoot = &log4cpp::Category::getRoot().getInstance("TigerGIS");
	// ������־�����ʽ
	mpFileLayout->setConversionPattern("%d: %p %c%x: %m%n");	
	// �����ļ����������Appender
	mpFileAppender = new log4cpp::FileAppender("fileAppender", "./log/tigerGIS.log");
	mpStreamAppender = new log4cpp::OstreamAppender("streamAppender", &mLogStream);
	// �����־��ʽ
	mpFileAppender->setLayout(mpFileLayout);
	mpStreamAppender->setLayout(mpFileLayout);
	// ��Appender�����ڸ�Category
	mpRoot->addAppender(mpFileAppender);
	mpRoot->addAppender(mpStreamAppender);
	// ֮ǰ����һ�����˵ģ������ػ�û���������� �������� ���ھ�ȷ�㲻 ע���˻���ô�� �������е�ʱ����ʾ��仰 ���ð�
	mpRoot->info("��ӭʹ��TigerGIS");
	QString capLog;
	auto info = mLogStream.str();
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	mLogStream.str("");  // �����־��
	mLogStream.clear();  // ���ô���״̬
}
// Analysis log
logger::logger(const char* sInfo, QPlainTextEdit* pLoggerEdit)
	: mpFileLayout(new log4cpp::PatternLayout())
	, mpLoggerEdit(pLoggerEdit)
	, mpFileAppender(nullptr)
	, mpStreamAppender(nullptr)
	, mpRoot(nullptr)
{
	// ��ȡ��Category
	mpRoot = &log4cpp::Category::getRoot().getInstance("Analysis");
	// ������־�����ʽ
	mpFileLayout->setConversionPattern("%d: %p %c%x: %m%n");
	// �����ļ����������Appender
	mpFileAppender = new log4cpp::FileAppender("fileAppender", "./log/tigerGIS.log");
	mpStreamAppender = new log4cpp::OstreamAppender("streamAppender", &mLogStream);
	// �����־��ʽ
	mpFileAppender->setLayout(mpFileLayout);
	mpStreamAppender->setLayout(mpFileLayout);
	// ��Appender�����ڸ�Category
	mpRoot->addAppender(mpFileAppender);
	mpRoot->addAppender(mpStreamAppender);
	// �������ȼ�
	mpRoot->info(sInfo);
	QString capLog;
	auto info = mLogStream.str();
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	mLogStream.str("");  // �����־��
	mLogStream.clear();  // ���ô���״̬
}

logger::~logger() {}

// �����־
// һ��������Ϣ
void logger::logInfo(const char* sInfo) {
	mpRoot->info(sInfo);
	//������־��Ϣ�����Qt�ؼ�
	QString capLog;
	auto info = mLogStream.str();
	// ȥ��ĩβ�Ļ��з�
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	// ������ƶ����ı�ĩβ���Զ����������·�
	mpLoggerEdit->moveCursor(QTextCursor::End);
	QCoreApplication::processEvents();  // ǿ�ƴ����¼����������� UI
	mLogStream.str("");  // �����־��
	mLogStream.clear();  // ���ô���״̬
}
// ������Ϣ
void logger::logErrorInfo(const char* sInfo) {
	mpRoot->error(sInfo);
	//������־��Ϣ�����Qt�ؼ�
	QString capLog;
	auto info = mLogStream.str();
	// ȥ��ĩβ�Ļ��з�
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	// ������ƶ����ı�ĩβ���Զ����������·�
	mpLoggerEdit->moveCursor(QTextCursor::End);
	QCoreApplication::processEvents();  // ǿ�ƴ����¼����������� UI
	mLogStream.str("");  // �����־��
	mLogStream.clear();  // ���ô���״̬
}
// ������Ϣ
void logger::logWarnInfo(const char* sInfo) {
	mpRoot->warn(sInfo);
	//������־��Ϣ�����Qt�ؼ�
	QString capLog;
	auto info = mLogStream.str();
	// ȥ��ĩβ�Ļ��з�
	if (!info.empty() && info.back() == '\n') {
		info.pop_back();
	}
	capLog = QString::fromLocal8Bit(info.c_str());
	mpLoggerEdit->appendPlainText(capLog);
	// ������ƶ����ı�ĩβ���Զ����������·�
	mpLoggerEdit->moveCursor(QTextCursor::End);
	QCoreApplication::processEvents();  // ǿ�ƴ����¼����������� UI
	mLogStream.str("");  // �����־��
	mLogStream.clear();  // ���ô���״̬
}