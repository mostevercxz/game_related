/*
 * 给定一个字符串，要求把字符串前面的若干个字符移动到字符串的尾部，如把字符串“abcdef”前面的2个字符'a'和'b'移动到字符串的尾部，使得原字符串变成字符串“cdefab”。请写一个函数完成此功能，要求对长度为n的字符串操作的时间复杂度为 O(n)，空间复杂度为 O(1)。
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
