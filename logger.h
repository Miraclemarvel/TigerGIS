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
// �����־
	// ��¼һ����Ϣ
	void logInfo(const char* sInfo);
	// ��¼������Ϣ
	void logErrorInfo(const char* sInfo);
	// ��¼������Ϣ
	void logWarnInfo(const char* sInfo);

private:
	QPlainTextEdit* mpLoggerEdit;		 // Qt�����־�Ŀؼ�
	log4cpp::PatternLayout* mpFileLayout;// ��־��ʽ
	log4cpp::Appender* mpFileAppender;	 // �ļ���Appender
	std::ostringstream mLogStream;		 // ���������
	log4cpp::Appender* mpStreamAppender; // ���������Appender		
	log4cpp::Category* mpRoot;			 // ��Category
};

