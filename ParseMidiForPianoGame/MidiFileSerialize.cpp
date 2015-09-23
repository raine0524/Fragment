#include <math.h>
#include <string.h>
#include <Windows.h>
#include "MidiFileSerialize.h"

#define R_BUF(buf, len)	\
{	\
	if (static_cast<int>(len+buf_index) > nBufLen) return 0;	\
	memcpy(buf, buffer+buf_index, len);		\
	buf_index += len;		\
}

int MidiFileSerialize::GetFileSize(const char* pFileName)
{
	if (!pFileName)
		return -1;

	int nFileSize;
	WIN32_FIND_DATA stFileInfo;
	HANDLE hFile = FindFirstFile(pFileName, &stFileInfo);
	if (INVALID_HANDLE_VALUE == hFile) {
		nFileSize = -2;
	} else {
		nFileSize =  stFileInfo.nFileSizeLow;
		FindClose(hFile);
	}
	return nFileSize;
}

int MidiFileSerialize::ReadMidiFile(const char* pFileName, int nFileSize, unsigned char* pMidiFileBuffer)
{
	if (!pFileName || nFileSize <= 0 || !pMidiFileBuffer)
		return -1;

	FILE* pFile = fopen(pFileName, "rb");
	if (!pFile)
		return -2;

	int szRead = fread(pMidiFileBuffer, 1, nFileSize, pFile);
	if (szRead != nFileSize)
		return -3;

	fclose(pFile);
	return 0;
}

MidiFile* MidiFileSerialize::loadFromFile(const char* file)
{
	if (!file)
		return NULL;

	int nFileSize = GetFileSize(file);
	if (nFileSize <= 0)
		return NULL;

	unsigned char* pMidiFileBuffer = new unsigned char[nFileSize];
	if (!pMidiFileBuffer) {
		return NULL;
	} else {
		int sts = ReadMidiFile(file, nFileSize, pMidiFileBuffer);
		if (sts)
		{
			delete []pMidiFileBuffer;
			return NULL;
		}
	}

	buf_index = 0;
	if (midi_)
	{
		delete midi_;
		midi_ = NULL;
	}

	MidiFile* midi = load(pMidiFileBuffer, nFileSize);
	delete []pMidiFileBuffer;
	return midi;
}

// int <-> word
unsigned int MidiFileSerialize::create_midi_int(unsigned char* p)
{
	unsigned int i, num = 0;
	const unsigned int SIZE = 4;
	for (i = 0; i < SIZE; i++)
		num = (num << 8)+*(p+i);
	return num;
}

//unsigned char a[2] -> word
unsigned short MidiFileSerialize::create_word(unsigned char* p)
{
	unsigned short num = 0;
	num = (*p)<<8;
	num += *(p+1);
	return num;
}

bool MidiFileSerialize::readTrackData(ITrack* track, unsigned char* buffer, int nBufLen)
{
	unsigned int len = 0;
	Chunk chunk;

	R_BUF(&chunk, sizeof(chunk));
	if (0 != memcmp(chunk.id_, "MTrk", 4))
		return false;

	len = create_midi_int(chunk.size_);
	if (static_cast<int>(len+buf_index) > nBufLen)
		return false;

	unsigned char* buff = buffer+buf_index;
	buf_index += len;
	if (!parseMidiEvent(buff, len, track))
		return false;
	return true;
}

bool MidiFileSerialize::parseMidiEvent(unsigned char* data, int size, ITrack* track)
{
	unsigned int tick = 0;
	unsigned char* p, *buff;
	unsigned char pre_ctrl = 0;

	p = data;
	buff = p;

	while (p-buff < size)
	{
		int t = 0, offset;
		unsigned char ch;

		offset = parseDeltaTime(p, &t);
		if (-1 == offset)
			return false;

		tick += t;
		p += offset;
		ch = *p;
		if (0xFF == ch)		//meta event ������ʾ�� track ���ơ���ʡ���ʾ���
			offset = parseMetaEvent(p, tick, track);
		else if (0xF0 == ch || 0xF7 == ch)		//sys exclusive event ϵͳ�߼���Ϣ
			offset = parseSystemExclusiveEvent(p, tick);
		else		//channel event
			offset = parseChannelEvent(p, &pre_ctrl, tick, track);
		p += offset;
	}
	return true;
}

int MidiFileSerialize::parseDeltaTime(unsigned char* p, int* time)
{
	unsigned int i, j;
	unsigned char ch;
	unsigned int MAX = 5;

	for (i = 0; i < MAX; i++)
	{
		ch = *(p+i);
		if (!(ch & 0x80))
			break;
	}

	if (i != MAX)
	{
		*time = 0;
		for (j = 0; j < i+1; j++)
		{
			ch = *(p+j);
			*time = ((*time) << 7)+(ch & 0x7F);
		}
		return i+1;
	}
	return -1;
}

