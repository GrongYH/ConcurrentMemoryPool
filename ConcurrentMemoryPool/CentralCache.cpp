#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_sInst;
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	assert(batchNum > 0);
	size_t index = SizeClass::Index(size);
	_spanList[index]._mtx.lock();
	Span* span = GetOneSpan(_spanList[index], size);

	assert(span);
	assert(span->_freeList);

	start = span->_freeList;
	end = start;

	size_t actualNum = 1;
	while (Nextobj(end) != nullptr && actualNum < batchNum)
	{
		end = Nextobj(end);
		actualNum++;
	}

	span->_freeList = Nextobj(end);
	Nextobj(end) = nullptr;
	span->_useCount += actualNum;

	_spanList[index]._mtx.unlock();
	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& spanList, size_t size)
{
	Span* cur = spanList.Begin();
	while (cur != spanList.End())
	{
		if (cur->_freeList)
		{
			return cur;
		}
		else
		{
			cur = cur->_next;
		}
	}
	spanList._mtx.unlock();
	//����ߵ����˵��centralCache��û�п��õ�Span�����ThreadCache�������Ҫ��PageCache����
	PageCache::GetInstance()->_mtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUsed = true;
	span->_objSize = size;
	PageCache::GetInstance()->_mtx.unlock();

	//�õ�span���span�����з֣��ȼ������ڴ��ͷβָ��
	char* start = (char*)((span->_pageId) << PAGE_SHIFT);
	PageID bytes = (span->_n) << PAGE_SHIFT;
	char* end = start + bytes;

	//�Ѵ���ڴ��г���������
	//��������һ����ͷ������β��
	span->_freeList = start;
	void* tail = start;
	start += size;
	while (start < end)
	{
		Nextobj(tail) = start;
		tail = start;
		start += size;
	}
	Nextobj(tail) = nullptr;

	spanList._mtx.lock();
	spanList.PushFront(span);
	return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanList[index]._mtx.lock();
	while (start)
	{
		void* next = Nextobj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		Nextobj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		//��ʱ˵�����span�зֳ�ȥ������С�ڴ�鶼��������
		if (span->_useCount == 0)
		{
			_spanList[index].Erease(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			_spanList[index]._mtx.unlock();

			PageCache::GetInstance()->_mtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_mtx.unlock();

			_spanList[index]._mtx.lock();
		}
		start = next;
	}
	_spanList[index]._mtx.unlock();
}