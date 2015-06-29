/*
 * ����һ���ַ�����Ҫ����ַ���ǰ������ɸ��ַ��ƶ����ַ�����β��������ַ�����abcdef��ǰ���2���ַ�'a'��'b'�ƶ����ַ�����β����ʹ��ԭ�ַ�������ַ�����cdefab������дһ��������ɴ˹��ܣ�Ҫ��Գ���Ϊn���ַ���������ʱ�临�Ӷ�Ϊ O(n)���ռ临�Ӷ�Ϊ O(1)��
 */

#include <stdlib.h>
#include <iostream>
void simple_shift(char *array, const unsigned int nLen, const unsigned int moveBits)
{
	if(!array || (nLen == 0)){return ;}
	if(0 == moveBits){return ;}
	if(moveBits > nLen){return ;}
	if(nLen == 1){return ;}
	
	char *temp = (char*)malloc(moveBits * sizeof(char));
	int i = 0;
	for(; i < moveBits; ++i)
	{
		temp[i] = array[i];
	}

	for(; i < nLen; ++i)
	{
		array[i-moveBits] = array[i];
	}

	int j = 0;
	for(i = i - moveBits; i < nLen; ++i,++j)
	{
		array[i] = temp[j];
	}
}

int main()
{
	char temp[] = "abcdefg";
	simple_shift(temp, sizeof(temp) - 1 , 2);
	std::cout << temp << std::endl;
	return 0;
}
