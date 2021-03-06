//------------------------------------------------------------------------------
//
// Name:       vadd_abc.cpp
// 
// Purpose:    Elementwise addition of three vectors at a time in a chain (R=A+B+C)
//
// HISTORY:    Initial version based on vadd.c, written by Tim Mattson, June 2011
//             Ported to C++ Wrapper API by Benedict Gaster, September 2011
//             Updated to C++ Wrapper API v1.2 by Tom Deakin and Simon McIntosh-Smith, October 2012
//             Ported back to C by Tom Deakin, July 2013
//             
//------------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#include <unistd.h>
#else
#include <CL/cl.h>
#endif

//pick up device type from compiler command line or from 
//the default type
#ifndef DEVICE
#define DEVICE CL_DEVICE_TYPE_DEFAULT
#endif

extern int output_device_info(cl_device_id );
char* err_code (cl_int);

//------------------------------------------------------------------------------

#define TOL    (0.001)   // tolerance used in floating point comparisons
#define LENGTH (1024)    // length of vectors a, b, and c

//------------------------------------------------------------------------------
//
// kernel:  vadd  
//
// Purpose: Compute the elementwise sum c = a+b
// 
// input: a, b and c float vectors of length count
//
// output: r float vector of length count holding the sum a + b + c
//
 
const char *KernelSource = "\n" \
"__kernel void vadd(                                                    \n" \
"   __global float* a,                                                  \n" \
"   __global float* b,                                                  \n" \
"   __global float* c,                                                  \n" \
"   __global float* r,                                                  \n" \
"   const unsigned int count)                                           \n" \
"{                                                                      \n" \
"   int i = get_global_id(0);                                           \n" \
"   if(i < count)                                                       \n" \
"       r[i] = a[i] + b[i] + c[i];                                      \n" \
"}                                                                      \n" \
"\n";

//------------------------------------------------------------------------------


