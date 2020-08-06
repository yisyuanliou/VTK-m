//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2014 Sandia Corporation.
//  Copyright 2014 UT-Battelle, LLC.
//  Copyright 2014 Los Alamos National Security.
//
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================

#include <iostream>
#include <fstream>
#include <iterator>

#include <vtkm/worklet/FieldHistogram.h>
#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/testing/Testing.h>
#include <vtkm/worklet/GMMTraining.h>
#include <vtkm/worklet/KMeanPP.h>
//#include <vtkm/worklet/testing/bmp_image.h>
#include <vtkm/cont/Timer.h>
//#include <vtkm/worklet/VtkmTable.h>
//#include <vtkm/worklet/VtkmSQL.h>
#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/DataSetBuilderExplicit.h>
#include <vtkm/cont/DataSetBuilderRectilinear.h>
#include <vtkm/cont/DataSetBuilderUniform.h>
#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/io/writer/VTKDataSetWriter.h>
#include <vtkm/io/reader/VTKDataSetReader.h>

namespace{

typedef vtkm::Float64 Real;
const int VARs = 3;
const int nGauComps = 10;
const int SampleCubeSize = 250;
const int SampleSphereRadius = 100;
const int SampleSphereBand = 5;
const int maxEmIterations = 100;
const int maxKMeanppIterations = 100;

void Index1DTo3D( int idx, int& x, int& y, int& z )
{
    x = idx % SampleCubeSize;
    idx = (idx - x) / SampleCubeSize;
    y = idx % SampleCubeSize;
    z = (idx-y) / SampleCubeSize;
}

int Index3DTo1D( int x, int y, int z )
{
    return z*SampleCubeSize*SampleCubeSize + y*SampleCubeSize + x;
}


///////////////////////Important Data Structure ////////////////////////////////
// our GMM trining is designed to train multiple GMM at the same time,
// the number of samples for each GMM does not have to be the same.
// training Data: std::vector< vtkm::Vec<DType, VARs>> >  ex: trainData
// Samples coreespoding GMM ID: std::vector<int>   ex: gmmIds
// trainData, gmmIds, must have the same length
// e.g. trainData[i]'s coresspoding GMM id is gmmIds[i]
// All samples in trainData have same correspoding GMM ID are used to train single GMM
// The samples which have same correspoding GMM ID have to be put together in trainData
// Also, id in gmmIds have to start from 0 and monotonically increase only

void TestAyanGMM()
{
    std::string filePrefixPath = "./";
    //std::string filePrefixPath = "/home/chuc/caseywang777/";

    std::vector<float> gtData(SampleCubeSize*SampleCubeSize*SampleCubeSize);

    ////////////////////  Step 1 ////////////////////////////////
    /////create ground truth data (training data)
    for(int i=0; i< SampleCubeSize*SampleCubeSize*SampleCubeSize; i++ ){
        int x, y, z;
        Index1DTo3D(i, x, y, z);
        float distance = sqrt((x-SampleCubeSize/2)*(x-SampleCubeSize/2) + 
                                 (y-SampleCubeSize/2)*(y-SampleCubeSize/2) +
                                 (z-SampleCubeSize/2)*(z-SampleCubeSize/2) );
        if( fabs( distance - SampleSphereRadius ) <= SampleSphereBand ){
            if( x < SampleCubeSize/2 ) gtData[i] = 0;
            else gtData[i] = 100;
        }
        else gtData[i] = 50;
    }
    //Write ground truth to raw binary file for visualization
    std::ofstream fout(filePrefixPath + "gtData.bin", std::ios::out | std::ios::binary);
    fout.write((char*)&gtData[0], gtData.size() * sizeof(float));
    fout.close();


    ////////////////////// Step 2: data to gmm training format ///////////////////////////////////////
    ///// from gtData to GMM traiing format
    std::vector<vtkm::Vec<Real,VARs>> gmmTrainData; //******* all training data for all GMMs
    std::vector<vtkm::Int32> gmmIds; //******** sample's coreesponding GMM ID
    int gmmCnt = 0; //*********** how many GMM we want to train
    std::vector<int> originalLoc;
    std::vector<int> lb;
    for(int i=0; i< SampleCubeSize*SampleCubeSize*SampleCubeSize; i++ ){
        if( gtData[i] == 0 ){
            int x, y, z;
            Index1DTo3D(i, x, y, z);
            vtkm::Vec<Real,VARs> sample;
            sample[0] = x; sample[1] = y; sample[2] = z;
            gmmTrainData.push_back(sample);
            gmmIds.push_back(0); //0 is GMM id 0
            originalLoc.push_back(i);
            lb.push_back(0);
        }
    }
    gmmCnt ++;
    for(int i=0; i< SampleCubeSize*SampleCubeSize*SampleCubeSize; i++ ){
        if( gtData[i] == 100 ){
            int x, y, z;
            Index1DTo3D(i, x, y, z);
            vtkm::Vec<Real,VARs> sample;
            sample[0] = x; sample[1] = y; sample[2] = z;
            gmmTrainData.push_back(sample);
            gmmIds.push_back(1); //1 is GMM id 1
            originalLoc.push_back(i);
            lb.push_back(1);
        }
    }
    gmmCnt ++;

    

    /////////////////////// Step 3: train  /////////////////////////////////////////////////
    // Run Kmean++ first to get better initialization for GMM training
    //vtkm::cont::Timer<> gmmKmeanTimer;
    //vtkm::cont::Timer<> kmeanTimer;
    std::vector< vtkm::Int32 > clusterLabel;
    vtkm::worklet::KMeanPP<nGauComps, VARs, Real> kmeanpp;
    kmeanpp.Run(gmmTrainData, gmmIds, gmmCnt, maxKMeanppIterations, clusterLabel);
    //std::cout<< "KMean++ Time: " << kmeanTimer.GetElapsedTime() << std::endl << std::endl << std::endl;

    std::vector<float> labelData(SampleCubeSize*SampleCubeSize*SampleCubeSize, -1);
    for( int i=0; i<originalLoc.size(); i++ ) {
        if( lb[i] == 0) labelData[originalLoc[i]] = clusterLabel[i];
        else labelData[originalLoc[i]] = clusterLabel[i] + 10;
    }
    std::ofstream lout(filePrefixPath + "labelData.bin", std::ios::out | std::ios::binary);
    lout.write((char*)&labelData[0], labelData.size() * sizeof(float));
    lout.close();

    //// GMM Training EM
    srand(time(NULL));
    //vtkm::cont::Timer<> gmmTimer;
    vtkm::worklet::GMMTraining<nGauComps, VARs, Real> em;
    em.Run(gmmTrainData, gmmIds, gmmCnt, maxEmIterations, clusterLabel, 1, 2, 0.001, 0.000001 );
    //6th para: 0: random init (clusterLabel useless), 1: use kmean++ init
    //7th para: screen output: 0: no output, 1:only iteration output, 2: output details
    //8th para: em stop threshold
    //9th para: minimal diagnal value in covariance matrix value (em must add this value to diagonal in cov mat)


    //////////////////////// Step 4 : write to file //////////////////////////////////
    char gmmFilePath[1000];
    strcpy( gmmFilePath, filePrefixPath.c_str() );
    strcat( gmmFilePath, "gmm.bin" );
    em.WriteGMMsFile( gmmFilePath ); ////  Save GMM to a file

    
    //////////////////////// Step 5 : load gmms from file /////////////////////////////////
    //// Load GMM from a file, programmer has to know the nGauComps, VARs, Real used to train the GMM
    vtkm::worklet::GMMTraining<nGauComps, VARs, Real> rsp; 
    rsp.LoadGMMsFile(gmmFilePath );
    

    //////////////////////// Step6 : resample ///////////////////////////////////////////
    /////Resample from GMMs
    std::vector< vtkm::Vec<Real, VARs> > resamples;

    std::vector<vtkm::Int32> reSampleGmmIds;
    std::vector<float> reSampleData(SampleCubeSize*SampleCubeSize*SampleCubeSize, 50);    
    
    // create reSampleGmmIds
    for( int i=0; i<1000000; i++ ){
        if( i< 500000)reSampleGmmIds.push_back( 0 );
        else reSampleGmmIds.push_back(1 );
    }
    rsp.SamplingFromMultipleGMMs( reSampleGmmIds,  resamples); //resample

    ////////////////////// Step 7: use resample result to reconstruct volume and write to file ///////////////////   
    ////// After resample, reconstruct the volume and save for visualization 
    for( int i=0; i<reSampleGmmIds.size(); i++ ){
        if( reSampleGmmIds[i] == 0 ){
            vtkm::Vec<Real, VARs> smp = resamples[i];
            int x = smp[0], y = smp[1], z = smp[2];
            if( x >=0 && x<SampleCubeSize && y >=0 && y<SampleCubeSize && z >=0 && z<SampleCubeSize ){
                int idx = Index3DTo1D(x, y, z);
                reSampleData[idx] = 0;
            }

        }
        if( reSampleGmmIds[i] == 1 ){
            vtkm::Vec<Real, VARs> smp = resamples[i];
            int x = smp[0], y = smp[1], z = smp[2];
            if( x >=0 && x<SampleCubeSize && y >=0 && y<SampleCubeSize && z >=0 && z<SampleCubeSize ){
                int idx = Index3DTo1D(x, y, z);
                reSampleData[idx] = 100;
            }
        }

    }
    std::ofstream ofout(filePrefixPath + "reSampleData.bin", std::ios::out | std::ios::binary);
    ofout.write((char*)&reSampleData[0], reSampleData.size() * sizeof(float));
    ofout.close();

} // TestMain Function
}//namespace

int main(int argc, char* argv[])
{
    return vtkm::cont::testing::Testing::Run(TestAyanGMM, argc, argv);
}
