#include <Windows.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include "NoteCompare.h"

int GetFileSize(const char* pFileName)
{
	if (!pFileName)
		return -1;

	int nFileSize;
#ifdef _WIN32
	WIN32_FIND_DATA stFileInfo;
	HANDLE hFile = FindFirstFile(pFileName, &stFileInfo);
	if (INVALID_HANDLE_VALUE == hFile) {
		nFileSize = -2;
	} else {
		nFileSize =  stFileInfo.nFileSizeLow;
		FindClose(hFile);
	}
#else
	if (-1 == access(pFileName, 0)) {
		nFileSize = -3;
	} else {
		struct stat buf;
		if (-1 == stat(pFileName, &buf))
			nFileSize = -4;
		else
			nFileSize = buf.st_size;
	}
#endif
	return nFileSize;
}

FileParser::FileParser()
	:m_pFile(nullptr)
{
}

FileParser::~FileParser()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = nullptr;
	}
}

std::multimap<float, std::pair<unsigned char, unsigned char> >&
FileParser::parse(const char* pFileName, FILE_TYPE type)
{
	if (!m_mulNoteSet.empty())
		m_mulNoteSet.clear();

	int nFileSize = GetFileSize(pFileName);
	if (nFileSize <= 0)
		return m_mulNoteSet;

	char* pFileBuffer = new char[nFileSize+1];
	m_pFile = fopen(pFileName, "r");
	if (!m_pFile)
	{
		printf("open file %s failed!\n", pFileName);
		return m_mulNoteSet;
	}

	int szRead = fread(pFileBuffer, 1, nFileSize, m_pFile);
	pFileBuffer[szRead] = 0;
	CollectNoteSet(pFileBuffer, szRead, type);
	delete[] pFileBuffer;
	return m_mulNoteSet;
}

void FileParser::CollectNoteSet(char* pFileBuffer, int nFileSize, FILE_TYPE type)
{
	//skip the first line in spite of the file type
	char* pch = strchr(pFileBuffer, '\n');
	if (pch != nullptr)
		pch++;
	else
		return;

	char* pEnd = nullptr, bitset = 0;
	bool bParseSucc;
	while (pch && pch < pFileBuffer+nFileSize)
	{
		bitset = 0;
		bParseSucc = true;
		switch (type)
		{
		case SEND_NOTE_FILE:
			{
				//it->first: timestamp; it->second->first: key|0x80, it->second->second: note on-1/off-0
				std::pair<float, std::pair<unsigned char, unsigned char> > key;
				do {
					key.first = atof(pch);
					pch = strchr(pch, '(');
					if (pch != nullptr) {
						key.second.first = strtol(++pch, &pEnd, 16) & 0x7F ;
					} else {
						bParseSucc = false;
						break;
					}
					for (int i = 1; i <= 2; ++i)
					{
						pch = strchr(pch, '(');
						if (pch != nullptr) {
							if (!strtol(++pch, &pEnd, 16))
								bitset |= i;
						} else {
							bParseSucc = false;
							break;
						}
					}
					if (!bParseSucc)
						break;
					
					if (3 == bitset)
						key.second.second = 0;
					else
						key.second.second = 1;
					break;
				} while (true);

				if (bParseSucc) {
					m_mulNoteSet.insert(std::move(key));
					pch = strchr(pch, '\n');
					if (pch != nullptr)
						++pch;
				} else {
					//printf("send file structure is not correct\n");
				}
				break;
			}
		case RECV_NOTE_FILE:
			{
				std::pair<float, std::pair<unsigned char, unsigned char> > key;
				do {
					key.first = atof(pch);
					for (int i = 1; i <= 2; ++i)
					{
						pch = strchr(pch, '(');
						if (pch != nullptr) {
							if (i == 1)
								key.second.second = (0x90 == (strtol(++pch, &pEnd, 16) & 0xF0)) ? 1 : 0;
							else		//i == 2
								key.second.first = strtol(++pch, &pEnd, 16);
						} else {
							bParseSucc = false;
							break;
						}
					}
					break;
				} while (true);
				if (bParseSucc) {
					m_mulNoteSet.insert(std::move(key));
					for (int i = 1; i <= 2; ++i)
					{
						pch = strchr(pch, '\n');
						if (pch != nullptr)
							++pch;
						else
							break;
					}
				} else {
					//printf("recv file structure is not correct\n");
				}
				break;
			}
		}
	}
}

void FileParser::Compare(FileParser& parser)
{
	auto& peer_set = parser.m_mulNoteSet;
	auto note = m_mulNoteSet.begin();
	while (note != m_mulNoteSet.end())
	{
		bool bFindPeerNote = false;
		for (auto tmp_note = peer_set.begin(); tmp_note != peer_set.end(); tmp_note++) {
			if (note->second.first == tmp_note->second.first && note->second.second == tmp_note->second.second) {
				bFindPeerNote = true;
				peer_set.erase(tmp_note);
				break;
			}
		}
		if (bFindPeerNote)
			note = m_mulNoteSet.erase(note);
		else
			note++;
	}
	printf("\nThe compared result: \n¢Ùthe remainder notes in the send file: \ntime\tnote\tstatus\n");
	for (auto it = m_mulNoteSet.begin(); it != m_mulNoteSet.end(); ++it)
		printf("%f\t%X\t%s\n", it->first, it->second.first, (it->second.second == 1) ? "on" : "off");
	printf("\n¢Úthe remainder notes in the recv file: \ntime\tnote\tstatus\n");
	for (auto it = peer_set.begin(); it != peer_set.end(); ++it)
		printf("%f\t%X\t%s\n", it->first, it->second.first, (it->second.second == 1) ? "on" : "off");
}

int main(int argc, char* argv[])
{
	char buffer[256];
	FileParser send_parser, recv_parser;

	std::cout<<"Please enter the send file name: ";
	std::cin.getline(buffer, 256);
	auto send_note_set = send_parser.parse(buffer, SEND_NOTE_FILE);

	std::cout<<"Please enter the recv file name: ";
	std::cin.getline(buffer, 256);
	auto recv_note_set = recv_parser.parse(buffer, RECV_NOTE_FILE);
	send_parser.Compare(recv_parser);
	system("pause > nul");
}