/* 
 * File:   perftimer.h
 * Author: fib
 *
 * Created on 19 December 2011, 15:53
 */

#ifndef PERFTIMER_H
#define	PERFTIMER_H

#include "timer.h"

#include <assert.h>
#include <stdio.h>

#include <map>
#include <stack>
#include <string>

class PerfTimer
{
public:
  PerfTimer(char const* const _name) :
    m_name(_name),
    m_startTime(Timer::GetPerfTime()),
    m_entry(Start(_name))
  {
  }
  
  ~PerfTimer()
  {
    m_entry->time += Timer::GetPerfTime() - m_startTime;
    Stop(m_entry);
  }
  
  struct Entry;
  
  typedef std::map<std::string, Entry> Map;
  typedef Map::const_iterator Iter;
  
  struct Entry
  {
    Entry() : name(), time(0), children() {}
    std::string name;
    Timer::PerfTime time;
    Map children;
  };
  
  static Entry* Start(char const* const _name)
  {
    Entry* curEntry = s_stack.top();
    Map& children = curEntry->children;
    
    children.insert(std::make_pair(_name, Entry()));
    Entry* newEntry = &children.at(_name);
    
    s_stack.push(newEntry);
    return newEntry;
  }
  
  static void Stop(Entry* const _entry)
  {
    assert(s_stack.top() == _entry);
    s_stack.pop();
  }
  
  static void StaticInit()
  {
    s_startTime = Timer::GetPerfTime();
    s_stack.push(&s_parent);
  }
  
  static void StaticShutdown()
  {
    s_parent.time += Timer::GetPerfTime() - s_startTime;
    s_stack.pop();

    Print();
  }
  
  static void Print()
  {
    Print(&s_parent, std::string());
  }
  
  static void Print(Entry const* const _entry, std::string const& _indent);

private:
  char const* m_name;
  Timer::PerfTime m_startTime;
  Entry* m_entry;
  
  static std::stack<Entry*> s_stack;
  static Entry s_parent;
  static Timer::PerfTime s_startTime;
};

#define PERFTIMER(_NAME) PerfTimer perfTimer##__LINE__(_NAME);

#endif	/* PERFTIMER_H */

