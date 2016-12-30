/******************************************************/
/*                                                    */
/* geoidboundary.cpp - geoid boundaries               */
/*                                                    */
/******************************************************/
/* Copyright 2016 Pierre Abbat.
 * This file is part of Bezitopo.
 * 
 * Bezitopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Bezitopo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bezitopo. If not, see <http://www.gnu.org/licenses/>.
 */
#include <cassert>
#include "geoid.h"
#include "geoidboundary.h"
using namespace std;

char vballcompare[8][8]=
{
  {00,77,77,77,77,77,77,77},
  {77,66,12,21,14,36,77,77},
  {77,21,66,12,36,77,14,77},
  {77,12,21,66,77,14,36,77},
  {77,41,63,77,66,45,54,77},
  {77,63,77,41,54,66,45,77},
  {77,77,41,63,45,54,66,77},
  {77,77,77,77,77,77,77,77}
};

bool operator==(const vball &a,const vball &b)
{
  int edgetype=vballcompare[a.face][b.face];
  bool ret=false;
  switch (edgetype)
  {
    case 00:
      ret=true;
      break;
    case 12:
      ret=a.x==1 && a.y==b.x && b.y==1;
      break;
    case 21:
      ret=a.y==1 && a.x==b.y && b.x==1;
      break;
    case 14:
      ret=a.y==-1 && a.x==-b.y && b.x==1;
      break;
    case 41:
      ret=a.x==1 && a.y==-b.x && b.y==-1;
      break;
    case 36:
      ret=a.x==-1 && a.y==b.x && b.y==-1;
      break;
    case 63:
      ret=a.y==-1 && a.x==b.y && b.x==-1;
      break;
    case 45:
      ret=a.y==1 && a.x==-b.y && b.x==-1;
      break;
    case 54:
      ret=a.x==-1 && a.y==-b.x && b.y==1;
      break;
    case 66:
      ret=a.x==b.x && a.y==b.y;
      break;
  }
  return ret;
}

bool sameEdge(const vball &a,const vball &b)
{
  int edgetype=vballcompare[a.face][b.face];
  bool ret=false;
  switch (edgetype)
  {
    case 00:
      ret=true;
      break;
    case 12:
      ret=a.x==1 && b.y==1;
      break;
    case 21:
      ret=a.y==1 && b.x==1;
      break;
    case 14:
      ret=a.y==-1 && b.x==1;
      break;
    case 41:
      ret=a.x==1 && b.y==-1;
      break;
    case 36:
      ret=a.x==-1 && b.y==-1;
      break;
    case 63:
      ret=a.y==-1 && b.x==-1;
      break;
    case 45:
      ret=a.y==1 && b.x==-1;
      break;
    case 54:
      ret=a.x==-1 && b.y==1;
      break;
    case 66:
      ret=a.x==b.x || a.y==b.y;
      break;
  }
  return ret;
}

char log29[]={
  63,
  0,1,5,2,22,6,12,
  3,10,23,25,7,18,13,
  27,4,21,11,9,24,17,
  26,20,8,16,19,15,14
};

int splitLevel(double coord)
/* Returns the number of times a geoquad has to be split to produce
 * (coord,coord) as a boundary point. This is used when merging boundaries,
 * as only those segments with the lowest level need be considered.
 */
{
  int i,n,ret;
  if (coord==rint(coord))
    ret=coord==0;
  else
  {
    coord=fabs(coord);
    for (n=-1,i=0;coord!=n;i++)
    {
      coord=(coord-trunc(coord))*16777216;
      n=rint(coord);
    }
    n=n&-n;
    ret=i*24-log29[n%29]+1;
  }
  return ret;
}

int splitLevel(vball v)
{
  int xLevel=splitLevel(v.x),yLevel=splitLevel(v.y);
  if (xLevel<yLevel)
    return xLevel;
  else
    return yLevel;
}

int splitLevel(vsegment v)
{
  if (v.start.face==v.end.face)
    if (v.start.x==v.end.x)
      return splitLevel(v.start.x);
    else if (v.start.y==v.end.y)
      return splitLevel(v.start.y);
    else
      return -1;
  else if (sameEdge(v.start,v.end))
    return 0;
  else
    return -1;
}

void g1boundary::push_back(vball v)
/* A g1boundary is initialized with four points, the corners of a geoquad
 * in counterclockwise order. A clockwise g1boundary is the boundary
 * of a hole in a region.
 */
{
  bdy.push_back(v);
}

vsegment g1boundary::seg(int n)
{
  vsegment ret;
  assert(bdy.size());
  n%=bdy.size();
  if (n<0)
    n+=bdy.size();
  ret.start=bdy[n];
  ret.end=bdy[(n+1)%bdy.size()];
  return ret;
}

vector<int> g1boundary::segmentsAtLevel(int l)
/* This returns indices, not segments, because the indices will be necessary
 * for surgery.
 */
{
  int i;
  vector<int> ret;
  for (i=0;i<bdy.size();i++)
    if (splitLevel(seg(i))==l)
      ret.push_back(i);
  return ret;
}
