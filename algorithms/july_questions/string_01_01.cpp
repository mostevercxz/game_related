/*
 * ����һ���ַ�����Ҫ����ַ���ǰ������ɸ��ַ��ƶ����ַ�����β��������ַ�����abcdef��ǰ���2���ַ�'a'��'b'�ƶ����ַ�����β����ʹ��ԭ�ַ�������ַ�����cdefab������дһ��������ɴ˹��ܣ�Ҫ��Գ���Ϊn���ַ���������ʱ�临�Ӷ�Ϊ O(n)���ռ临�Ӷ�Ϊ O(1)��
 */

#include <stdlib.h>
#include <iostream>
void reverseString(char *s, const unsigned int start, const unsigned int end)
{
	if(end <= start)
	{
					return;
	}
	char temp;
	for(int i = start, j = end; j > i; ++i, --j)
	{
		temp = s[i];
		s[i] = s[j];
		s[j] = temp;
	}
}


void simple_shift(char *array, const unsigned int nLen, const unsigned int moveBits)
{
	if(!array || (nLen == 0)){return ;}
	if(0 == moveBits){return ;}
	if(moveBits > nLen){return ;}
	if(nLen == 1){return ;}
	
	reverseString(array, 0, moveBits-1);
	reverseString(array, moveBits, nLen-1);
	reverseString(array, 0, nLen-1);
}

int main()
{
	char temp[] = "abcdefg";
	simple_shift(temp, sizeof(temp) - 1 , 2);
	std::cout << temp << std::endl;
	return 0;
}
