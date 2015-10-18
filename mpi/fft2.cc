#include "Array.h"
#include "mpifftw++.h"
#include "utils.h"
#include "mpiutils.h"

using namespace std;
using namespace fftwpp;
using namespace Array;

inline void init(Complex *f, split d) 
{
  unsigned int c=0;
  for(unsigned int i=0; i < d.x; ++i) {
    unsigned int ii=d.x0+i;
    for(unsigned int j=0; j < d.Y; j++) {
      f[c++]=Complex(ii,j);
    }
  }
}

unsigned int outlimit=100;

int main(int argc, char* argv[])
{
  int retval = 0; // success!

#ifndef __SSE2__
  fftw::effort |= FFTW_NO_SIMD;
#endif

  // Number of iterations.
  unsigned int N0=10000000;
  unsigned int N=0;
  unsigned int nx=4;
  unsigned int ny=4;
  int divisor=0; // Test for best block divisor
  int alltoall=-1; // Test for best alltoall routine
  
  bool quiet=false;
  bool test=false;
  
#ifdef __GNUC__ 
  optind=0;
#endif  
  for (;;) {
    int c = getopt(argc,argv,"hN:a:m:s:x:y:n:T:qt");
    if (c == -1) break;
                
    switch (c) {
      case 0:
        break;
      case 'a':
        divisor=atoi(optarg);
        break;
      case 'N':
        N=atoi(optarg);
        break;
      case 'm':
        nx=ny=atoi(optarg);
        break;
      case 's':
        alltoall=atoi(optarg);
        break;
      case 'x':
        nx=atoi(optarg);
        break;
      case 'y':
        ny=atoi(optarg);
        break;
      case 'n':
        N0=atoi(optarg);
        break;
      case 'T':
        fftw::maxthreads=atoi(optarg);
        break;
      case 'q':
        quiet=true;
        break;
      case 't':
        test=true;
        break;
      case 'h':
      default:
        usage(2);
        usageTranspose();
        exit(1);
    }
  }

  int provided;
  MPI_Init_thread(&argc,&argv,MPI_THREAD_MULTIPLE,&provided);

  if(ny == 0) ny=nx;

  if(N == 0) {
    N=N0/nx/ny;
    if(N < 10) N=10;
  }
  
  int fftsize=min(nx,ny);

  MPIgroup group(MPI_COMM_WORLD,fftsize);

  if(group.size > 1 && provided < MPI_THREAD_FUNNELED)
    fftw::maxthreads=1;

  if(!quiet && group.rank == 0) {
    cout << "provided: " << provided << endl;
    cout << "fftw::maxthreads: " << fftw::maxthreads << endl;
  }
  
  if(!quiet && group.rank == 0) {
    cout << "Configuration: " 
	 << group.size << " nodes X " << fftw::maxthreads 
	 << " threads/node" << endl;
  }

  if(group.rank < group.size) { 
    bool main=group.rank == 0;
    if(!quiet && main) {
      cout << "N=" << N << endl;
      cout << "nx=" << nx << ", ny=" << ny << endl;
    } 

    split d(nx,ny,group.active);
  
    Complex *f=ComplexAlign(d.n);

    // Create instance of FFT
    fft2dMPI fft(d,f,mpiOptions(divisor,alltoall));

    if(!quiet && group.rank == 0)
      cout << "Initialized after " << seconds() << " seconds." << endl;    

    if(test) {
      init(f,d);

      if(!quiet && nx*ny < outlimit) {
	if(main) cout << "\nDistributed input:" << endl;
	show(f,d.x,ny,group.active);
      }

      size_t align=sizeof(Complex);
      array2<Complex> flocal(nx,ny,align);
      fft2d localForward(-1,flocal);
      fft2d localBackward(1,flocal);
      gatherx(f, flocal(), d, 1, group.active);

      if(!quiet && main) {
	cout << "\nGathered input:\n" << flocal << endl;
      }

      fft.Forward(f);

      if(!quiet && nx*ny < outlimit) {
      	if(main) cout << "\nDistributed output:" << endl;
      	show(f,nx,d.y,group.active);
      }
      
      array2<Complex> fgather(nx,ny,align);
      gathery(f, fgather(), d, 1, group.active);

      MPI_Barrier(group.active);
      if(main) {
	localForward.fft(flocal);
	if(!quiet) {
	  cout << "\nGathered output:\n" << fgather << endl;
	  cout << "\nLocal output:\n" << flocal << endl;
	}
        double maxerr=0.0, norm=0.0;
        unsigned int stop=d.X*d.Y;
        for(unsigned int i=0; i < stop; i++) {
          maxerr=std::max(maxerr,abs(fgather(i)-flocal(i)));
          norm=std::max(norm,abs(flocal(i)));
        }
	cout << "max error: " << maxerr << endl;
        if(maxerr > 1e-12*norm) {
	  cerr << "CAUTION: max error is LARGE!" << endl;
	  retval += 1;
	}
      }

      fft.Backward(f);
      fft.Normalize(f);

      if(!quiet && nx*ny < outlimit) {
      	if(main) cout << "\nDistributed inverse:" << endl;
      	show(f,d.x,ny,group.active);
      }

      gatherx(f, fgather(), d, 1, group.active);
      MPI_Barrier(group.active);
      if(main) {
	localBackward.fftNormalized(flocal);
	if(!quiet) {
	  cout << "\nGathered inverse:\n" << fgather << endl;
	  cout << "\nLocal inverse:\n" << flocal << endl;
	}
        retval += checkerror(flocal(),fgather(),d.X*d.Y);
      }

      if(!quiet && group.rank == 0) {
        cout << endl;
        if(retval == 0)
          cout << "pass" << endl;
        else
          cout << "FAIL" << endl;
      }  
  
    } else {
      if(N > 0) {
	double *T=new double[N];
	for(unsigned int i=0; i < N; ++i) {
	  init(f,d);
	  seconds();
	  fft.Forward(f);
	  fft.Backward(f);
	  fft.Normalize(f);
	  T[i]=seconds();
	}    
	if(main) timings("FFT timing:",nx,T,N);
	delete [] T;
      }
    }

    deleteAlign(f);
  }
  
  MPI_Finalize();

  return retval;
}
