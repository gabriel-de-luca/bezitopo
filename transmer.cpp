/******************************************************/
/*                                                    */
/* transmer.cpp - series for transverse Mercator      */
/*                                                    */
/******************************************************/
/* Copyright 2018-2020 Pierre Abbat.
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
 *
 * This file is distributed under the terms of the GNU General Public
 * License, because it uses FFTW.
 */
#include <string>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <fftw3.h>
#include "config.h"
#include "polyline.h"
#include "ldecimal.h"
#include "projection.h"
#include "ellipsoid.h"
#include "ps.h"
#include "vball.h"
#include "manysum.h"
#include "binio.h"

#define PETERS 1
#define EQUIREC 0
#define NDOTS 10000

using namespace std;

vector<string> args;
map<int,fftw_plan> plans;
map<int,double *> inmem,outmem;

fftw_plan makePlan(int n)
// fftw_plan is a pointer to plan_s, so it can be checked as bool
{
  if (!plans[n])
  {
    inmem[n]=fftw_alloc_real(n);
    outmem[n]=fftw_alloc_real(n);
    plans[n]=fftw_plan_r2r_1d(n,inmem[n],outmem[n],FFTW_RODFT10,FFTW_ESTIMATE);
  }
  return plans[n];
}

void destroyPlans()
{
  map<int,fftw_plan>::iterator i;
  map<int,double *>::iterator j;
  for (i=plans.begin();i!=plans.end();i++)
    fftw_destroy_plan(i->second);
  for (j=inmem.begin();j!=inmem.end();j++)
    fftw_free(j->second);
  for (j=outmem.begin();j!=outmem.end();j++)
    fftw_free(j->second);
  plans.clear();
  inmem.clear();
  outmem.clear();
}

vector<double> fft(vector<double> input)
/* The output is calibrated so that the frequency-domain terms are independent
 * of the size of the input.
 */
{
  vector<double> output(input);
  int i,sz=input.size();
  fftw_plan plan=makePlan(sz);
  memcpy(inmem[sz],&input[0],sz*sizeof(double));
  fftw_execute(plan);
  for (i=0;i<sz;i++)
    output[i]=outmem[sz][i]/sz;
  return output;
}

polyspiral psApprox(ellipsoid *ell,int n)
/* Computes an n-piece approximation to a quadrant of meridian of the ellipsoid.
 * The ellipse is a quadratic algebraic curve, but its length is not in closed form.
 * The spiral's length is simply its parameter. Its position is not in closed form,
 * but I've already solved that problem with series. So I approximate the ellipse
 * with spiralarcs.
 * 
 * The purpose of this is to compute the function from length along the ellipsoid's
 * meridian to length along the sphere's meridian under the conformal map. This is
 * the same as the function from the ellipsoidal transverse Mercator map to the
 * spherical transverse Mercator map along the central meridian. The rest of the map
 * follows by conformality.
 */
{
  int i;
  polyspiral ret;
  xyz meridianPoint;
  vector<int> latSplit;
  ret.smooth();
  for (i=0;i<=n;i++)
  {
    latSplit.push_back(rint((double)DEG90*i/n));
    meridianPoint=ell->geoc(latSplit[i],0,0)-ell->getCenter();
    ret.insert(xy(meridianPoint.getx(),meridianPoint.getz()));
  }
  ret.open();
  ret.setlengths();
  for (i=0;i<=n;i++)
    ret.setbear(i,latSplit[i]+DEG90);
  for (i=0;i<n;i++)
    ret.setspiral(i);
  ret.setlengths();
  return ret;
}

double compareLengths(polyspiral fewer,polyspiral more)
{
  int i,j,ratio;
  vector<double> diff;
  double sum;
  ratio=more.size()/fewer.size();
  for (i=0;i<fewer.size();i++)
  {
    for (sum=j=0;j<ratio;j++)
      sum+=more.getspiralarc(i*ratio+j).length();
    diff.push_back(sqr(sum-fewer.getspiralarc(i).length()));
  }
  return pairwisesum(diff);
}

