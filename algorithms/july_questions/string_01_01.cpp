/*
 * 给定一个字符串，要求把字符串前面的若干个字符移动到字符串的尾部，如把字符串“abcdef”前面的2个字符'a'和'b'移动到字符串的尾部，使得原字符串变成字符串“cdefab”。请写一个函数完成此功能，要求对长度为n的字符串操作的时间复杂度为 O(n)，空间复杂度为 O(1)。
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
