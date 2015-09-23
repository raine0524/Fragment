#ifndef	NOTECOMPARE_H_
#define	NOTECOMPARE_H_
#include <stdio.h>
#include <map>

typedef enum
{
	SEND_NOTE_FILE = 0,
	RECV_NOTE_FILE,
} FILE_TYPE;

class FileParser
{
public:
	FileParser();
	~FileParser();

public:
	std::multimap<float, std::pair<unsigned char, unsigned char> >& parse(const char* pFileName, FILE_TYPE type);
	void Compare(FileParser& parser);

private:
	void CollectNoteSet(char* pFileBuffer, int nFileSize, FILE_TYPE type);

private:
	FILE* m_pFile;
	std::multimap<float, std::pair<unsigned char, unsigned char> > m_mulNoteSet;
};
#endif		//NOTECOMPARE_H_