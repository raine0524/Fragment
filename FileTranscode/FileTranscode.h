#ifndef	FILETRANSCODE_H_
#define	FILETRANSCODE_H_
#include <errno.h>
#include <utility>
#include <cctype>
#include <algorithm>
#ifndef WIN32
	#include <sys/wait.h>
#endif

#define	SHELL_BUF_LEN	65536
#define	CHECK_TRANS	1000		//1s
#define	FILENAME_GLUE		"Stub.mp4"

//it->first: videoid, it->second: videoname
typedef std::pair<std::string, std::string> MEDIA_PAIR;

typedef enum {
	TRANS_NEED = 1,
	TRANS_DOING,
	TRANS_OVER,
	TRANS_EXCEPTION
} TRANS_STATUS;

class TranscodeChecker;
class FileTransNotify
{
public:
	virtual void NotifyFileNameChanged(const std::string& strVideoID, const std::string& strVideoPath, const std::string& strPlayPath) = 0;
	virtual void NotifyTranscodeProgress(const std::string& strVideoID, int nTranStatus, int nTransProg) = 0;
	//nFileSize: file size, strRes: resolution of video & format such as 1920X1080, duration: last time of video
	virtual void NotifyTranscodeOver(const std::string& strVideoID, int nFileSize, const std::string& strRes, int duration) = 0;
};

class FileTranscode : public KThread
{
public:
	FileTranscode(FileTransNotify& rNotify);
	virtual ~FileTranscode() {}

public:
	void PushBufferPool(const MEDIA_PAIR& pair);
	void ReportProgress();
	void Stop();

private:
	void ThreadProcMain();
	void ProcessBufferPool();
	void ClearBufferPool();
	void CreateTimer();
	void DestroyTimer();
	int RunShellCmd(char* cmdstring);
	int CalculateTime(char* pch);
	void GetInfoFromShell(std::string& strAudioFmt, std::string& strVideoFmt, int& duration);
	void OnTranscodeFile();	

private:
	bool m_bWantToStop;
	TranscodeChecker* m_pTransChecker;
	char* m_pShellBuf;
	int m_nTotal, m_duration;		//the unit of duration is second
	FileTransNotify& m_rNotify;
	MEDIA_PAIR m_mediaPair;
	BUFFER_QUEUE m_lstBufferPool;
	KCritSec m_secBufferPool, m_secShellBuf;
};

class TranscodeChecker : public KTimer
{
public:
	TranscodeChecker(FileTranscode& rCallback) : m_rCallback(rCallback) {}
	virtual void OnTimerEvent(unsigned int nEventID);

private:
	FileTranscode& m_rCallback;
};
#endif		//FILETRANSCODE_H_