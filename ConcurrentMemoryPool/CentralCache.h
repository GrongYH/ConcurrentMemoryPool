#pragma once
#include"Common.h"

//ȫ��ֻ��һ��CentralCache����˿���ʹ�õ���ģʽ
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	//�����Ļ����ȡһ�������Ķ����threadCache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	//��SpanList����PageCache�л�ȡһ��span
	Span* GetOneSpan(SpanList& spanList, size_t size);

	//��һ���������ڴ���ͷŵ�Span��
	void ReleaseListToSpans(void* start, size_t size);

private:
	CentralCache() = default;
	CentralCache& operator=(const CentralCache&) = delete;
	CentralCache(const CentralCache&) = delete;
	static CentralCache _sInst;
private:
	SpanList _spanList[NFREELISTS];
};