array<double,3> compareTransforms(vector<double> fewer,vector<double> more)
/* ret[0] is the total square difference between fewer and the first half of more.
 * ret[1] is the total square of the last half of more.
 * ret[2] is the largest absolute value of the last half of more.
 * ret[1] and ret[2] are estimates of the noise floor.
 */
{
  vector<double> squares;
  array<double,3> ret;
  ret[2]=0;
  int i;
  for (i=0;i<fewer.size();i++)
    squares.push_back(sqr(fewer[i]-more[i]));
  ret[0]=pairwisesum(squares);
  squares.clear();
  for (;i<more.size();i++)
  {
    squares.push_back(sqr(more[i]));
    if (fabs(more[i])>ret[2])
      ret[2]=fabs(more[i]);
  }
  ret[1]=pairwisesum(squares);
  return ret;
}

vector<array<double,2> > projectForward(ellipsoid *ell,polyspiral apx,int n)
/* Projects n points (n is a power of 2) from the sphere to the ellipsoid,
 * returning a vector of lengths along the meridian. The vector has size n+1;
 * the last member is the North Pole, i.e. the total length of the meridian.
 * Sphere to ellipsoid is forward because that is used when projecting
 * from the ellipsoid to the plane.
 */
{
  int i;
  latlong llSphere,llEllipsoid;
  xyz meridianPoint;
  vector<array<double,2> > ret;
  array<double,2> totalLength,projPair;
  totalLength[0]=ell->sphere->geteqr()*M_PI/2;
  totalLength[1]=apx.length();
  for (i=0;i<n;i++)
  {
    llSphere=latlong(i*(DEG90/n)+DEG45/n,0);
    llEllipsoid=ell->inverseConformalLatitude(llSphere);
    meridianPoint=ell->geoc(llEllipsoid,0)-ell->getCenter();
    projPair[0]=llSphere.lat*ell->sphere->geteqr();
    projPair[1]=apx.closest(xy(meridianPoint.getx(),meridianPoint.getz()));
    ret.push_back(projPair);
    if (frac(i*64./n)==0.5)
    {
      cout<<'*';
      cout.flush();
    }
  }
  ret.push_back(totalLength);
  return ret;
}

vector<array<double,2> > projectBackward(ellipsoid *ell,polyspiral apx,int n)
/* Projects n points (n is a power of 2) from the ellipsoid to the sphere,
 * returning a vector of lengths along the meridian. The vector has size n+1;
 * the last member is the North Pole, i.e. the total length of the meridian.
 */
{
  int i;
  latlong llSphere,llEllipsoid;
  latlongelev lleEllipsoid;
  xyz meridianPoint;
  vector<array<double,2> > ret;
  array<double,2> totalLength,projPair;
  totalLength[1]=ell->sphere->geteqr()*M_PI/2;
  totalLength[0]=apx.length();
  for (i=0;i<n;i++)
  {
    projPair[0]=((i+0.5)/n)*totalLength[0];
    meridianPoint=apx.station(projPair[0]);
    lleEllipsoid=ell->geod(xyz(meridianPoint.getx(),0,meridianPoint.gety())+ell->getCenter());
    if (n<10)
      cout<<lleEllipsoid.elev<<' ';
    assert(fabs(lleEllipsoid.elev)<0.018/243);
    /* 18 mm is 1/2 angle ulp; 243 is the number of spiralarcs.
     */
    llEllipsoid=lleEllipsoid;
    llSphere=ell->conformalLatitude(llEllipsoid);
    projPair[1]=llSphere.lat*ell->sphere->geteqr();
    ret.push_back(projPair);
    if (frac(i*64./n)==0.5)
    {
      cout<<"\b \b";
      cout.flush();
    }
  }
  if (n<10)
    cout<<endl;
  ret.push_back(totalLength);
  return ret;
}

