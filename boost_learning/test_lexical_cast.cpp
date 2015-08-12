#include <boost/lexical_cast.hpp>
#include <vector>
#include <iostream>

int main(int argc, char **argv)
{
  if (argc == 1){return 0;}
  using boost::lexical_cast;
  using boost::bad_lexical_cast;
  std::vector<short> args;
  while(*++argv)
  {
    try
    {
      args.push_back(lexical_cast<short>(*argv));
    }
    catch(const bad_lexical_cast &)
    {
      std::cout << "bad cast for :" << *argv <<  std::endl;
    }
  }

  for (std::vector<short>::iterator it = args.begin(); it != args.end(); ++it)
  {
      std::cout << "cast for :" << *it <<  std::endl;
  }
  return 0;
}
