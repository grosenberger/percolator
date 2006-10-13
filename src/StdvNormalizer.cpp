#include <vector>
#include <iostream>
#include <math.h>
#include <vector>
#include <string>
using namespace std;
#include "DataSet.h"
#include "Normalizer.h"
#include "StdvNormalizer.h"
#include "IsoChargeSet.h"

StdvNormalizer::StdvNormalizer()
{
  avg.resize(DataSet::getNumFeatures(),0.0);
  stdv.resize(DataSet::getNumFeatures(),0.0);
}

StdvNormalizer::~StdvNormalizer()
{
}

void StdvNormalizer::normalize(const double *in,double* out){
  for (int ix=0;ix<DataSet::getNumFeatures();ix++) {
  	out[ix]=(in[ix]-avg[ix])/stdv[ix];
  }
}

void StdvNormalizer::unnormalizeweight(const double *in,double* out){
  double sum = 0;
  int i=0;
  for (;i<DataSet::getNumFeatures();i++) {
  	out[i]=in[i]/stdv[i];
  	sum=avg[i]/stdv[i];
  }
  out[i]=in[i]-sum;
}

void StdvNormalizer::normalizeweight(const double *in,double* out){
  double sum = 0;
  int i=0;
  for (;i<DataSet::getNumFeatures();i++) {
  	out[i]=in[i]*stdv[i];
  	sum=avg[i]/stdv[i];
  }
  out[i]=in[i]+sum;
}

void StdvNormalizer::setSet(IsoChargeSet * set){
  double n=0.0;
  int setPos=0;
  int ixPos=-1;
  const double * features;
  int ix;
  for (ix=0;ix<DataSet::getNumFeatures();ix++) {
    avg[ix]=0.0;
    stdv[ix]=0.0;
  }
  while((features=set->getNext(setPos,ixPos))!=NULL) {
	n++;
	for (ix=0;ix<DataSet::getNumFeatures();ix++) {
	  avg[ix]+=features[ix];
	}
  }
  for (ix=0;ix<DataSet::getNumFeatures();ix++) {
  	if (n>0.0)
     avg[ix]/=n;
  }
  setPos=0;
  ixPos=-1;
  while((features=set->getNext(setPos,ixPos))!=NULL) {
    for (ix=0;ix<DataSet::getNumFeatures();ix++) {
      double d = features[ix]-avg[ix];
      stdv[ix]+=d*d;
    }
  }
  for (ix=0;ix<DataSet::getNumFeatures();ix++) {
    if (stdv[ix]<=0 || n==0) {
      stdv[ix]=1.0;
    } else {
  	  stdv[ix]=sqrt(stdv[ix]/n);
    }
//  	cout << ix << " " << avg[ix] << " " << stdv[ix] << "\n";
  }
}