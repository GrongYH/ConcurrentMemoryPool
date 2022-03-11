#include"PageCache.h"
PageCache PageCache::_cInst;


Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (PageID)obj >> PAGE_SHIFT;
	std::unique_lock<std::mutex> lock(_mtx);
	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end())
	{
		return ret->second;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	//����128 page��ֱ���������
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();
		span->_n = k;
		span->_pageId = ((PageID)ptr >> PAGE_SHIFT);
		span->_objSize = k >> PAGE_SHIFT;
		_idSpanMap[span->_pageId] = span;

		return span;
	}

	if (!_spanList[k].IsEmpty())
	{
		Span* span = _spanList[k].PopFront();
		for (PageID i = 0; i < span->_n; i++)
		{
			_idSpanMap[span->_pageId + i] = span;
		}
		return span;
	}

	for (int i = k + 1; i < NPAGES; i++)
	{
		if (!_spanList[i].IsEmpty())
		{
			Span* nSpan = _spanList[i].PopFront();
			Span* kSpan = _spanPool.New();
			kSpan->_n = k;
			kSpan->_pageId = nSpan->_pageId;

			nSpan->_n -= k;
			nSpan->_pageId += k;

			_spanList[nSpan->_n].PushFront(nSpan);

			//�洢nSpan����βҳ����nspan��ӳ�䣬��������ϲ�ʱ�Ĳ���
			_idSpanMap[nSpan->_pageId] = nSpan;
			_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;

			//��kSpand��ÿһ��ҳ��ҳ����kSpan����ӳ��
			for (PageID i = 0; i < kSpan->_n; i++)
			{
				_idSpanMap[kSpan->_pageId + i] = kSpan;
			}


			return kSpan;
		}
	}
	//���ִ�е����˵��Ŀǰpagecache��Ҳû��span����Ҫ���ں�����
	//������һ��NPAGES��С��span���ҵ�NPAGES���ٽ����з�

	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PageID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanList[bigSpan->_n].PushFront(bigSpan);

	return NewSpan(k);

}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//����128ҳ��pageֱ�ӻ�����
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		_spanPool.Delete(span);
		return;
	}
	//��ǰ���ҳ���кϲ�
	while (true)
	{
		PageID prevId = span->_pageId - 1;
		auto ret = _idSpanMap.find(prevId);
		//ǰ���ҳ��û���ˣ����ϲ���
		//������Ҫע����ǣ���Ϊÿ�η���spanʱ��������ӳ�䣬���Բ���Ҫ����ӳ�䵽�ɵ�span
		if (ret == _idSpanMap.end())
		{
			break;
		}
		//ǰ���span��ʹ�ã�
		if (ret->second->_isUsed == true)
		{
			break;
		}
		//����ϲ�֮�����128ҳ���Ͳ��ϲ�
		if (ret->second->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		span->_pageId = ret->second->_pageId;
		span->_n += ret->second->_n;
		_spanList[ret->second->_n].Erease(ret->second);
		_spanPool.Delete(ret->second);
	}
	while (true)
	{
		PageID nextId = span->_pageId + span->_n;
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end())
		{
			break;
		}
		//�����span��ʹ�ã�
		Span* span1 = ret->second;
		if (ret->second->_isUsed == true)
		{
			break;
		}
		//����ϲ�֮�����128ҳ���Ͳ��ϲ�
		if (ret->second->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		span->_n += ret->second->_n;
		_spanList[ret->second->_n].Erease(ret->second);
		_spanPool.Delete(ret->second);
	}

	_spanList[span->_n].PushFront(span);
	span->_isUsed = false;
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;
}