/******************************************************/
/*                                                    */
/* test.cpp - test patterns and functions             */
/*                                                    */
/******************************************************/

#include <cmath>
#include "bezitopo.h"
#include "test.h"
#include "angle.h"
#include "pointlist.h"

using std::map;

void dumppoints()
{map<int,point>::iterator i;
 printf("dumppoints\n");
 for (i=pointlists[1].points.begin();i!=pointlists[1].points.end();i++)
     i->second.dump();
 printf("end dump\n");
 }

void dumppointsvalence()
{
  map<int,point>::iterator i;
  printf("dumppoints\n");
  for (i=pointlists[1].points.begin();i!=pointlists[1].points.end();i++)
    printf("%d %d\n",i->first,i->second.valence());
  printf("end dump\n");
}

double flatslope(xy pnt)
{
  double z;
  z=pnt.east()/7+pnt.north()/17;
  return z;
}

xy flatslopegrad(xy pnt)
{
  return xy(1/7.,1/17.);
}

double rugae(xy pnt)
{
  double z;
  z=sin(pnt.east())+pnt.north()/50;
  return z;
}

xy rugaegrad(xy pnt)
{
  return xy(cos(pnt.east()),0.02);
}

double hypar(xy pnt)
{
  double z;
  z=(sqr(pnt.east())-sqr(pnt.north()))/50;
  return z;
}

xy hypargrad(xy pnt)
{
  return xy(pnt.east()/25,-pnt.north()/25);
}

double cirpar(xy pnt)
{
  double z;
  z=(sqr(pnt.east())+sqr(pnt.north()))/50;
  return z;
}

xy cirpargrad(xy pnt)
{
  return xy(pnt.east()/25,pnt.north()/25);
}

double (*testsurface)(xy pnt)=rugae;
xy (*testsurfacegrad)(xy pnt)=rugaegrad;

void setsurface(int surf)
{
  switch (surf)
  {
    case RUGAE:
      testsurface=rugae;
      testsurfacegrad=rugaegrad;
      break;
    case HYPAR:
      testsurface=hypar;
      testsurfacegrad=hypargrad;
      break;
    case CIRPAR:
      testsurface=cirpar;
      testsurfacegrad=cirpargrad;
      break;
    case FLATSLOPE:
      testsurface=flatslope;
      testsurfacegrad=flatslopegrad;
      break;
  }
}

void aster(int n)
/* Fill points with asteraceous pattern. Pattern invented by H. Vogel in 1979
   and later by me, not knowing of Vogel. */
{int i;
 double angle=(sqrt(5)-1)*M_PI;
 xy pnt;
 for (i=0;i<n;i++)
     {pnt=xy(cos(angle*i)*sqrt(i+0.5),sin(angle*i)*sqrt(i+0.5));
      pointlists[1].addpoint(i+1,point(pnt,testsurface(pnt),"test"));
      }
 }

void _ellipse(int n,double skewness)
/* Skewness is not eccentricity. When skewness=0.01, eccentricity=0.14072. */
{
  int i;
  double angle=(sqrt(5)-1)*M_PI;
  xy pnt;
  for (i=0;i<n;i++)
  {
    pnt=xy(cos(angle*i)*sqrt(n+0.5)*(1-skewness),sin(angle*i)*sqrt(n+0.5)*(1+skewness));
    pointlists[1].addpoint(i+1,point(pnt,testsurface(pnt),"test"));
  }
}

void regpolygon(int n)
{
  int i;
  double angle=2*M_PI/n;
  xy pnt;
  for (i=0;i<n;i++)
  {
    pnt=xy(cos(angle*i)*sqrt(n+0.5),sin(angle*i)*sqrt(n+0.5));
    pointlists[1].addpoint(i+1,point(pnt,testsurface(pnt),"test"));
  }
}

void ring(int n)
/* Points in a circle, for most ambiguous case of the Delaunay algorithm.
 * The number of different ways to make the TIN is a Catalan number.
 */
{
  _ellipse(n,0);
}

void ellipse(int n)
/* Points in an ellipse, for worst case of the Delaunay algorithm. */
{
  _ellipse(n,0.01);
}

void longandthin(int n)
{
  _ellipse(n,0.999);
}

void straightrow(int n)
// Add points in a straight line.
{
  int i;
  double angle;
  xy pnt;
  for (i=0;i<n;i++)
  {
    angle=(2.0*i/(n-1)-1)*M_PI/6;
    pnt=xy(0,sqrt(n)*tan(angle));
    pointlists[1].addpoint(i+1,point(pnt,testsurface(pnt),"test"));
  }
}

void lozenge(int n)
// Add points on the short diagonal of a rhombus, then add the two other points.
{
  xy pnt;
  straightrow(n);
  pnt=xy(-sqrt(n),0);
  pointlists[1].addpoint(n+1,point(pnt,testsurface(pnt),"test"));
  pnt=xy(sqrt(n),0);
  pointlists[1].addpoint(n+2,point(pnt,testsurface(pnt),"test"));
}

void rotate(int n)
{int i;
 double tmpx,tmpy;
 map<int,point>::iterator j;
 for (j=pointlists[1].points.begin();j!=pointlists[1].points.end();j++)
     for (i=0;i<n;i++)
         {tmpx=j->second.x*0.6-j->second.y*0.8;
          tmpy=j->second.y*0.6+j->second.x*0.8;
          j->second.x=tmpx;
          j->second.y=tmpy;
          }
 }

void movesideways(double sw)
{
  int i;
  double tmpx,tmpy;
  map<int,point>::iterator j;
  for (j=pointlists[1].points.begin();j!=pointlists[1].points.end();j++)
    j->second.x+=sw;
}

void enlarge(double sc)
{
  int i;
  double tmpx,tmpy;
  map<int,point>::iterator j;
  for (j=pointlists[1].points.begin();j!=pointlists[1].points.end();j++)
  {
    j->second.x*=sc;
    j->second.y*=sc;
  }
}
