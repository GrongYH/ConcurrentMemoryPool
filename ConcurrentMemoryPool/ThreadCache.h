#pragma once
#include"Common.h"


// ThreadCache��ÿ���̶߳��еģ�����С��256KB�ڴ�ķ��䡣
// ���߳���ҪС�ڵ���256KB���ڴ�ʱ����ֱ�Ӵ�ThreadCache���롣


class ThreadCache
{
public:
	//�����ڴ�
	void* Allocate(size_t size);

	//�ͷ��ڴ�
	void Deallocate(void* ptr, size_t size);

	//�����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);

	//�ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeList[NFREELISTS];
};

static _declspec(thread) ThreadCache* tls_threadcache = nullptr;