#pragma once
#include"Common.h"
#include"ObjectPool.h"
#include"PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_cInst;
	}


	//��CentralCache kҳ��С��Span
	Span* NewSpan(size_t k);

	//��ȡ�ڴ���Span��ӳ��
	Span* MapObjectToSpan(void* obj);

	void ReleaseSpanToPageCache(Span* span);


public:
	std::mutex _mtx;
private:
	SpanList _spanList[NPAGES];
	ObjectPool<Span> _spanPool;
	//std::unordered_map<PageID, Span*> _idSpanMap;
	TcMalloc_PageMap2 <sizeof(void*)*8 - PAGE_SHIFT> _idSpanMap;
private:
	static PageCache _cInst;
	PageCache() = default;
	PageCache(const PageCache&) = delete;
};