vector<double> exeutheicity(vector<array<double,2> > proj)
/* The amount by which something deviates from a straight line, from Greek
 * εξ + ευθεια, by analogy with "eccentricity".
 * proj[i][0] should be equally spaced, except the last, which gives the range.
 */
{
  vector<double> ret;
  int i;
  for (i=0;i<proj.size()-1;i++)
    ret.push_back((proj[i][1]/proj.back()[1]-proj[i][0]/proj.back()[0])*M_PI);
  return ret;
}

double median(double a,double b,double c)
{
  if (a>b)
    swap(a,b);
  if (b>c)
    swap(b,c);
  if (a>b)
    swap(a,b);
  return b;
}

void drawKrugerize(ellipsoid &ell,PostScript &ps,bool rev,int totalTerms)
{
  double yHalfHeight=1e7,xHalfHeight,sq=1e6;
  int x,y,maxx,maxy;
  vector<vector<xy> > nodes;
  vector<vector<int> > bears;
  xy node,node1;
  int bear,bear1;
  spiralarc edge;
  BoundRect br;
  if (totalTerms<8)
    xHalfHeight=2e7;
  else
    xHalfHeight=16e7/totalTerms;
  maxy=rint(yHalfHeight/sq);
  maxx=rint(xHalfHeight/sq);
  nodes.resize(maxy+1);
  bears.resize(maxy+1);
  for (y=0;y<=maxy;y++)
    for (x=0;x<=maxx;x++)
      if (rev)
      {
	nodes[y].push_back(ell.dekrugerize(xy(x*sq,y*sq)));
	bears[y].push_back(atan2i(ell.dekrugerizeDeriv(xy(x*sq,y*sq))));
      }
      else
      {
	nodes[y].push_back(ell.krugerize(xy(x*sq,y*sq)));
	bears[y].push_back(atan2i(ell.krugerizeDeriv(xy(x*sq,y*sq))));
      }
  for (y=0;y<=maxy;y++)
    for (x=0;x<=maxx;x++)
    {
      node=nodes[y][x];
      br.include(node);
      node=-node;
      br.include(node);
    }
  ps.startpage();
  ps.setscale(br);
  for (y=-maxy;y<=maxy;y++) // Draw horizontal lines
    for (x=-maxx;x<maxx;x++)
    {
      node=nodes[abs(y)][abs(x)];
      node1=nodes[abs(y)][abs(x+1)];
      bear=bears[abs(y)][abs(x)];
      bear1=bears[abs(y)][abs(x+1)];
      if (y<0)
      {
	node=xy(node.getx(),-node.gety());
	node1=xy(node1.getx(),-node1.gety());
	bear=-bear;
	bear1=-bear1;
      }
      if (x<0)
      {
	node=xy(-node.getx(),node.gety());
	bear=-bear;
      }
      if (x+1<0)
      {
	node1=xy(-node1.getx(),node1.gety());
	bear1=-bear1;
      }
      edge=spiralarc(xyz(node,0),xyz(node1,0));
      edge.setdelta(bear1-bear,bear1+bear-2*edge.chordbearing());
      ps.spline(edge.approx3d(1e3));
    }
  for (x=-maxx;x<=maxx;x++) // Draw vertical lines
    for (y=-maxy;y<maxy;y++)
    {
      node=nodes[abs(y)][abs(x)];
      node1=nodes[abs(y+1)][abs(x)];
      bear=bears[abs(y)][abs(x)]+DEG90;
      bear1=bears[abs(y+1)][abs(x)]+DEG90;
      if (x<0)
      {
	node=xy(-node.getx(),node.gety());
	node1=xy(-node1.getx(),node1.gety());
	bear=DEG180-bear;
	bear1=DEG180-bear1;
      }
      if (y<0)
      {
	node=xy(node.getx(),-node.gety());
	bear=DEG180-bear;
      }
      if (y+1<0)
      {
	node1=xy(node1.getx(),-node1.gety());
	bear1=DEG180-bear1;
      }
      edge=spiralarc(xyz(node,0),xyz(node1,0));
      edge.setdelta(bear1-bear,bear1+bear-2*edge.chordbearing());
      ps.spline(edge.approx3d(1e3));
    }
  ps.endpage();
}

