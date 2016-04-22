#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <string>
#include <iostream>

struct Employee
{
  int m_id;
  int m_age;
  std::string m_name;
  Employee(int id, int age, const std::string &name):m_id(id), m_age(age), m_name(name){}
  
  bool operator < (const Employee &e) const {return this->m_id < e.m_id;}
  friend std::ostream & operator << (std::ostream &os, const Employee &e)
  {
    os << "Employee, id=" << e.m_id << " age=" << e.m_age << " name=" << e.m_name << std::endl;
    return os;
  }
};

using namespace ::boost;
using namespace ::boost::multi_index;

typedef multi_index_container<
  Employee,
  ::indexed_by<
    ::ordered_unique<identity<Employee> >,
    ::ordered_non_unique<member<Employee, std::string, &Employee::m_name> >,
    ::ordered_non_unique<member<Employee, int, &Employee::m_age> >
  >
> EmployeeSet;

typedef EmployeeSet::nth_index<0>::type IdIndex;
typedef EmployeeSet::nth_index<1>::type NameIndex;
typedef EmployeeSet::nth_index<2>::type AgeIndex;


//Part 2, sequenced indices
#include <boost/tokenizer.hpp>
#include <list>
std::string text=
  "Alice was beginning to get very tired of sitting by her sister on the "
  "bank, and of having nothing to do: once or twice she had peeped into the "
  "book her sister was reading, but it had no pictures or conversations in "
  "it, 'and what is the use of a book,' thought Alice 'without pictures or "
  "conversation?'";

typedef std::list<std::string> TextContainer;
TextContainer tc;
boost::tokenizer<boost::char_separator<char> > tok
  (text, boost::char_separator<char>(" \t\n.,;:!?'\"-"));

//std::copy(tok.begin(), tok.end(), std::back_inserter(tc));

int main()
{
  EmployeeSet set;
  set.insert(Employee(0, 35, "Boss"));
  set.insert(Employee(1, 32, "SmallBoss"));
  set.insert(Employee(2, 22, "LittleEmployee"));

  IdIndex& ids = set.get<0>();
  std::copy(ids.begin(), ids.end(), std::ostream_iterator<Employee>(std::cout));
  std::cout << " ids end" << std::endl;

  NameIndex &names = set.get<1>();
  std::copy(names.begin(), names.end(), std::ostream_iterator<Employee>(std::cout));
  std::cout << " names end" << std::endl;

  AgeIndex &ages = set.get<2>();
  std::copy(ages.begin(), ages.end(), std::ostream_iterator<Employee>(std::cout));
  std::cout << " ages end" << std::endl;
  return 0;
}
