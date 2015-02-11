#include "stdafx.h"
#include "FileTranscode.h"

void TranscodeChecker::OnTimerEvent(unsigned int nEventID)
{
	if (CHECK_TRANS == nEventID)
		m_rCallback.ReportProgress();
}

FileTranscode::FileTranscode(FileTransNotify& rNotify)
:m_bWantToStop(false)
,m_pTransChecker(NULL)
,m_pShellBuf(new char[SHELL_BUF_LEN])
,m_rNotify(rNotify)
{
}

void FileTranscode::PushBufferPool(const MEDIA_PAIR& pair)
{
	KAutoLock l(m_secBufferPool);
	MEDIA_PAIR* pPair = new MEDIA_PAIR(pair);
	m_lstBufferPool.push(pPair);
}

void FileTranscode::Stop()
{
	m_bWantToStop = true;
	WaitForStop();
	ClearBufferPool();
	DestroyTimer();
	if (m_pShellBuf)
	{
		delete[] m_pShellBuf;
		m_pShellBuf = NULL;
	}
}

void FileTranscode::ClearBufferPool()
{
	KAutoLock l(m_secBufferPool);
	while (!m_lstBufferPool.empty())
	{
		MEDIA_PAIR* pPair = (MEDIA_PAIR*)m_lstBufferPool.front();
		m_lstBufferPool.pop();
		if (pPair)
		{
			delete pPair;
			pPair = NULL;
		}
	}
}

void FileTranscode::ProcessBufferPool()
{
	MEDIA_PAIR* pPair = NULL;
	{
		KAutoLock l(m_secBufferPool);
		if (m_lstBufferPool.empty())
			return;

		pPair = (MEDIA_PAIR*)m_lstBufferPool.front();
		m_lstBufferPool.pop();
	}

	if (pPair)
	{
		m_mediaPair = *pPair;
		delete pPair;
		pPair = NULL;
		LOG::INF("File transcoding, process file with videoid=%s, videoname=%s\n", m_mediaPair.first.c_str(), m_mediaPair.second.c_str());
		OnTranscodeFile();
	}
}

void FileTranscode::ThreadProcMain()
{
	while (!m_bWantToStop)
	{
		ProcessBufferPool();
		Pause(10);
	}
}

void FileTranscode::CreateTimer()
{
	if (!m_pTransChecker)
	{
		m_pTransChecker = new TranscodeChecker(*this);
		m_pTransChecker->SetTimerEvent(CHECK_TRANS, CHECK_TRANS);
		m_pTransChecker->StartTimer();
	}
}

void FileTranscode::DestroyTimer()
{
	if (m_pTransChecker)
	{
		m_pTransChecker->StopTimer();
		delete m_pTransChecker;
		m_pTransChecker = NULL;
	}
}

void FileTranscode::ReportProgress()
{
	int trans_duration = 0;
	std::string strShellBuf, strEigenValue = "time=";
	//LOG::INF("Reporting progress...First copy the output buffer and clear obsolete data\n");
	{
		KAutoLock l(m_secShellBuf);
		strShellBuf = std::string(m_pShellBuf, m_nTotal);
		memset(m_pShellBuf, 0, m_nTotal);
		m_nTotal = 0;
	}
	size_t pos = strShellBuf.rfind("bitrate=");
	if (std::string::npos != pos)		//find successfully
		strShellBuf.erase(strShellBuf.begin()+pos, strShellBuf.end());

	pos = strShellBuf.rfind(strEigenValue);
	if (std::string::npos != pos)
	{
		pos += strEigenValue.size();
		trans_duration += STR2INT(strShellBuf.substr(pos, 2))*60*60;
		pos += 3;	//skip an extra ':' character
		trans_duration += STR2INT(strShellBuf.substr(pos, 2))*60;
		pos += 3;
		trans_duration += STR2INT(strShellBuf.substr(pos, 2));
		int progress = static_cast<int>(1.0f*trans_duration/m_duration*100);
		//LOG::INF("Transcoding... ration=%d\%\n", progress);
		m_rNotify.NotifyTranscodeProgress(m_mediaPair.first, TRANS_DOING, progress);
	}
}