int plotErrorDot(Projection &proj,PostScript &ps,latlong ll,int cylproj)
{
  xyz before3d,after3d;
  latlong afterll;
  double error,radius;
  int dottype;
  xy dotpos;
  afterll=proj.gridToLatlong(proj.latlongToGrid(ll));
  before3d=proj.ellip->geoc(ll,0);
  after3d=proj.ellip->geoc(afterll,0);
  error=dist(before3d,after3d);
  if (std::isfinite(error))
    dottype=floor(log10(error)+5);
  else
    dottype=15;
  if (dottype<0)
    dottype=0;
  switch (cylproj)
  {
    case PETERS:
      dotpos=xy(ll.lon,2*sin(ll.lat));
      break;
    case EQUIREC:
      dotpos=xy(ll.lon,ll.lat);
      break;
  }
  radius=(dottype+1)*0.002;
  if (dottype<3)
    ps.setcolor(0,0,1);
  else if (dottype<6)
    ps.setcolor(0,0,0);
  else
    ps.setcolor(1,0,0);
  ps.circle(dotpos,radius);
  if (dottype==15)
  {
    ps.setcolor(1,1,1);
    ps.circle(dotpos,2*radius/3);
  }
  return dottype;
}

void plotErrorPeters(ellipsoid &ell,PostScript &ps,ostream &merctext)
{
  TransverseMercatorEllipsoid proj(&ell,0);
  int i;
  latlong ll;
  int histo[16],tallestbar=0;
  polyline bar;
  double lo,hi,pre;
  Measure ms;
  ms.setMetric();
  memset(histo,0,sizeof(histo));
  ps.startpage();
  ps.setscale(-3.15,-2,3.15,2);
  for (i=-180;i<=180;i+=15)
    ps.line2p(xy(degtorad(i),-2),xy(degtorad(i),2));
  for (i=-90;i<=90;i+=15)
    ps.line2p(xy(-M_PI,2*sin(degtorad(i))),xy(M_PI,2*sin(degtorad(i))));
  for (i=0;i<NDOTS;i++)
  {
    ll.lat=asin((2*i+1.)/NDOTS-1);
    ll.lon=bintorad(foldangle(i*PHITURN));
    histo[plotErrorDot(proj,ps,ll,PETERS)]++;
  }
  ps.endpage();
  merctext<<"--------\n";
  for (i=0;i<16;i++)
    if (histo[i]>tallestbar)
      tallestbar=histo[i];
  ps.startpage();
  ps.setscale(0,0,42,28,0);
  ps.setcolor(0,0,0);
  for (i=0;i<4;i++)
  {
    ps.startline();
    ps.lineto(xy(6+9*i,-1));
    ps.lineto(xy(6+9*i,7));
    ps.endline();
  }
  ps.centerWrite(xy(6,-2),"1 mm");
  ps.centerWrite(xy(15,-2),"1 m");
  ps.centerWrite(xy(24,-2),"1 km");
  ps.centerWrite(xy(33,-2),"1 Mm");
  for (i=0;i<16;i++)
    if (histo[i])
    { // There are 14 bars, numbered 0 through 12 and 15.
      if (i==15)
	ps.setcolor(1,0,0);
      else
	ps.setcolor(0,0,1);
      bar=polyline(0);
      bar.insert(xy(i*3-6*(i>12)+0.5,0));
      bar.insert(xy(i*3-6*(i>12)+2.5,0));
      bar.insert(xy(i*3-6*(i>12)+2.5,histo[i]*28./tallestbar));
      bar.insert(xy(i*3-6*(i>12)+0.5,histo[i]*28./tallestbar));
      ps.spline(bar.approx3d(1),true);
      merctext<<ldecimal(histo[i]*1e2/NDOTS,0.01)<<"% ";
      if (i==15)
	merctext<<"floating-point overflow";
      else
      {
	lo=pow(10,i-5);
	hi=lo*10;
	if (i<5)
	  pre=0.001;
	else if (i<8)
	  pre=1;
	else
	  pre=1000;
	if (i==0)
	  lo=0;
	merctext<<ms.formatMeasurementUnit(lo,LENGTH,pre,pre/10)<<" - "<<ms.formatMeasurementUnit(hi,LENGTH,pre,pre/10);
      }
      merctext<<endl;
    }
  ps.endpage();
}

