#include "convolution.h"
#include "explicit.h"
#include "direct.h"
#include "utils.h"
#include <unistd.h>

using namespace std;
using namespace fftwpp;

bool Direct=false, Implicit=true, Explicit=false, Test=false;

unsigned int A, B; // Number of inputs and outputs
bool compact=false;

// Pair-wise binary multiply for even A and B=1.
// NB: example function, not optimised or threaded.
void mymult(double ** F, unsigned int m, unsigned int threads)
{
  for(unsigned int i=0; i < m; ++i) {
    F[0][i] *= F[1][i];
    for(unsigned int a=2; a < A; a += 2) 
      F[0][i] += F[a][i]*F[a+1][i];
  }
}

// Pair-wise binary multiply for even A.
// All of the B outputs are identical.
// NB: example function, not optimised or threaded.
void mymultB(double ** F, unsigned int m, unsigned int threads)
{
  mymult(F,m,threads);
  // copy output
  for(unsigned int i=0; i < m; ++i)
    for(unsigned int b=0; b < B; b++) 
      F[b][i]=F[0][i];
}

inline void init(Complex **F, unsigned int m,  unsigned int A) 
{
  if(!compact) 
    for(unsigned int i=0; i < A; ++i)
      F[i][m]=0.0;
                 
  const Complex I(0.0,1.0);
  const double iE=exp(1.0);
  const double iF=sqrt(3.0);
  const double iG=sqrt(5.0);
  
  if(A % 2 != 0 && A != 1) {
    cerr << "A=" << A << " is not yet implemented" << endl; 
    exit(1);
  }
  if(A == 1) {
    Complex *f=F[0];
    for(unsigned int k=0; k < m; ++k) {
      f[k]=iF*pow(iE,k*I);
    }
  }
  if(A % 2 == 0) {
    unsigned int M=A/2;
    double factor=1.0/sqrt((double) M);
    for(unsigned int s=0; s < M; s++) {
      double ffactor=(1.0+s)*factor;
      double gfactor=1.0/(1.0+s)*factor;
      Complex *fs=F[s];
      Complex *gs=F[s+M];
      if(Test) {
	for(unsigned int k=0; k < m; k++) fs[k]=factor*iF*pow(iE,k*I);
	for(unsigned int k=0; k < m; k++) gs[k]=factor*iG*pow(iE,k*I);
	//    for(unsigned int k=0; k < m; k++) fs[k]=factor*iF*k;
	//    for(unsigned int k=0; k < m; k++) gs[k]=factor*iG*k;
      } else {
	fs[0]=1.0*ffactor;
	for(unsigned int k=1; k < m; k++) fs[k]=ffactor*Complex(k,k+1);
	gs[0]=2.0*gfactor;
	for(unsigned int k=1; k < m; k++) gs[k]=gfactor*Complex(k,2*k+1);
      }
    }
  }
}

void test(unsigned int m, unsigned int M, Complex *h0)
{
  Complex *h=ComplexAlign(m);
  double error=0.0;
  cout << endl;
  double norm=0.0;
  long long mm=m;

  const Complex I(0.0,1.0);
  const double E=exp(1.0);
  const double F=sqrt(3.0);
  const double G=sqrt(5.0);
  
  for(long long k=0; k < mm; k++) {
    h[k]=F*G*(2*mm-1-k)*pow(E,k*I);
    //      h[k]=F*G*(4*m*m*m-6*(k+1)*m*m+(6*k+2)*m+3*k*k*k-3*k)/6.0;
    error += abs2(h0[k]-h[k]);
    norm += abs2(h[k]);
  }
  if(norm > 0) error=sqrt(error/norm);
  cout << "error=" << error << endl;
  if (error > 1e-12)
    cerr << "Caution! error=" << error << endl;
  deleteAlign(h);

}

