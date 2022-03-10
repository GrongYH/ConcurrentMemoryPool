#pragma once
#include<iostream>
#include<vector>
#include<ctime>
#include<cassert>
#include<mutex>
#include<thread>
#include<algorithm>
#include<unordered_map>
#include <atomic>
using std::cout;
using std::endl;

//����һ��MAX_BYTES,���������ڴ��С��MAX_BYTES,�ʹ�thread_cache����
//����MAX_BYTES, ��ֱ�Ӵ�page_cache������
static const size_t MAX_BYTES = 256 * 1024;

//thread_cache��central cache�����������ϣͰ�ı��С
static const size_t NFREELISTS = 208;

//ҳ��Сת��ƫ������һҳΪ2^13K��Ҳ����8KB
static const size_t PAGE_SHIFT = 13;

//PageCache ��ϣ��Ĵ�С
static const size_t NPAGES = 129;

//ҳ������ͣ�32λ��4Byte��64λ��8byte
#ifdef _WIN64
	typedef unsigned long long PageID;
#elif _WIN32
	typedef size_t PageID;
#else
	//
#endif

#ifdef _WIN32
	#include<Windows.h>
	#undef min
#else
//��linux������ռ��ͷ�ļ�
#endif

//ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	//��linux������
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	//linux�µ��߼�
#endif
}

//��ȡ�ڴ�����д洢��ͷ4bit����8bitֵ����������һ������ĵ�ַ
static inline void*& Nextobj(void* obj)
{
	return *((void**)obj);
}

class FreeList
{
public:
	void Push(void* obj)
	{
		Nextobj(obj) = _head;
		_head = obj;
		++_size;
	}

	void* Pop()
	{
		void* obj = _head;
		_head = Nextobj(_head);
		--_size;
		return obj;
	}

	bool IsEmpty()
	{
		if (_head != nullptr)
		{
			int i = 0;
		}
		return _head == nullptr;
	}

	size_t& MaxSize()
	{
		return _max_size;
	}

	void PushRange(void* start, void* end, size_t rangeNum)
	{
		Nextobj(end) = _head;
		_head = start;
		_size += rangeNum;
	}

	void PopRange(void*& start, void*& end, size_t rangNum)
	{
		assert(rangNum <= _size);
		start = _head;
		end = start;
		for (size_t i = 0; i < rangNum - 1; i++)
		{
			end = Nextobj(end);
		}
		_head = Nextobj(end);
		Nextobj(end) = nullptr;
		_size -= rangNum;
	}

	size_t Size()
	{
		return _size;
	}

private:
	void* _head = nullptr;
	//һ��������������ڴ����������
	size_t _max_size = 1;
	//���������е��ڴ�������
	size_t _size = 0;
};


/********************************************************
 * ��������Ĺ�ϣͰλ�ú������ڴ���С��ӳ���ϵ       *
 * ThreadCache�ǹ�ϣͰ�ṹ��ÿ��Ͱ��Ӧ��һ����������  *
 * ÿ�����������У�����Ŀ�Ĵ�С�ǹ̶��ġ�             *
 * ÿ��Ͱ�ǰ��������������ڴ��Ĵ�С����Ӧӳ��λ�õġ� *
 * �����1B��256KBÿ���ֽڶ�����һ����ϣͰ���Ⲣ����ʵ��*
 * ��ˣ����Բ��ö������ķ�ʽ��                         *
 ********************************************************/

class SizeClass
{
public:
	// ������������10%���ҵ�����Ƭ�˷�
	// [1,128]                  8byte����        [0,16)�Ź�ϣͰ
	// [128+1,1024]             16byte����       [16,72)�Ź�ϣͰ
	// [1024+1,8*1024]          128byte����      [72,128)�Ź�ϣͰ
	// [8*1024+1,64*1024]       1024byte����     [128,184)�Ź�ϣͰ
	// [64*1024+1,256*1024]     8*1024byte����   [184,208)�Ź�ϣͰ

	//������Ҫ���ֽ����Ͷ�����������ʵ��������ڴ���С
	static inline size_t _RoundUp(size_t bytes, size_t align)
	{
		return (bytes + align - 1)&~(align - 1);
	}

	static inline size_t RoundUp(size_t bytes)
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);
		}
		else
		{
			return _RoundUp(bytes, 1 << PAGE_SHIFT);
		}
		return -1;
	}

	//align_shift�Ƕ�������ƫ���������������8KB��ƫ��������13
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		// ��� -1 ����˼������ȡ��, ��Ϊ��ϣͰ����Ǵ�0��ʼ��
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	//������������ڴ���С��Ѱ�����ĸ���������Ͱ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		//��һ������洢��ÿ�������ж��ٸ�Ͱ
		static int group[4] = { 16, 56, 56, 56 };
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 4) + group[0];
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes - 1024, 7) + group[0] + group[1];
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes - 8 * 1024, 10) + group[0] + group[1] + group[2];
		}
		else if(bytes <= 256 * 1024)
		{
			return _Index(bytes - 64 * 1024, 13) + group[0] + group[1] + group[2] + group[3];
		}
		else
		{
			assert(false);
		}

		return 0;
	}

	//һ�δ����Ļ����ȡ���ٸ�
	static size_t NumMovSize(size_t size)
	{
		if (size == 0)
			return 0;
		//�����һ�λ�õ��٣�С����һ�λ�õĶ�
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;

		return num;
	}

	static size_t NumMovePage(size_t size)
	{
		int num = NumMovSize(size);
		int npage = num * size;
		npage >>= PAGE_SHIFT;
		if (npage == 0)
			return 1;
		return npage;
	}
};

// PageCache��CentralCache��Ͱ�ҵĶ�����ҳΪ��λSpan����,Span�������Դ�ͷ˫��ѭ���������ʽ���ڵ�
class Span
{
public:
	//��ʼҳ��
	PageID _pageId = 0;
	//Span�����е�ҳ��
	size_t _n = 0;
	
	Span* _next = nullptr;
	Span* _prev = nullptr;

	//Span�ᱻ�зֳ�������������ThreadCacheֱ������Ӧ��Ͱ��ȡ���ڴ��
	void* _freeList = nullptr;

	//Ŀǰ��Span�����Ѿ������˶����ڴ���ThreadCache����_useCount == 0,˵��ThreadCache�Ѿ������е��ڴ�黹������
	size_t _useCount = 0;

	//�г����ĵ�������Ĵ�С��
	size_t _objSize = 0;

	//�Ƿ�ʹ��
	bool _isUsed = false;

};

class SpanList
{
public:
	SpanList() :_head(new Span)
	{
		_head->_next = _head;
		_head->_prev = _head;
	}

	void Insert(Span* cur, Span* newSpan)
	{
		assert(cur);
		Span* prev = cur->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = cur;
		cur->_prev = newSpan;
	}

	void Erease(Span* cur)
	{
		assert(cur);
		assert(cur != _head);

		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	bool IsEmpty()
	{
		return _head->_next == _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	Span* PopFront()
	{
		Span* span = _head->_next;
		Erease(span);
		return span;
	}

public:
	//CentralCache�ڷ���SpanListʱ��Ҫ����
	std::mutex _mtx;
private:
	Span* _head;
};