void header(ostream &merc)
{
  writeustring(merc,"TransMerFFT");
  writeleshort(merc,0); // version
  writeleshort(merc,FP_IEEE);
  writeleshort(merc,64);
}

void doEllipsoid(ellipsoid &ell,PostScript &ps,ostream &merc,ostream &merctext)
/* Computes approximations to the meridian of the ellipsoid. Projects
 * equidistant points along the meridian of the ellipsoid to the sphere,
 * and vice versa. Then takes the Fourier transform of the difference between
 * the projected points and the equidistant points. Finally writes the first
 * few terms of the Fourier transform to a file, for the transverse Mercator
 * projection to use.
 *
 * A record in the file looks like this:
 * 57 47 53 38 34 00       WGS84                   Name of ellipsoid
 * 05                      5                       Number of following numbers
 * 41 63 13 C5 B7 56 87 A8 10001965.729312733 m    Half-meridian of ellipsoid
 * 3F 41 79 C8 C4 00 05 FD 5.3331664094019538e-4   First harmonic of forward transform
 * 3E A0 40 BD 84 C3 4F 42 4.8437392188370177e-7   Second harmonic
 * 3E 0A 32 88 2A 9A 3F 9C 7.6244440379731101e-10  Third harmonic
 * 3D 7B 35 48 47 CD A3 5B 1.5466033666269329e-12  Fourth harmonic
 * 05                      5                       Number of following numbers
 * 41 63 16 7F 14 72 4F 2E 10007544.638953771 m    Half-meridian of sphere
 * BF 41 79 C9 3C 32 63 EC -5.333168595768023e-4   First harmonic of reverse transform
 * BE 64 2F 6B CF 26 8F 9B -3.7597937113734575e-8  Second harmonic
 * BD DD 48 D4 E3 AC 49 2E -1.0653638467792568e-10 Third harmonic
 * BD 43 66 F9 D6 AA BF A6 -1.3786127605631334e-13 Fourth harmonic
 */
{
  int i,j,nseg,sz1,decades;
  bool done=false;
  polyline forwardSpectrum,reverseSpectrum,frame;
  double minNonzero,minLog,maxLog;
  int goodForwardTerms,goodReverseTerms,forwardNoiseFloor,reverseNoiseFloor;
  int graphWidth;
  xy pnt;
  xyz belowPoint,abovePoint;
  vector<polyspiral> apx3,apx7,apxK;
  vector<array<double,2> > forwardLengths3,reverseLengths3;
  vector<array<double,2> > forwardLengths7,reverseLengths7;
  vector<array<double,2> > forwardLengthsK,reverseLengthsK;
  vector<double> forwardTransform3,reverseTransform3;
  vector<double> forwardTransform7,reverseTransform7;
  vector<double> forwardTransformK,reverseTransformK;
  vector<double> forwardTransform,reverseTransform,lastForwardTransform,lastReverseTransform;
  vector<double> forwardTm,reverseTm;
  array<double,3> forwardDifference,reverseDifference;
  frame.insert(xy(0,0));
  frame.insert(xy(3,0));
  frame.insert(xy(3,2));
  frame.insert(xy(0,2));
  ps.startpage();
  ps.comment(ell.getName());
  ps.setscale(0,0,EARTHRAD,EARTHRAD,0);
  for (i=0,nseg=1;i<5;i++,nseg*=7)
    apx7.push_back(psApprox(&ell,nseg));
  for (i=0,nseg=1;i<7;i++,nseg*=3)
    apx3.push_back(psApprox(&ell,nseg));
  apxK.push_back(psApprox(&ell,273));
  cout<<ell.getName()<<endl;
  for (i=0;i<apx3.size()-1;i++)
    cout<<setw(2)<<i<<setw(12)<<compareLengths(apx3[i],apx3[i+1])<<
          setw(12)<<apx3[i+1].length()-apx3[i].length()<<endl;
  for (i=0;i<apx7.size()-1;i++)
    cout<<setw(2)<<i<<setw(12)<<compareLengths(apx7[i],apx7[i+1])<<
          setw(12)<<apx7[i+1].length()-apx7[i].length()<<endl;
  // Draw 32 dots along the meridian, representing the input to the FFT
  forwardLengths3=projectForward(&ell,apx3[5],32);
  for (i=0;i<32;i++)
  {
    ps.setcolor(1,0,1);
    ps.circle(apx3.back().station(forwardLengths3[32][1]*(i+0.5)/32),5e4);
    ps.setcolor(0,0,1);
    ps.circle(apx3.back().station(forwardLengths3[i][1]),3e4);
  }
  // Draw a meridian of the ellipsoid from equator to pole
  ps.setcolor(0,0,0);
  ps.spline(apx3.back().approx3d(1e3));
  // Draw 26 tickmarks, separating the meridian into 27 parts, each 3+1/3° lat
  for (i=1;i<27;i++)
  {
    belowPoint=ell.geoc(M_PI*i/54,0.,-1e5)-ell.getCenter();
    abovePoint=ell.geoc(M_PI*i/54,0., 1e5)-ell.getCenter();
    ps.line2p(xy(belowPoint.getx(),belowPoint.getz()),xy(abovePoint.getx(),abovePoint.getz()));
  }
  for (i=0,nseg=1;i<24 && !done;i++,nseg*=2)
  {
    forwardLengths3=projectForward(&ell,apx3[5],nseg);
    reverseLengths3=projectBackward(&ell,apx3[5],nseg);
    forwardTransform3=fft(exeutheicity(forwardLengths3));
    reverseTransform3=fft(exeutheicity(reverseLengths3));
    forwardLengths7=projectForward(&ell,apx7[3],nseg);
    reverseLengths7=projectBackward(&ell,apx7[3],nseg);
    forwardTransform7=fft(exeutheicity(forwardLengths7));
    reverseTransform7=fft(exeutheicity(reverseLengths7));
    forwardLengthsK=projectForward(&ell,apxK[0],nseg);
    reverseLengthsK=projectBackward(&ell,apxK[0],nseg);
    forwardTransformK=fft(exeutheicity(forwardLengthsK));
    reverseTransformK=fft(exeutheicity(reverseLengthsK));
    forwardTransform.clear();
    reverseTransform.clear();
    for (j=0;j<forwardTransform3.size();j++)
    {
      forwardTransform.push_back(median(forwardTransform3[j],forwardTransform7[j],forwardTransformK[j]));
      reverseTransform.push_back(median(reverseTransform3[j],reverseTransform7[j],reverseTransformK[j]));
    }
    if (lastForwardTransform.size())
    {
      forwardDifference=compareTransforms(lastForwardTransform,forwardTransform);
      reverseDifference=compareTransforms(lastReverseTransform,reverseTransform);
      cout<<setw(2)<<i<<setw(14)<<forwardDifference[0]<<setw(12)<<forwardDifference[1]<<setw(12)<<forwardDifference[2];
      cout<<setw(14)<<reverseDifference[0]<<setw(12)<<reverseDifference[1]<<setw(12)<<reverseDifference[2]<<endl;
      done=forwardDifference[0]<3.4*forwardDifference[1] && reverseDifference[0]<3.4*reverseDifference[1];
      goodForwardTerms=goodReverseTerms=0;
      forwardNoiseFloor=reverseNoiseFloor=0;
      sz1=lastForwardTransform.size()+1;
      for (j=0;j<lastForwardTransform.size();j++)
      {
	if (fabs(forwardTransform[j]-lastForwardTransform[j])<
	    fabs(forwardTransform[j]+lastForwardTransform[j])/49152 && goodForwardTerms>=j)
	  goodForwardTerms++;
	if (fabs(reverseTransform[j]-lastReverseTransform[j])<
	    fabs(reverseTransform[j]+lastReverseTransform[j])/49152 && goodReverseTerms>=j)
	  goodReverseTerms++;
	// There's a spike at 486, which is 243*2. Ignore spikes in noise floor past 243.
	if (fabs(forwardTransform[j])>sz1/(i+1)*forwardDifference[2])
	  forwardNoiseFloor=j+1;
	if (fabs(reverseTransform[j])>sz1/(i+1)*reverseDifference[2])
	  reverseNoiseFloor=j+1;
      }
      cout<<"Forward "<<goodForwardTerms<<" good, noise "<<forwardNoiseFloor<<"   ";
      cout<<"Reverse "<<goodReverseTerms<<" good, noise "<<reverseNoiseFloor<<endl;
      if (goodForwardTerms<forwardNoiseFloor-1 || goodReverseTerms<reverseNoiseFloor-1)
	done=false;
    }
    lastForwardTransform=forwardTransform;
    lastReverseTransform=reverseTransform;
  }
  for (i=0;i<0;i++)
    cout<<setw(2)<<i+1<<setw(12)<<forwardTransform[i]<<setw(12)<<reverseTransform[i]<<endl;
  ps.endpage();
  ps.startpage();
  ps.setscale(0,0,3,2,0);
  minNonzero=INFINITY;
  for (i=0;i<forwardTransform.size();i++)
  {
    if (fabs(forwardTransform[i])<minNonzero && forwardTransform[i]!=0)
      minNonzero=fabs(forwardTransform[i]);
    if (fabs(reverseTransform[i])<minNonzero && reverseTransform[i]!=0)
      minNonzero=fabs(reverseTransform[i]);
  }
  minLog=INFINITY;
  maxLog=-INFINITY;
  minNonzero/=65536;
  // Comment out the next line to see all the noise floor
  graphWidth=max(forwardNoiseFloor,reverseNoiseFloor);
  if (graphWidth==0)
    graphWidth=forwardTransform.size();
  for (i=0;i<graphWidth;i++)
  {
    if (log(fabs(forwardTransform[i])+minNonzero)<minLog)
      minLog=log(fabs(forwardTransform[i])+minNonzero);
    if (log(fabs(forwardTransform[i])+minNonzero)>maxLog)
      maxLog=log(fabs(forwardTransform[i])+minNonzero);
    if (log(fabs(reverseTransform[i])+minNonzero)<minLog)
      minLog=log(fabs(reverseTransform[i])+minNonzero);
    if (log(fabs(reverseTransform[i])+minNonzero)>maxLog)
      maxLog=log(fabs(reverseTransform[i])+minNonzero);
  }
  minLog=floor(minLog/log(10))*log(10);
  maxLog=ceil (maxLog/log(10))*log(10);
  for (i=0;i<graphWidth;i++)
  {
    pnt=xy(3.*(i+1)/graphWidth,2*(log(fabs(forwardTransform[i])+minNonzero)-minLog)/(maxLog-minLog));
    forwardSpectrum.insert(pnt);
    ps.setcolor(0,0,1);
    ps.circle(pnt,0.02);
    pnt=xy(3.*(i+1)/graphWidth,2*(log(fabs(reverseTransform[i])+minNonzero)-minLog)/(maxLog-minLog));
    reverseSpectrum.insert(pnt);
    ps.setcolor(1,0,0);
    ps.circle(pnt,0.02);
  }
  ps.setcolor(0,0,0);
  decades=rint((maxLog-minLog)/log(10));
  for (i=0;i<=decades;i++)
  {
    ps.write(xy(3.1,i*2./decades),ldecimal(exp(minLog)*pow(10,i),exp(minLog)*pow(10,i-1)));
    ps.startline();
    ps.lineto(xy(3,i*2./decades));
    ps.lineto(xy(3.1,i*2./decades));
    ps.endline();
  }
  forwardSpectrum.open();
  reverseSpectrum.open();
  ps.spline(frame.approx3d(1e-2));
  ps.spline(forwardSpectrum.approx3d(1e-2));
  ps.spline(reverseSpectrum.approx3d(1e-2));
  ps.endpage();
  writeustring(merc,ell.getName());
  merctext<<ell.getName()<<'\n';
  merctext<<"Eccentricity "<<ldecimal(ell.eccentricity())<<'\n';
  writegeint(merc,forwardNoiseFloor+1);
  writebedouble(merc,forwardLengths3.back()[1]);
  merctext<<ldecimal(forwardLengths3.back()[1])<<'\n';
  forwardTm.push_back(forwardLengths3.back()[1]);
  for (i=0;i<forwardNoiseFloor;i++)
  {
    writebedouble(merc,forwardTransform[i]);
    merctext<<ldecimal(forwardTransform[i])<<'\n';
    forwardTm.push_back(forwardTransform[i]);
  }
  writegeint(merc,reverseNoiseFloor+1);
  writebedouble(merc,reverseLengths3.back()[1]);
  merctext<<"--------\n"<<ldecimal(reverseLengths3.back()[1])<<'\n';
  reverseTm.push_back(reverseLengths3.back()[1]);
  for (i=0;i<reverseNoiseFloor;i++)
  {
    writebedouble(merc,reverseTransform[i]);
    merctext<<ldecimal(reverseTransform[i])<<'\n';
    reverseTm.push_back(reverseTransform[i]);
  }
  ell.setTmCoefficients(forwardTm,reverseTm);
  drawKrugerize(ell,ps,false,forwardNoiseFloor+reverseNoiseFloor);
  drawKrugerize(ell,ps,true,forwardNoiseFloor+reverseNoiseFloor);
  plotErrorPeters(ell,ps,merctext);
  merctext<<"========\n";
}

void calibrate()
{
  int i,sz=32;
  vector<double> input,output;
  for (i=0;i<sz;i++)
    input.push_back(sin(DEG180/(2*sz)*(2*i+1)));
  output=fft(input);
  cout<<output[0]<<endl;
}

int main(int argc, char *argv[])
{
  int i;
  PostScript ps;
  ofstream merc("transmer.dat",ios::binary);
  ofstream merctext("transmer.txt");
  for (i=1;i<argc;i++)
    args.push_back(argv[i]);
  cout<<"Transmer, part of Bezitopo version "<<VERSION<<" © "<<COPY_YEAR<<" Pierre Abbat\n"
      <<"Distributed under GPL v3 or later. This is free software with no warranty."<<endl;
  header(merc);
  ps.open("transmer.ps");
  ps.setpaper(papersizes["A4 landscape"],0);
  ps.prolog();
  for (i=0;i<countEllipsoids();i++)
    doEllipsoid(getEllipsoid(i),ps,merc,merctext);
  ps.trailer();
  ps.close();
  calibrate();
  destroyPlans();
  return 0;
}