void FileTranscode::OnTranscodeFile()
{
#ifndef WIN32
	LOG::INF("OnTranscodeFile begin.\r\n");
	std::string strRawFile = m_mediaPair.second;
	if (-1 == access(strRawFile.c_str(), 0))
	{
		LOG::ERR("[ERR] ProcessFile file(%s) not find .\r\n",strRawFile.c_str());
		return;
	}

	char buf[256] = {0};
	size_t pos = strRawFile.rfind(".");

	//get media information
	std::string strAudioFmt = "", strVideoFmt = "";
	m_duration = 0;
	sprintf(buf, "ffmpeg -i %s 2>&1", strRawFile.c_str());
	RunShellCmd(buf);		//get the info of temp file
	GetInfoFromShell(strAudioFmt, strVideoFmt, m_duration);
	LOG::INF("Get the raw info: audio format=%s, video format=%s, duration=%d\n", strAudioFmt.c_str(), strVideoFmt.c_str(), m_duration);

	//combine different commands according to the different compressed format
	std::string strFinalFile = strRawFile.substr(0, pos)+".mp4";
	if ("h264" == strVideoFmt && "aac" == strAudioFmt)
	{
		if (strRawFile != strFinalFile)
		{
			sprintf(buf, "ffmpeg -i %s -vcodec copy -acodec copy %s 1>/dev/null 2>/dev/null", strRawFile.c_str(), strFinalFile.c_str());
			system(buf);
			sprintf(buf, "rm -f %s", strRawFile.c_str());
			system(buf);			
		}
		m_rNotify.NotifyTranscodeProgress(m_mediaPair.first, TRANS_OVER, 100);
	}
	else
	{
		std::string strTempFile = strRawFile.substr(0, pos)+FILENAME_GLUE;
		sprintf(buf, "ffmpeg -y -i %s ", strRawFile.c_str());
		if ("h264" == strVideoFmt)
			sprintf(buf+strlen(buf), "-vcodec copy ");
		else if ("" != strVideoFmt)
			sprintf(buf+strlen(buf), "-vcodec libx264 -vprofile baseline -g 10 ");
		if ("aac" == strAudioFmt)
			sprintf(buf+strlen(buf), "-acodec copy ");
		else if ("" != strAudioFmt)
			sprintf(buf+strlen(buf), "-acodec libfaac -ar 44100 -ac 1 ");
		sprintf(buf+strlen(buf), "%s 2>&1", strTempFile.c_str());
		LOG::INF("Construct the transcode command: %s\n", buf);

		//run shell command and get progress in the timer thread
		if ("h264" != strVideoFmt)
			CreateTimer();
		RunShellCmd(buf);
		if ("h264" != strVideoFmt)
			DestroyTimer();

		sprintf(buf, "mv -f %s %s", strTempFile.c_str(), strFinalFile.c_str());
		system(buf);
	}

	if (strRawFile != strFinalFile)
		m_rNotify.NotifyFileNameChanged(m_mediaPair.first, strFinalFile, strFinalFile.substr(strFinalFile.rfind("/")+1, std::string::npos));

	//notify transcode over
	int nFileSize = 0;
	std::string strRes;
	sprintf(buf, "ffmpeg -i %s 2>&1 | grep 'Duration: ' | cut -d ' ' -f 4 | sed s/,//", strFinalFile.c_str());
	RunShellCmd(buf);		//get media duration
	m_duration = CalculateTime(m_pShellBuf);
	nFileSize = get_file_len(strFinalFile.c_str());
	sprintf(buf, "ffmpeg -i %s 2>&1 | grep -Eo '[1-9][0-9]{0,3}x[1-9][0-9]{0,3}'", strFinalFile.c_str());
	RunShellCmd(buf);		// get media resolution
	strRes = m_pShellBuf;
	transform(strRes.begin(), strRes.end(), strRes.begin(), ::toupper);
	m_rNotify.NotifyTranscodeOver(m_mediaPair.first, nFileSize, strRes, m_duration);
	LOG::INF("Transcode/Copy over, FileSize=%d, Resolution=%s, duration=%d(s)\n", nFileSize, strRes.c_str(), m_duration);
	LOG::INF("OnTranscodeFile end.\r\n");
#endif
}

