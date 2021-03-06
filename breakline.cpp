/******************************************************/
/*                                                    */
/* breakline.cpp - breaklines                         */
/*                                                    */
/******************************************************/
/* Copyright 2017-2019 Pierre Abbat.
 * This file is part of Bezitopo.
 *
 * Bezitopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Bezitopo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License and Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and Lesser General Public License along with Bezitopo. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <cstring>
#include <climits>
#include <string>
#include "breakline.h"
#include "except.h"
using namespace std;

Breakline0::Breakline0()
{
}

Breakline0::Breakline0(int a,int b)
{
  nodes.push_back(a);
  nodes.push_back(b);
}

Breakline0::Breakline0(vector<string> numbers)
/* Some bad values (shown with hyphens):
 *              empty
 * 238          only one number, no edge
 * 91-92-9z-94  9z is not a number
 */
{
  int i;
  size_t idx;
  try
  {
    for (i=0;i<numbers.size();i++)
    {
      nodes.push_back(stoi(numbers[i],&idx));
      if (idx<numbers[i].length())
        throw idx;
    }
  }
  catch (...)
  {
    i=-1;
  }
  if (i<2)
    throw BeziExcept(badBreaklineFormat);
}

/* The breakline () is empty and open, and thus can be joined with any
 * breakline. It is the identity element of joining.
 * The breakline (a), where a is any integer, is empty but closed.
 * It should never occur. Since it is closed, it cannot be joined with
 * another breakline.
 */

bool Breakline0::isEmpty()
{
  return nodes.size()<2;
}

bool Breakline0::isOpen()
{
  if (nodes.size())
    return nodes[0]!=nodes.back();
  else
    return true;
}

void Breakline0::normalize()
{
  int i,j,least,leastpos,sz;
  if (nodes.size()>2 && !isOpen())
  {
    sz=nodes.size();
    for (i=0,least=INT_MAX;i<sz;i++)
      if (nodes[i]<least)
      {
        least=nodes[i];
        leastpos=i;
      }
    for (i=1;i<=leastpos;i++)
      nodes.push_back(nodes[i]);
    memmove(&nodes[0],&nodes[leastpos],sz*sizeof(int));
    nodes.resize(sz);
  }
  if ((nodes.size() && nodes[0]>nodes.back()) ||
      (nodes.size()>2 && nodes[0]==nodes.back() && nodes[1]>nodes[nodes.size()-2]))
    for (i=0,j=nodes.size()-1;i<j;i++,j--)
      swap(nodes[i],nodes[j]);
}

int Breakline0::lowEnd()
{
  return nodes[0];
}

int Breakline0::highEnd()
{
  return nodes.back();
}

int Breakline0::size()
{
  int sz=nodes.size();
  if (sz)
    sz--;
  return sz;
}

void Breakline0::reverse()
{
  int i;
  for (i=0;2*i<nodes.size();i++)
    swap(nodes[i],nodes[nodes.size()-1-i]);
}

Breakline0& Breakline0::operator<<(int endp)
{
  nodes.push_back(endp);
  return *this;
}

array<int,2> Breakline0::operator[](int n)
{
  array<int,2> ret;
  ret[0]=nodes[n];
  ret[1]=nodes[n+1];
  return ret;
}

bool jungible(Breakline0 &a,Breakline0 &b)
{
  bool ret=a.isOpen() && b.isOpen();
  if (ret && a.nodes.size() && b.nodes.size())
    ret=a.highEnd()==b.highEnd() || a.highEnd()==b.lowEnd() ||
        a.lowEnd()==b.highEnd() || a.lowEnd()==b.lowEnd();
  return ret;
}

Breakline0 operator+(Breakline0 &a,Breakline0 &b)
/* a and b should be jungible. If they aren't, it returns garbage.
 * The sum is returned normalized;
 */
{
  Breakline0 ret;
  int i;
  bool atailbtail,aheadbtail,atailbhead,aheadbhead;
  if (a.isEmpty())
    ret=b;
  else if (b.isEmpty())
    ret=a;
  else
  {
    aheadbhead=a.lowEnd()==b.lowEnd();
    aheadbtail=a.lowEnd()==b.highEnd();
    atailbhead=a.highEnd()==b.lowEnd();
    atailbtail=a.highEnd()==b.highEnd();
    if (aheadbhead)
    {
      for (i=a.nodes.size()-1;i>=0;i--)
        ret.nodes.push_back(a.nodes[i]);
      for (i=1;i<b.nodes.size();i++)
        ret.nodes.push_back(b.nodes[i]);
    }
    else if (aheadbtail)
    {
      for (i=a.nodes.size()-1;i>=0;i--)
        ret.nodes.push_back(a.nodes[i]);
      for (i=b.nodes.size()-2;i>=0;i--)
        ret.nodes.push_back(b.nodes[i]);
    }
    else if (atailbhead)
    {
      for (i=0;i<a.nodes.size();i++)
        ret.nodes.push_back(a.nodes[i]);
      for (i=1;i<b.nodes.size();i++)
        ret.nodes.push_back(b.nodes[i]);
    }
    else
    {
      for (i=0;i<a.nodes.size();i++)
        ret.nodes.push_back(a.nodes[i]);
      for (i=b.nodes.size()-2;i>=0;i--)
        ret.nodes.push_back(b.nodes[i]);
    }
    ret.normalize();
  }
  return ret;
}

void Breakline0::writeText(ostream &ofile)
{
  int i;
  for (i=0;i<nodes.size();i++)
  {
    if (i)
      ofile<<'-';
    ofile<<nodes[i];
  }
}

void Breakline0::writeXml(ostream &ofile)
{
  int i;
  ofile<<"<break0>";
  for (i=0;i<nodes.size();i++)
  {
    if (i)
      ofile<<' ';
    ofile<<nodes[i];
  }
  ofile<<"</break0>\n";
}

vector<string> parseBreakline(string line,char delim)
{
  vector<string> ret;
  size_t pos;
  while ((pos=line.find(delim))<line.length())
  {
    ret.push_back(line.substr(0,pos));
    line.erase(0,pos+1);
  }
  if (line.length() || ret.size())
    ret.push_back(line);
  return ret;
}