int main(int argc, char* argv[])
{
  fftw::maxthreads=get_max_threads();

  unsigned int N=0; // Number of iterations.
  unsigned int N0=10000000; // Nominal number of iterations
  unsigned int m=11; // Problem size
  unsigned int M=1;
  
  A=2; // Number of inputs
  B=1; // Number of outputs
  unsigned int Bcheck=0; // Which output to check

  unsigned int stats=0; // Type of statistics used in timing test.


#ifndef __SSE2__
  fftw::effort |= FFTW_NO_SIMD;
#endif  
  
#ifdef __GNUC__	
  optind=0;
#endif	
  for (;;) {
    int c = getopt(argc,argv,"hdeiptM:A:B:b:N:m:n:T:S:X:");
    if (c == -1) break;
		
    switch (c) {
      case 0:
        break;
      case 'd':
        Direct=true;
        break;
      case 'e':
        Explicit=true;
        Implicit=false;
        break;
      case 'i':
        Implicit=true;
        Explicit=false;
        break;
      case 'p':
        break;
      case 'M':
        M=atoi(optarg);
	A=2*M; // Number of independent inputs
        break;
      case 'A':
        A=atoi(optarg);
        break;
      case 'B':
        B=atoi(optarg);
        break;
      case 'b':
        Bcheck=atoi(optarg);
        break;
      case 'N':
        N=atoi(optarg);
        break;
      case 't':
        Test=true;
        break;
      case 'm':
        m=atoi(optarg);
        break;
      case 'n':
        N0=atoi(optarg);
        break;
      case 'T':
        fftw::maxthreads=max(atoi(optarg),1);
        break;
      case 'S':
        stats=atoi(optarg);
        break;
      case 'X':
        compact=atoi(optarg) == 0;
        break;
      case 'h':
      default:
        usage(1,true,true,false);
	usageA();
	usageB();
	exit(1);
    }
  }

  unsigned int n=hpadding(m);
  
  cout << "n=" << n << endl;
  cout << "m=" << m << endl;
  
  if(N == 0) {
    N=N0/n;
    if(N < 10) N=10;
  }
  cout << "N=" << N << endl;
  
  unsigned int np=Explicit ? n/2+1 : m+!compact;

  // explicit and direct convolutions are only implemented for binary
  // convolutions.
  if(!Implicit) 
    A=2;
  
  Complex *f=ComplexAlign(A*np);
  Complex **F=new Complex *[A];
  for(unsigned int s=0; s < A; ++s)
    F[s]=f+s*np;

  Complex *h0=NULL;
  if(Test || Direct) h0=ComplexAlign(m);

  double* T=new double[N];

  if(Implicit) {
    ImplicitHConvolution C(m,compact,A,B);
    cout << "threads=" << C.Threads() << endl << endl;

    if (A%2 != 0) {
      cerr << "A=" << A << " is not yet implemented" << endl; 
      exit(1);
    }
    
    realmultiplier *mult;
    if(B == 1) {
      switch(A) {
      case 2: mult=multbinary; break;
      case 4: mult=multbinary2; break;
      default: mult=mymult;
      }
    } else
      mult=mymultB;
    
    for(unsigned int i=0; i < N; ++i) {
      init(F,m,A);
      seconds();
      C.convolve(F,mult);
//      C.convolve(F[0],G[0]);
      T[i]=seconds();
    }

    timings("Implicit",m,T,N,stats);
    
    if(m < 100) 
      for(unsigned int i=0; i < m; i++) cout << F[Bcheck][i] << endl;
    else cout << f[0] << endl;
    if(Test || Direct)
      for(unsigned int i=0; i < m; i++) h0[i]=F[Bcheck][i];
    
  }
  
  if(Explicit) {
    ExplicitHConvolution C(n,m,f);
    for(unsigned int i=0; i < N; ++i) {
      init(F,m,2);
      seconds();
      C.convolve(F[0],F[1]);
      T[i]=seconds();
    }

    cout << endl;
    timings("Explicit",m,T,N);

    if(m < 100) 
      for(unsigned int i=0; i < m; i++) cout << f[i] << endl;
    else cout << f[0] << endl;
    if(Test || Direct) 
      for(unsigned int i=0; i < m; i++) h0[i]=f[i];
  }
  
  if(Direct) {
    DirectHConvolution C(m);
    init(F,m,2);
    Complex *h=ComplexAlign(m);
    seconds();
    C.convolve(h,F[0],F[1]);
    T[0]=seconds();
    
    cout << endl;
    timings("Direct",m,T,1);

    if(m < 100) 
      for(unsigned int i=0; i < m; i++) cout << h[i] << endl;
    else cout << h[0] << endl;

    { // compare implicit or explicit version with direct verion:
      double error=0.0;
      cout << endl;
      double norm=0.0;
      for(unsigned long long k=0; k < m; k++) {
	error += abs2(h0[k]-h[k]);
	norm += abs2(h[k]);
      }
      if(norm > 0) error=sqrt(error/norm);
      cout << "error=" << error << endl;
      if (error > 1e-12) cerr << "Caution! error=" << error << endl;
    }

    if(Test) 
      for(unsigned int i=0; i < m; i++) h0[i]=h[i];
    deleteAlign(h);
  }

  if(Test) 
    test(m,M,h0);
  
  delete [] T;
  deleteAlign(f);
  delete [] F;
  
  return 0;
}