int MidiFileSerialize::parseMetaEvent(unsigned char* p, int tick, ITrack* track)
{
	char ch = 0;
	int len = 0, ret = 0;

	p++;		//0xFF
	ch = *p++;

	ret = parseDeltaTime(p, &len);
	if (-1 == ret)
		return false;
	p += ret;

	switch(ch)
	{
	case 0x00:		//FF 00 02 ss ss: �����
		{
			break;
		}
	case 0x01:		//�ı��¼�������ע�� track ���ı�
		{
			TextEvent event;
			event.tick = tick;
			event.text = std::string(p, p+len);
			track->texts.push_back(event);
			break;
		}
	case 0x02:		//��Ȩ������ ������ƶ�����ʽ��(C) 1850 J.Strauss��
		{
			midi_->copyright = std::string(p, p+len);
			break;
		}
	case 0x03:		// ����� track �����ơ�
		{
			track->name = std::string(p, p+len);
			break;
		}
	case 0x04:		//��������
		{
			track->instrument = std::string(p, p+len);
			break;
		}
	case 0x05:		//���
		{
			TextEvent event;
			event.tick = tick;
			event.text = std::string(p, p+len);
			track->lyrics.push_back(event);
			break;
		}
	case 0x06:		//��ǣ��磺��ʫƪ1����
		{
			TextEvent event;
			event.tick = tick;
			event.text = std::string(p, p+len);
			midi_->markers.push_back(event);
			break;
		}
	case 0x07:		//��ʾ�� ������ʾ��̨�Ϸ��������顣�磺��Ļ�����𡱡����˳���̨�󡱵ȡ�
		{
			TextEvent event;
			event.tick = tick;
			event.text = std::string(p, p+len);
			midi_->cuePoints.push_back(event);
			break;
		}
	case 0x2f:		//Track ����
		{
			break;
		}
	case 0x51:		//����:1/4�������ٶȣ���΢���ʾ�����û��ָ����ȱʡ���ٶ�Ϊ 120��/�֡�����൱�� tttttt = 500,000��
		{
			TempoEvent event;
			event.tick = tick;
			event.tempo = 0;

			int tempo = *p++;
			event.tempo |= tempo << 16;
			tempo = *p++;
			event.tempo |= tempo << 8;
			tempo = *p;
			event.tempo |= tempo;
			midi_->tempos.push_back(event);
			break;
		}
	case 0x58:		//���ӼǺ�: �磺 6/8 �� nn=6��dd=3 (2^3)��ʾ��
		{
			TimeSignatureEvent event;
			event.tick = tick;
			event.numerator = *p++;					//����
			event.denominator = *p++;				//��ĸ��ʾΪ 2 �ģ�dd�Σ�ڤ
			event.number_ticks = *p++;				//ÿ�� MIDI ʱ�ӽ������� tick ��Ŀ
			event.number_32nd_notes = *p;		//24��MIDIʱ����1/32��������Ŀ��8�Ǳ�׼�ģ�
			event.denominator = (int)pow((float)2, event.denominator);
			midi_->timeSignatures.push_back(event);
			break;
		}
	case 0x59:		//��������:0 ��ʾ C ����������ʾ����������������ʾ����������
		{
			KeySignatureEvent event;
			event.tick = tick;
			event.sf = *p++;		//�����򽵵�ֵ  -7 = 7 ����,  0 =  C ��,  +7 = 7 ����
			event.mi = *p;			//0 = ���, 1 = С��
			midi_->keySignatures.push_back(event);
			break;
		}
	case 0x7f:		//����������  Meta-event
		{
			SpecificInfoEvent event;
			event.tick = tick;
			event.infos = std::vector<unsigned char>(p, p+len);
			track->specificEvents.push_back(event);
			break;
		}
	default:
		{
			break;
		}
	}
	return len+ret+2;
}

//ϵͳ�߼���Ϣ
int MidiFileSerialize::parseSystemExclusiveEvent(unsigned char* p, int tick)
{
	int offset, len = offset = 0;
	SysExclusiveEvent sys_event;
	sys_event.tick = tick;
	sys_event.event.push_back(*p++);
	offset = parseDeltaTime(p, &len);
	p += offset;
	for (int i = 0; i < len; i++)
		sys_event.event.push_back(*p++);
	midi_->sysExclusives.push_back(sys_event);
	return len+offset+1;
}

int MidiFileSerialize::parseChannelEvent(unsigned char* p, unsigned char* pre_ctrl, int tick, ITrack* track)
{
	int len = 0;
	unsigned char ch = 0;
	unsigned int temp = 0;

	Event event;
	event.tick = tick;
	ch = *p;
	if (ch & 0x80) {
		temp = *p++;
		*pre_ctrl = ch;
		len++;
	} else {
		temp = *pre_ctrl;
		ch = *pre_ctrl;
	}
	event.evt = ch;
	ch &= 0xF0;

	switch(ch)
	{
	case 0x80:		//�����ر� (�ͷż���) ����:00~7F ����:00~7F
	case 0x90:		//������ (���¼���) ����:00~7F ����:00~7F
	case 0xa0:		//���������Ժ�  ����:00~7F ����:00~7F
	case 0xb0:		//������  ����������:00~7F ����������:00~7F
	case 0xe0:		//���� ����(Pitch)��λ:Pitch mod 128  ���߸�λ:Pitch div 128
		{
			len += 2;
			temp = *p++;
			event.nn = temp;
			temp = *p;
			event.vv = temp;
			break;
		}
	case 0xc0:		//�л���ɫ�� ��������:00~7F
	case 0xd0:		//ͨ������ѹ�����ɽ�����Ϊ�������� ֵ:00~7F
		{
			len += 1;
			temp = *p;
			event.nn = temp;
			break;
		}
	default:
		{
			break;
		}
	}
	track->events.push_back(event);
	return len;
}

