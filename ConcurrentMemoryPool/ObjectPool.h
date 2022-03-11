#pragma once
#include"Common.h"


template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		if (_freeList)
		{
			obj = (T*)_freeList;
			_freeList = *((void**)_freeList);
		}
		else
		{
			if (_leftBytes < sizeof(T))
			{
				_leftBytes = 128 * 1024;
				_memory = (char*)SystemAlloc (_leftBytes >> PAGE_SHIFT);
				//�������ʧ�ܣ����쳣
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			//������� sizeof(T)���Կ��С���з���
			//��ô32bit�£�С��4�ֽڵ��޷������������д���
			//64bit�£�С��4�ֽڵ��޷������������д���
			//�������Ҫ����ָ���С
			/*_memory += sizeof(T);
			_leftBytes -= sizeof(T);*/
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_leftBytes -= objSize;
		}
		new(obj) T;
		return obj;
	}

	void Delete(T* obj)
	{
		obj -> ~T();
		*((void**)obj) = _freeList;
		_freeList = obj;
	}
private:
	char* _memory = nullptr;
	//freeList ��ǰָ�����С��ŵ���һ����ĵ�ַ
	void* _freeList = nullptr;
	size_t _leftBytes = 0;
};