/*
 *@cmdstring: external program or script command
 *@m_pShellBuf: buffer for the shell output
 *@m_nTotal: valid bytes in buffer, this value is less than ${SHELL_BUF_LEN}
 */
int FileTranscode::RunShellCmd(char* cmdstring)
{
#ifndef WIN32
	int fd[2];
	if (pipe(fd) < 0)
		return -1;

	pid_t pid = fork();
	if (pid < 0)
	{
		printf("error in fork!\n");
		return -2;
	}
	else if (0 == pid)		//child process
	{
		close(fd[0]);		//Close read end
		//printf("[Child process]Ready to duplicate the STDOUT_FILENO to the write end of pipe\n");
		if (fd[1] != STDOUT_FILENO)
		{
			//copy file pointer and overwrite STDOUT_FILENO's pointer
			if (dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO)
				return -3;
			close(fd[1]);
		}
		execl("/bin/sh", "sh", "-c", cmdstring, (char*)0);
	}
	else		/*parent process*/
	{
		m_nTotal = 0;
		memset(m_pShellBuf, 0, SHELL_BUF_LEN);
		int nCnt;
		char tmpBuf[256];
		close(fd[1]);		//Close write end
		//printf("[Parent process]reading...\n");
		//obvious this operation will block current thread!
		while ((nCnt = read(fd[0], tmpBuf, 256)) != 0)	//return 0 means another end is closed!
		{
			if (nCnt < 0)		//error!
			{
				printf("[Parent process]read error, errno: %d->%s\n", errno, strerror(errno));
				waitpid(pid, NULL, 0);
				return -5;
			}
			else
			{
				//maybe blocked here, wait for processor
				while (m_nTotal+nCnt >= SHELL_BUF_LEN)
					Pause(1);
				{
					KAutoLock l(m_secShellBuf);
					memcpy(m_pShellBuf+m_nTotal, tmpBuf, nCnt);
					m_nTotal += nCnt;
				}
			}
		}

		close(fd[0]);
		waitpid(pid, NULL, 0);
	}
#endif
	return 0;
}

void FileTranscode::GetInfoFromShell(std::string& strAudioFmt, std::string& strVideoFmt, int& duration)
{
	std::string strEigenValue;
	char* pch = NULL;

	//get audio format
	strEigenValue = "Audio: ";
	pch = strstr(m_pShellBuf, strEigenValue.c_str());
	if (pch)
	{
		pch += strEigenValue.size();
		strAudioFmt = std::string(pch, 3);	//"aac" occupy 3 bytes
	}	

	//get video format
	strEigenValue = "Video: ";
	pch = strstr(m_pShellBuf, strEigenValue.c_str());
	if (pch)
	{
		pch += strEigenValue.size();
		strVideoFmt = std::string(pch, 4);	//"h264" occupy 4 bytes
	}

	//get duration
	strEigenValue = "Duration: ";
	pch = strstr(m_pShellBuf, strEigenValue.c_str());
	if (pch)
	{
		pch += strEigenValue.size();
		duration = CalculateTime(pch);
	}
}

//Convert time string with format "hh:mm:ss" to an integer in seconds
int FileTranscode::CalculateTime(char* pch)		//pch point to the hour
{
	int time = atoi(pch)*60*60;	//convert hour to second
	pch		+= 3;		//pch point to the minute
	time	+= atoi(pch)*60;		//convert minute to second
	pch		+= 3;		//pch point to the second
	time	+= atoi(pch);
	return time;
}