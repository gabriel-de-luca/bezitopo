/******************************************************/
/*                                                    */
/* brent.cpp - Brent's root-finding method            */
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
#include <utility>
#include <iostream>
#include <cmath>
#include "cogo.h"
#include "ldecimal.h"
#include "brent.h"

using namespace std;

/* Input:
 * 9s trit is the sign of the contrapoint,
 * 3s trit is the sign of the new point,
 * 1s trit is the sign of the old point.
 * Output:
 * 0: done
 * 1: new point becomes old point
 * 2: new point becomes contrapoint
 * 3: should never happen.
 */
char sidetable[27]=
{
  3,3,2, 3,0,0, 3,3,1,
  3,3,3, 3,0,3, 3,3,3,
  1,3,3, 0,0,3, 2,3,3
};

double invquad(double x0,double y0,double x1,double y1,double x2,double y2)
{
  double z0,z1,z2,offx;
  z0=x0;
  z1=x1;
  z2=x2;
  if (z0>z1)
    swap(z0,z1);
  if (z1>z2)
    swap(z1,z2);
  if (z0>z1)
    swap(z0,z1);
  //cout<<"Brent: "<<ldecimal(x0)<<' '<<ldecimal(x1)<<' '<<ldecimal(x2)<<endl;
  //cout<<"Before: "<<ldecimal(x0-x1)<<' '<<ldecimal(x1-x2)<<' '<<ldecimal(x2-x0)<<endl;
  if (z0<0 && z2>0)
    offx=0;
  if (z0>=0)
    offx=z0;
  if (z2<=0)
    offx=z2;
  x0-=offx;
  x1-=offx;
  x2-=offx;
  //cout<<"After:  "<<ldecimal(x0-x1)<<' '<<ldecimal(x1-x2)<<' '<<ldecimal(x2-x0)<<endl;
  z0=x0*y1*y2/(y0-y1)/(y0-y2);
  z1=x1*y2*y0/(y1-y2)/(y1-y0);
  z2=x2*y0*y1/(y2-y0)/(y2-y1);
  return (z0+z1+z2)+offx;
}

bool brent::between(double s)
{
  double g=(3*a+b)/4;
  return (g<s && s<b) || (b<s && s<g);
}

double brent::init(double x0,double y0,double x1,double y1)
{
  if (fabs(y0)>fabs(y1))
  {
    a=x0;
    fa=y0;
    b=x1;
    fb=y1;
  }
  else
  {
    a=x1;
    fa=y1;
    b=x0;
    fb=y0;
  }
  mflag=true;
  x=b-fb*(a-b)/(fa-fb);
  if (!between(x))
    x=(a+b)/2;
  if ((y0>0 && y1>0) || (y0<0 && y1<0))
    x=NAN;
  return x;
}

double brent::step(double y)
{
  double s,bsave=b,fbsave=fb;
  if (fa==fb || fb==y || y==fa)
    s=x-y*(b-x)/(fb-y);
  else
    s=invquad(a,fa,b,fb,x,y);
  if (between(s))
    mflag=false;
  else
  {
    mflag=true;
    s=(a+b)/2;
  }
  side=sidetable[9*sign(fa)+3*sign(y)+sign(fb)+13];
  switch (side)
  {
    case 0:
      s=x;
      break;
    case 1:
      b=x;
      fb=y;
      break;
    case 2:
      a=x;
      fa=y;
      break;
    case 3:
      s=NAN;
  }
  if (mflag && (s==a || s==b)) // interval [a,b] is too small to bisect, we're done
  {
    s=b;
    side=0;
  }
  if (side%3)
  {
    if (fabs(fb)>fabs(fa))
    {
      swap(fa,fb);
      swap(a,b);
    }
    d=c;
    c=bsave;
    x=s;
    fd=fc;
    fc=fbsave;
  }
  return s;
}