void MidiFileSerialize::parseHeadInfo(ITrack* track)
{
	if (!midi_)
		return;
	midi_->name = track->name;
	if (!track->texts.empty())
		midi_->author = track->texts.front().text;
}

MidiFile* MidiFileSerialize::load(unsigned char* buffer, int nBufLen)
{
	bool ret = false;
	int fmt;
	unsigned int track_cnt;
	unsigned char word[2];
	Chunk chunk;

	midi_ = new MidiFile();
	buf_index = 0;

	do {
		R_BUF(&chunk, sizeof(chunk));
		//printf("%c%c%c%c, %02x %02x %02x %02x\n", chunk.id_[0], chunk.id_[1], chunk.id_[2], chunk.id_[3], chunk.size_[0], chunk.size_[1], chunk.size_[2], chunk.size_[3]);

		if (memcmp(chunk.id_, "MThd", 4) != 0) {
			if (create_midi_int(chunk.size_) != 0)
				break;
		}

		R_BUF(word, 2);		//format
		fmt = create_word(word);
		midi_->format = fmt;
		//printf("format=%d, fmt=%d\n", midi_->format, fmt);

		R_BUF(word, 2);		//track count
		track_cnt = create_word(word);
		
		R_BUF(word, 2);		//delta time
		midi_->quarter = create_word(word);

		if (0 == fmt) {
			ITrack track;
			if (!readTrackData(&track, buffer, nBufLen))
				break;
			parseHeadInfo(&track);
			midi_->tracks.push_back(track);
			ret = true;
		} else if (1 == fmt) {
			unsigned int i;
			for (i = 0; i < track_cnt; i++)
			{
				ITrack track;
				if (!readTrackData(&track, buffer, nBufLen))
					break;

				if (0 != i || !track.events.empty())
					midi_->tracks.push_back(track);
			}
			ret = true;
		}
	} while(0);

	if (ret)
		return midi_;
	else
		return NULL;
}

int main(int argc, char* argv[])
{
	std::string strDir = ".\\MidiDir\\";
	MidiFileSerialize MidiParser;

	WIN32_FIND_DATA FindData;
	HANDLE hFindFile = FindFirstFile(std::string(strDir+"*.*").c_str(), &FindData);
	if (INVALID_HANDLE_VALUE == hFindFile)
	{
		printf("get midi file failed\n");
		return EXIT_FAILURE;
	}

	char tmp_buf[64];
	while (FindNextFile(hFindFile, &FindData))
	{
		if (!strcmp(FindData.cFileName, ".") || !strcmp(FindData.cFileName, ".."))
			continue;

		std::string strFile = strDir+FindData.cFileName;
		if (strFile.substr(strFile.rfind(".")) != ".mid")
			continue;
		MidiFile* midi = MidiParser.loadFromFile(strFile.c_str());
		strFile.replace(strFile.rfind("."), strlen(".mid"), ".txt");

		FILE* text = fopen(strFile.c_str(), "w");
		sprintf(tmp_buf, "%d %d/%d %d %d\n", 60*1000*1000/midi->tempos.front().tempo, midi->timeSignatures.front().numerator, midi->timeSignatures.front().denominator, midi->quarter, midi->tempos.front().tempo);
		fwrite(tmp_buf, 1, strlen(tmp_buf), text);

		std::list<ITrack>::iterator track = midi->tracks.begin();
		while (track->events.empty())
			track++;
		for (std::list<Event>::iterator event = track->events.begin(); event != track->events.end(); event++)
		{
			if (0x90 != static_cast<int>(event->evt & 0xF0) || (0x90 == static_cast<int>(event->evt & 0xF0) && !event->vv))
				continue;

			bool found = false;
			std::list<Event>::iterator it = event;
			while (++it != track->events.end()) {
				if ((it->nn == event->nn) && (0x80 == static_cast<int>(it->evt &0xF0) || (0x90 == static_cast<int>(it->evt & 0xF0) && !it->vv))) {
					found = true;
					break;
				}
			}
			if (found) {
				sprintf(tmp_buf, "%d:%d:%d\n", event->nn, event->tick, it->tick-event->tick);
				fwrite(tmp_buf, 1, strlen(tmp_buf), text);
			} else {
				printf("midi file get error\n");
			}
		}
		fclose(text);
	}
	system("pause");
	return EXIT_SUCCESS;
}