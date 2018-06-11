/******************************************************/
/*                                                    */
/* manyarc.cpp - approximate spiral with many arcs    */
/*                                                    */
/******************************************************/
/* Copyright 2018 Pierre Abbat.
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
#include "manyarc.h"
#include "rootfind.h"
#include "manysum.h"
using namespace std;

/* Spiralarcs are used for centerlines of highways. A property line or easement
 * may be defined as a distance offset from the centerline of a highway or
 * railroad, but an offset from a spiral is hard to work with. Previously
 * surveyors have connected points on the offset with straight lines, but
 * that doesn't look very good. Instead, one should approximate the spiralarc
 * with several circular arcs and offset the arcs.
 */

double manyArcTrimFunc(double p,double n)
{
  manysum ret;
  ret-=4*p*p*p/3;
  ret+=n*p*p;
  ret+=p*p;
  ret-=n*p;
  ret+=n/6;
  return ret.total();
}

double manyArcTrimDeriv(double p,double n)
{
  manysum ret;
  ret-=4*p*p;
  ret+=n*p*2;
  ret+=p*2;
  ret-=n;
  return ret.total();
}

double manyArcTrim(unsigned n)
/* Computes the amount by which to trim [0,n] to get n segments to fit n arcs
 * to a spiralarc. Define f(x) as piecewise linear from (0,0) to (1,1) to (2,4)
 * to (3,9) and so on. Lower f(x) by about 1/6 so that there's as much area
 * above f(x) and below x² as below f(x) and above x². (It's exactly 1/6 when
 * n is infinite.) Then trim p off each end where f(x) intersects x².
 * 
 * ∫ [p,n-p] (f(x)-f(p)+p²-x²) dx = 0
 * -4p³/3+(n+1)p²-np+n/6=0
 * There are two solutions in [0,1]; we want the one in [0,1/2].
 */
{
  double p;
  Newton ne;
  p=ne.init(0,manyArcTrimFunc(0,n),manyArcTrimDeriv(0,n),0.5,manyArcTrimFunc(0.5,n),manyArcTrimDeriv(0.5,n));
  while (!ne.finished())
  {
    p=ne.step(manyArcTrimFunc(p,n),manyArcTrimDeriv(p,n));
  }
  return p;
}

polyarc manyArcUnadjusted(spiralarc a,int narcs)
{
  int sb=a.startbearing(),eb=a.endbearing();
  int i;
  double piecelength,thispiecelength,chordlength,abscissa[5],overhang;
  double startbear,midbear,endbear;
  int midbeara,midbearb;
  double p,q,cur;
  polyarc ret;
  xy thispoint=a.getstart();
  ret.insert(thispoint);
  p=manyArcTrim(narcs);
  q=p*p-p+0.25;
  piecelength=a.length()/(narcs-2*p);
  overhang=piecelength*p;
  for (i=0;i<narcs;i++)
  {
    abscissa[1]=i*piecelength;
    abscissa[0]=abscissa[1]-overhang;
    abscissa[2]=abscissa[0]+piecelength/2;
    abscissa[4]=abscissa[0]+piecelength;
    abscissa[3]=abscissa[4]-overhang;
    midbeara=a.bearing(abscissa[3]);
    midbearb=a.bearing(abscissa[1]);
    midbear=bintorad(midbeara)+bintorad(midbearb-midbeara)/2;
    cur=bintorad(midbeara-midbearb)/(abscissa[3]-abscissa[1]);
    thispiecelength=piecelength;
    if (i)
      startbear=midbear-cur*piecelength/2;
    else
    {
      startbear=midbear-cur*(abscissa[2]-abscissa[1]);
      thispiecelength-=overhang;
    }
    if (i<narcs-1)
      endbear=midbear+cur*piecelength/2;
    else
    {
      endbear=midbear+cur*(abscissa[3]-abscissa[2]);
      thispiecelength-=overhang;
    }
    chordlength=thispiecelength*cos((endbear-startbear)/2);
    thispoint+=chordlength*cossin((endbear+startbear)/2);
    ret.insert(thispoint);
    ret.setdelta(i,radtobin(endbear-startbear));
  }
  ret.open();
  ret.setlengths();
  return ret;
}