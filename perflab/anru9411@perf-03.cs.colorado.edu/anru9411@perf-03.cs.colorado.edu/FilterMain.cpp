#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"
#include <omp.h> 

using namespace std;

#include "rtdsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

struct Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output) {
	
long long cycStart, cycStop;

//Optimize use of function calls by creating local variables used by the registers outside of loops.
//Reading/writing registers is much faster than reading/writing memory.
    
    
//Procedure calls are expensive
//Bounds checking is expensive
//Memory references are expensive
 
const short columns = input -> width - 1; 
const short rows = input -> height - 1; 
register int divisor = filter -> getDivisor();
short i = 0;
short j = 0;
short c;
short p;
short r;
 
//These call get function outside the loops, increasing performance
int data1 = filter -> get(0,0);
int data2 = filter -> get(0,1);
int data3 = filter -> get(0,2);
    
int data4 = filter -> get(1,0);
int data5 = filter -> get(1,1);
int data6 = filter -> get(1,2);
    
int data7 = filter -> get(2,0);
int data8 = filter -> get(2,1);
int data9 = filter -> get(2,2);
    
cycStart = rdtscll();
    
output -> width = input -> width;
output -> height = input -> height;
  
//Utilize multiple processors by running these three nested loops in parallel
#pragma omp parallel for collapse(3)
	for(p = 0; p < 3; p++)
    {
        for(r = 1; r < rows; r++)
			{
                //Unroll column loop 3 times
				for(c= 1; c < columns; c+=3)
				{
				
                //Use accumulators to hold value at each unroll
				register int acc1 = output -> color[p][r][c];
				register int acc2 = output -> color[p][r][c+1];
				register int acc3 = output -> color[p][r][c+2];
				
                //Unroll 1
				acc1 = (acc1 + (input -> color[p][r + i - 1] [c + j - 1] * data1));
                acc1 = (acc1 + (input -> color[p][r + i - 1] [c + j]     * data2));
                acc1 = (acc1 + (input -> color[p][r + i - 1] [c + j + 1] * data3));
                acc1 = (acc1 + (input -> color[p][r + i]     [c + j - 1] * data4));
                acc1 = (acc1 + (input -> color[p][r + i]     [c + j]     * data5));
                acc1 = (acc1 + (input -> color[p][r + i]     [c + j + 1] * data6));
                acc1 = (acc1 + (input -> color[p][r + i + 1] [c + j - 1] * data7));
                acc1 = (acc1 + (input -> color[p][r + i + 1] [c + j]     * data8));
                acc1 = (acc1 + (input -> color[p][r + i + 1] [c + j + 1] * data9));
                
                //Unroll 2
                acc2 = (acc2 + (input -> color[p][r + i - 1] [c + j] 	 * data1));
                acc2 = (acc2 + (input -> color[p][r + i - 1] [c + j + 1] * data2));
                acc2 = (acc2 + (input -> color[p][r + i - 1] [c + j + 2] * data3));
                acc2 = (acc2 + (input -> color[p][r + i]     [c + j] 	 * data4));
                acc2 = (acc2 + (input -> color[p][r + i]     [c + j + 1] * data5));
                acc2 = (acc2 + (input -> color[p][r + i]     [c + j + 2] * data6));
                acc2 = (acc2 + (input -> color[p][r + i + 1] [c + j] 	 * data7));
                acc2 = (acc2 + (input -> color[p][r + i + 1] [c + j + 1] * data8));
                acc2 = (acc2 + (input -> color[p][r + i + 1] [c + j + 2] * data9));
                
                //Unroll 3
                acc3 = (acc3 + (input -> color[p][r + i - 1] [c + j + 1] * data1));
                acc3 = (acc3 + (input -> color[p][r + i - 1] [c + j + 2] * data2));
                acc3 = (acc3 + (input -> color[p][r + i - 1] [c + j + 3] * data3));
                acc3 = (acc3 + (input -> color[p][r + i]     [c + j + 1] * data4));
                acc3 = (acc3 + (input -> color[p][r + i]     [c + j + 2] * data5));
                acc3 = (acc3 + (input -> color[p][r + i]     [c + j + 3] * data6));
                acc3 = (acc3 + (input -> color[p][r + i + 1] [c + j + 1] * data7));
                acc3 = (acc3 + (input -> color[p][r + i + 1] [c + j + 2] * data8));
                acc3 = (acc3 + (input -> color[p][r + i + 1] [c + j + 3] * data9));
				
                
                
                //Logic to limit the use of division operator whenever possible
                    
                if ( divisor == 0 )
                {
                    acc1 = 0;
                    acc2 = 0;
                    acc3 = 0;
                    
                }
                    
                //Limit division
                else if ( divisor == 1 )
                {
                }
                    
                //Divide only if the divisor is not one or zero
                else
                {
                    acc1 = acc1 / divisor; 
                    acc2 = acc2 / divisor;
                    acc3 = acc3 / divisor;
                }
                if ( acc1 < 0 )
                {
                    acc1 = 0;
                }
                if ( acc1 > 255 )
                {
                    acc1 = 255;
                }
                if ( acc2 < 0 )
                {
                    acc2 = 0;
                }
                if ( acc2 > 255 )
                {
                    acc2 = 255;
                }
                if ( acc3 < 0 )
                {
                    acc3 = 0;
                }
                if ( acc3 > 255 )
                {
                    acc3 = 255;
                }
                
                output -> color[p][r][c] = acc1;
                output -> color[p][r][c+1] = acc2;
                output -> color[p][r][c+2] = acc3;
            }
        }
      
        
       /*
        for(c = 1; c < rows; c++)
        {
            //Unroll column loop 3 times
            for(r= 1; r < columns; r+=3)
            {
                
                //Use accumulators to hold value at each unroll
                register int acc1 = output -> color[p][r][c];
                register int acc2 = output -> color[p][r+1][c];
                register int acc3 = output -> color[p][r+2][c];
                
                //Unroll 1
                acc1 = (acc1 + (input -> color[p][r + i - 1] [c + j - 1] * data1));
                acc1 = (acc1 + (input -> color[p][r + i - 1] [c + j]     * data2));
                acc1 = (acc1 + (input -> color[p][r + i - 1] [c + j + 1] * data3));
                acc1 = (acc1 + (input -> color[p][r + i]     [c + j - 1] * data4));
                acc1 = (acc1 + (input -> color[p][r + i]     [c + j]     * data5));
                acc1 = (acc1 + (input -> color[p][r + i]     [c + j + 1] * data6));
                acc1 = (acc1 + (input -> color[p][r + i + 1] [c + j - 1] * data7));
                acc1 = (acc1 + (input -> color[p][r + i + 1] [c + j]     * data8));
                acc1 = (acc1 + (input -> color[p][r + i + 1] [c + j + 1] * data9));
                
                //Unroll 2
                acc2 = (acc2 + (input -> color[p][r + i] [c + j - 1] 	 * data1));
                acc2 = (acc2 + (input -> color[p][r + i] [c + j] * data2));
                acc2 = (acc2 + (input -> color[p][r + i] [c + j + 1] * data3));
                acc2 = (acc2 + (input -> color[p][r + i + 1]     [c + j - 1] 	 * data4));
                acc2 = (acc2 + (input -> color[p][r + i + 1]     [c + j] * data5));
                acc2 = (acc2 + (input -> color[p][r + i + 1]     [c + j + 1] * data6));
                acc2 = (acc2 + (input -> color[p][r + i + 2] [c + j - 1] 	 * data7));
                acc2 = (acc2 + (input -> color[p][r + i + 2] [c + j] * data8));
                acc2 = (acc2 + (input -> color[p][r + i + 2] [c + j + 1] * data9));
                
                //Unroll 3
                acc3 = (acc3 + (input -> color[p][r + i + 1] [c + j -1] * data1));
                acc3 = (acc3 + (input -> color[p][r + i + 1] [c + j] * data2));
                acc3 = (acc3 + (input -> color[p][r + i + 1] [c + j + 1] * data3));
                acc3 = (acc3 + (input -> color[p][r + i + 2]     [c + j - 1] * data4));
                acc3 = (acc3 + (input -> color[p][r + i + 2]     [c + j] * data5));
                acc3 = (acc3 + (input -> color[p][r + i + 2]     [c + j + 1] * data6));
                acc3 = (acc3 + (input -> color[p][r + i + 3] [c + j - 1] * data7));
                acc3 = (acc3 + (input -> color[p][r + i + 3] [c + j] * data8));
                acc3 = (acc3 + (input -> color[p][r + i + 3] [c + j + 1] * data9));
                
                
                
                //Logic to limit the use of division operator whenever possible
                
                if ( divisor == 0 )
                {
                    acc1 = 0;
                    acc2 = 0;
                    acc3 = 0;
                    
                }
                
                //Limit division
                else if ( divisor == 1 )
                {
                }
                
                //Divide only if the divisor is not one or zero
                else
                {
                    acc1 = acc1 / divisor;
                    acc2 = acc2 / divisor;
                    acc3 = acc3 / divisor;
                }
                if ( acc1 < 0 )
                {
                    acc1 = 0;
                }
                if ( acc1 > 255 )
                {
                    acc1 = 255;
                }
                if ( acc2 < 0 )
                {
                    acc2 = 0;
                }
                if ( acc2 > 255 )
                {
                    acc2 = 255;
                }
                if ( acc3 < 0 )
                {
                    acc3 = 0;
                }
                if ( acc3 > 255 )
                {
                    acc3 = 255;
                }
                
                output -> color[p][r][c] = acc1;
                output -> color[p][r+1][c] = acc2;
                output -> color[p][r+2][c] = acc3;
            }
        }  

        */
        
        
    }

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}