int main(int argc, char** argv)
{
    int          err;               // error code returned from OpenCL calls
    float        h_a[LENGTH];       // a vector 
    float        h_b[LENGTH];       // b vector 
    float        h_c[LENGTH];       // c vector
    float        h_r[LENGTH];       // r vector (result)
    unsigned int correct;           // number of correct results  

    size_t global;                  // global domain size  

    cl_device_id     device_id;     // compute device id 
    cl_context       context;       // compute context
    cl_command_queue commands;      // compute command queue
    cl_program       program;       // compute program
    cl_kernel        ko_vadd;       // compute kernel
    
    cl_mem d_a;                     // device memory used for the input  a vector
    cl_mem d_b;                     // device memory used for the input  b vector
    cl_mem d_c;                     // device memory used for the input  c vector
    cl_mem d_r;                     // device memory used for the output r vector
 
    // Fill vectors a and b with random float values
    int i = 0;
    int count = LENGTH;
    for(i = 0; i < count; i++){
        h_a[i] = rand() / (float)RAND_MAX;
        h_b[i] = rand() / (float)RAND_MAX;
        h_c[i] = rand() / (float)RAND_MAX;
    }
    
    // Set up platform and GPU device

    cl_uint numPlatforms;

    // Find number of platforms
    err = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (err != CL_SUCCESS || numPlatforms <= 0)
    {
        printf("Error: Failed to find a platform!\n%s\n",err_code(err));
        return EXIT_FAILURE;
    }

    // Get all platforms
    cl_platform_id Platform[numPlatforms];
    err = clGetPlatformIDs(numPlatforms, Platform, NULL);
    if (err != CL_SUCCESS || numPlatforms <= 0)
    {
        printf("Error: Failed to get the platform!\n%s\n",err_code(err));
        return EXIT_FAILURE;
    }

    // Secure a GPU
    for (i = 0; i < numPlatforms; i++)
    {
        err = clGetDeviceIDs(Platform[i], DEVICE, 1, &device_id, NULL);
        if (err == CL_SUCCESS)
        {
            break;
        }
    }

    if (device_id == NULL)
    {
        printf("Error: Failed to create a device group!\n%s\n",err_code(err));
        return EXIT_FAILURE;
    }

    err = output_device_info(device_id);
  
    // Create a compute context 
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (!context)
    {
        printf("Error: Failed to create a compute context!\n%s\n", err_code(err));
        return EXIT_FAILURE;
    }

    // Create a command queue
    commands = clCreateCommandQueue(context, device_id, 0, &err);
    if (!commands)
    {
        printf("Error: Failed to create a command commands!\n%s\n", err_code(err));
        return EXIT_FAILURE;
    }

    // Create the compute program from the source buffer
    program = clCreateProgramWithSource(context, 1, (const char **) & KernelSource, NULL, &err);
    if (!program)
    {
        printf("Error: Failed to create compute program!\n%s\n", err_code(err));
        return EXIT_FAILURE;
    }

    // Build the program  
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        size_t len;
        char buffer[2048];

        printf("Error: Failed to build program executable!\n%s\n", err_code(err));
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        printf("%s\n", buffer);
        return EXIT_FAILURE;
    }

    // Create the compute kernel from the program 
    ko_vadd = clCreateKernel(program, "vadd", &err);
    if (!ko_vadd || err != CL_SUCCESS)
    {
        printf("Error: Failed to create compute kernel!\n%s\n", err_code(err));
        return EXIT_FAILURE;
    }

    // Create the input (a, b, e, g) arrays in device memory
    // NB: we copy the host pointers here too
    d_a  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,  sizeof(float) * count, &h_a, NULL);
    d_b  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,  sizeof(float) * count, &h_b, NULL);
    d_c  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,  sizeof(float) * count, &h_c, NULL);
    
    // Create the output arrays in device memory
    d_r  = clCreateBuffer(context,  CL_MEM_READ_WRITE, sizeof(float) * count, NULL, NULL);
    
    if (!d_a || !d_b || !d_c || !d_r)
    {
        printf("Error: Failed to allocate device memory!\n");
        exit(1);
    }    

    // Enqueue kernel - first time
    // Set the arguments to our compute kernel
    err  = clSetKernelArg(ko_vadd, 0, sizeof(cl_mem), &d_a);
    err |= clSetKernelArg(ko_vadd, 1, sizeof(cl_mem), &d_b);
    err |= clSetKernelArg(ko_vadd, 2, sizeof(cl_mem), &d_c);
    err |= clSetKernelArg(ko_vadd, 3, sizeof(cl_mem), &d_r);
    err |= clSetKernelArg(ko_vadd, 4, sizeof(unsigned int), &count);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to set kernel arguments!\n");
        exit(1);
    }
	
    // Execute the kernel over the entire range of our 1d input data set
    // letting the OpenCL runtime choose the work-group size
    global = count;
    err = clEnqueueNDRangeKernel(commands, ko_vadd, 1, NULL, &global, NULL, 0, NULL, NULL);
    if (err)
    {
        printf("Error: Failed to execute kernel!\n%s\n", err_code(err));
        return EXIT_FAILURE;
    }

    // Read back the result from the compute device
    err = clEnqueueReadBuffer( commands, d_r, CL_TRUE, 0, sizeof(float) * count, h_r, 0, NULL, NULL );  
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to read output array!\n%s\n", err_code(err));
        exit(1);
    }
    
    // Test the results
    correct = 0;
    float tmp;
    
    for(i = 0; i < count; i++)
    {
        tmp = h_a[i] + h_b[i] + h_c[i];              // assign element i of a+b+c to tmp
        tmp -= h_r[i];                               // compute deviation of expected and output result
        if(tmp*tmp < TOL*TOL)                        // correct if square deviation is less than tolerance squared
            correct++;
        else {
            printf(" tmp %f h_a %f h_b %f h_c %f h_r %f \n",tmp, h_a[i], h_b[i], h_c[i], h_r[i]);
        }
    }
    
    // summarize results
    printf("R = A+B+C:  %d out of %d results were correct.\n", correct, count);
    
    // cleanup then shutdown
    clReleaseMemObject(d_a);
    clReleaseMemObject(d_b);
    clReleaseMemObject(d_c);
    clReleaseMemObject(d_r);
    clReleaseProgram(program);
    clReleaseKernel(ko_vadd);
    clReleaseCommandQueue(commands);
    clReleaseContext(context);

    return 0;
}

