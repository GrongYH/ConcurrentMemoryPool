#pragma once
#include"Common.h"
#include"ObjectPool.h"

//����һ������Ļ�������ԭ����ҳ������
//BITS��������ҳ��Ҫռ�õ�λ��
template<int BITS>
class TcMalloc_PageMap2
{
private:
	//�ڸ��з���32����Ŀ����ÿ��Ҷ�з��루2^ BITS��/ 32����Ŀ��
	static const int ROOT_BITS = 5;                 
	static const int ROOT_LENGTH = 1 << ROOT_BITS;  

	static const int LEAF_BITS = BITS - ROOT_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	struct Leaf {
		void* values[LEAF_LENGTH];
	};
	Leaf* _root[ROOT_LENGTH];
public:
	typedef PageID Number;
	explicit TcMalloc_PageMap2()
	{
		memset(_root, 0, sizeof(_root));
		PreallocateMoreMemory();
	}

	void Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1; )
		{
			const Number i1 = key >> LEAF_BITS;
			//����Ƿ�Խ��
			if (i1 >= ROOT_LENGTH) return;
			//����б�Ҫ�������ڶ���
			if (_root[i1] == nullptr)
			{
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = (Leaf*)(leafPool.New());
				memset(leaf, 0, sizeof(*leaf));
				_root[i1] = leaf;
			}
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
	}

	void PreallocateMoreMemory()
	{
		Ensure(0, 1 << BITS);
	}

	void* get(Number k)const
	{
		//�ڸ�����е�����
		const Number i1 = k >> LEAF_BITS;
		//��Ҷ�ӽ���е�λ��
		const Number i2 = k & (LEAF_LENGTH - 1);
		//Խ����߸����Ϊ��
		if ((k >> BITS) > 0 || _root[i1] == nullptr)
			return nullptr;
		return _root[i1]->values[i2];
	}
	void Set(Number k, void* v)
	{
		//�ڸ�����е�����
		const Number i1 = k >> LEAF_BITS;
		//��Ҷ�ӽ���е�λ��
		const Number i2 = k & (LEAF_LENGTH - 1);
		assert(i1 < ROOT_LENGTH);
		_root[i1]->values[i2] = v;
	}
};