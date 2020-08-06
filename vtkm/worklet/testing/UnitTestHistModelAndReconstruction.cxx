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
#include <vtkm/cont/Timer.h>
#include <vtkm/worklet/VtkmTable.h>
#include <vtkm/worklet/VtkmSQL.h>
#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/DataSetBuilderExplicit.h>
#include <vtkm/cont/DataSetBuilderRectilinear.h>
#include <vtkm/cont/DataSetBuilderUniform.h>
#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/io/writer/VTKDataSetWriter.h>
#include <vtkm/io/reader/VTKDataSetReader.h>

namespace{

///// dataset and field generator from "vtkm-m/vtkm/rendering/testing/UnitTestMapperVolume.cxx" /////
class TangleField : public vtkm::worklet::WorkletMapField
{
public:
  typedef void ControlSignature(FieldIn<IdType> vertexId, FieldOut<Scalar> v);
  typedef void ExecutionSignature(_1, _2);
  typedef _1 InputDomain;

  const vtkm::Id xdim, ydim, zdim;
  const vtkm::Float32 xmin, ymin, zmin, xmax, ymax, zmax;
  const vtkm::Id cellsPerLayer;

  VTKM_CONT
  TangleField(const vtkm::Id3 dims, const vtkm::Float32 mins[3], const vtkm::Float32 maxs[3]) : xdim(dims[0]), ydim(dims[1]), zdim(dims[2]),
              xmin(mins[0]), ymin(mins[1]), zmin(mins[2]), xmax(maxs[0]), ymax(maxs[1]), zmax(maxs[2]), cellsPerLayer((xdim) * (ydim)) { };

  VTKM_EXEC
  void operator()(const vtkm::Id &vertexId, vtkm::Float32 &v) const
  {
    const vtkm::Id x = vertexId % (xdim);
    const vtkm::Id y = (vertexId / (xdim)) % (ydim);
    const vtkm::Id z = vertexId / cellsPerLayer;

    const vtkm::Float32 fx = static_cast<vtkm::Float32>(x) / static_cast<vtkm::Float32>(xdim-1);
    const vtkm::Float32 fy = static_cast<vtkm::Float32>(y) / static_cast<vtkm::Float32>(xdim-1);
    const vtkm::Float32 fz = static_cast<vtkm::Float32>(z) / static_cast<vtkm::Float32>(xdim-1);

    const vtkm::Float32 xx = 3.0f*(xmin+(xmax-xmin)*(fx));
    const vtkm::Float32 yy = 3.0f*(ymin+(ymax-ymin)*(fy));
    const vtkm::Float32 zz = 3.0f*(zmin+(zmax-zmin)*(fz));

    v = (xx*xx*xx*xx - 5.0f*xx*xx + yy*yy*yy*yy - 5.0f*yy*yy + zz*zz*zz*zz - 5.0f*zz*zz + 11.8f) * 0.2f + 0.5f;
  }
};


// Construct an input data set using the tangle field worklet
vtkm::cont::DataSet Make3DDatasetForVtkmSQL(vtkm::Id3 dims)
{
  vtkm::cont::DataSet dataSet;

  const vtkm::Id3 vdims(dims[0] + 1, dims[1] + 1, dims[2] + 1);

  vtkm::Float32 mins[3] = {-1.0f, -1.0f, -1.0f};
  vtkm::Float32 maxs[3] = {1.0f, 1.0f, 1.0f};

  vtkm::cont::ArrayHandle<vtkm::Float32> fieldArray;
  vtkm::cont::ArrayHandleCounting<vtkm::Id> vertexCountImplicitArray(0, 1, vdims[0]*vdims[1]*vdims[2]);
  vtkm::worklet::DispatcherMapField<TangleField> tangleFieldDispatcher(TangleField(vdims, mins, maxs));
  tangleFieldDispatcher.Invoke(vertexCountImplicitArray, fieldArray);

  std::vector<vtkm::Id> x;
  std::vector<vtkm::Id> y;
  std::vector<vtkm::Id> z;
  for( int d0=0; d0<dims[2] + 1; d0++){
    for( int d1=0; d1<dims[1] + 1; d1++){
      for( int d2=0; d2<dims[0] + 1; d2++){
        z.push_back(d0);
        y.push_back(d1);
        x.push_back(d2);
      }
    }
  }

  vtkm::worklet::VtkmSQL::AddColumn(dataSet, "X", x);
  vtkm::worklet::VtkmSQL::AddColumn(dataSet, "Y", y);
  vtkm::worklet::VtkmSQL::AddColumn(dataSet, "Z", z);
  vtkm::worklet::VtkmSQL::AddColumn(dataSet, "V", fieldArray);

  return dataSet;
}


class Sampling : public vtkm::worklet::WorkletMapField
{
public:
    typedef void ControlSignature(FieldIn<>, FieldIn<>, FieldIn<>, FieldOut<>, WholeArrayIn<>, WholeArrayIn<>);
    typedef void ExecutionSignature(_1, _2, _3, _4, _5, _6);

    VTKM_CONT
    Sampling() {}

    template<typename BinPortalType, typename FreqPortalType>
    VTKM_EXEC
    void operator()(const vtkm::Id &stIdx, const vtkm::Id &len, const vtkm::Float64 &rUniform, 
                    vtkm::Id &sample, BinPortalType &binInPortal, FreqPortalType &freqInPortal ) const
    {
      vtkm::Float32 freqSum = 0;
      for( int i = 0; i < len; i++ ) freqSum += freqInPortal.Get(stIdx + i);
      vtkm::Float32 r = freqSum * rUniform;
      int i;
      vtkm::Float32 sum = 0;
      for( i = 0; i < len; i++ ){
         sum += freqInPortal.Get(stIdx + i);
         if( sum > r )break;
      }

      if( i == len ) i--;
      sample = binInPortal.Get(stIdx + i);
    }
};


void TestHistModelAndReconstruction()
{

  vtkm::Id3 dims(200, 198, 195); 
  //the output dataset resolution is dims[0]+1, dims[1]+1, dims[2]+11 
  //note that the data range of the raw and resampling are in differnt domain range
  //click rescale domain range button to see volume renering in paraview
  vtkm::cont::DataSet rawDataSet = Make3DDatasetForVtkmSQL(dims);

  std::string blkSize = "16";

  // Compute Histogram
  vtkm::cont::DataSet histogramTable;
  {
    vtkm::worklet::VtkmSQL vtkmSql(rawDataSet);
    
    // select QuantizedByMinDelta(x, 0, 16) as BlkX from table.0 
    vtkmSql.Select(0, "X", "QuantizeByMinDelta", "0", blkSize, "BlkX");
    vtkmSql.Select(0, "Y", "QuantizeByMinDelta", "0", blkSize, "BlkY");
    vtkmSql.Select(0, "Z", "QuantizeByMinDelta", "0", blkSize, "BlkZ");
    vtkmSql.Select(0, "V", "QuantizeByBin", "128", "Bin");
    
    // select Count(ID) as Freq from table.0 (ID is the automatically generated 
    // row_id column).
    vtkmSql.Select(0, "RowID", "COUNT", "Freq" );

    // Group by BlkX, BlkY, BlkZ, Bin
    // Note: For current implemention, "Group by" implies "Order by"
    vtkmSql.GroupBy(0, "BlkX");
    vtkmSql.GroupBy(0, "BlkY");
    vtkmSql.GroupBy(0, "BlkZ");
    vtkmSql.GroupBy(0, "Bin");

    // Order by BlkX, BlkY, BlkZ, Bin
    vtkmSql.SortBy(0, "BlkX");
    vtkmSql.SortBy(0, "BlkY");
    vtkmSql.SortBy(0, "BlkZ");
    vtkmSql.SortBy(0, "Bin");
    histogramTable = vtkmSql.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());
    // Limit 10
    vtkm::worklet::VtkmSQL::PrintVtkmSqlTable(histogramTable, 10);
  }
  
  //Compute index for the sparse representation histograms
  vtkm::cont::DataSet histIndexTable;
  {
    vtkm::worklet::VtkmSQL vtkmSql(histogramTable);
    
    // select BlkX, BlkY, BlkZ, Min(ID) as BlkHistSt,
    // Count(Id) as BlkHistLen
    vtkmSql.Select(0, "BlkX", "BlkX" );
    vtkmSql.Select(0, "BlkY", "BlkY" );
    vtkmSql.Select(0, "BlkZ", "BlkZ" );
    vtkmSql.Select(0, "RowID", "MIN", "BlkHistSt" );
    vtkmSql.Select(0, "RowID", "COUNT", "BlkHistLen" );

    // Group By BlkX, BlkY, BlkZ
    vtkmSql.GroupBy(0, "BlkX" );
    vtkmSql.GroupBy(0, "BlkY" );
    vtkmSql.GroupBy(0, "BlkZ" );

    // Order By BlkX, BlkY, BlkZ
    vtkmSql.SortBy(0, "BlkX" );
    vtkmSql.SortBy(0, "BlkY" );
    vtkmSql.SortBy(0, "BlkZ" );
    histIndexTable = vtkmSql.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());
    vtkm::worklet::VtkmSQL::PrintVtkmSqlTable(histIndexTable, 10);
  }

  // read write file test
  vtkm::worklet::VtkmSQL::VtkmSqlTableWriter(histIndexTable, "test.vtk");
  printf("finish write \n");

  vtkm::cont::DataSet histIndexTableLoad;
  vtkm::worklet::VtkmSQL::VtkmSqlTableReader(histIndexTableLoad, "test.vtk");
  vtkm::worklet::VtkmSQL::PrintVtkmSqlTable(histIndexTableLoad, 10);
  printf("finish load \n");

  //Compute block xyz for all xyz we want to reconstruct
  vtkm::cont::DataSet xyzIdx2HistTable;
  {
    vtkm::worklet::VtkmSQL vtkmSql(rawDataSet, histIndexTableLoad);
    
    // SELECT
    //      rawDataSet,X as X
    //      rawDataSet.Y as Y,
    //      rawDataSet.Z as Z,
    //      QuantizedByMinDelta(X, 0, 16) as BlkX,
    //      QuantizedByMinDelta(Y, 0, 16) as BlkY,
    //      QuantizedByMinDelta(Z, 0, 16) as BlkZ,
    //      histIndexTableLoad.BlkX as BlkX_1,
    //      histIndexTableLoad.BlkY as BlkY_1,
    //      histIndexTableLoad.BlkZ as BlkZ_1,
    //      histIndexTableLoad.BlkHistSt as BlkHistSt,
    //      histIndexTableLoad.BlkHistLen as BlkHistLen,
    
    vtkmSql.Select(0, "X", "X" );
    vtkmSql.Select(0, "Y", "Y" );
    vtkmSql.Select(0, "Z", "Z" );
    vtkmSql.Select(0, "X", "QuantizeByMinDelta", "0", blkSize, "BlkX");
    vtkmSql.Select(0, "Y", "QuantizeByMinDelta", "0", blkSize, "BlkY");
    vtkmSql.Select(0, "Z", "QuantizeByMinDelta", "0", blkSize, "BlkZ");
    vtkmSql.Select(1, "BlkX", "BlkX_1");
    vtkmSql.Select(1, "BlkY", "BlkY_1");
    vtkmSql.Select(1, "BlkZ", "BlkZ_1");
    vtkmSql.Select(1, "BlkHistSt", "BlkHistSt");
    vtkmSql.Select(1, "BlkHistLen", "BlkHistLen");

    // EqualJoin BlkX = BlkX_1, BlkY = BlkY_1, BlkZ = BlkZ_1
    vtkmSql.EqualJoin("BlkX", "BlkX_1");
    vtkmSql.EqualJoin("BlkY", "BlkY_1");
    vtkmSql.EqualJoin("BlkZ", "BlkZ_1");

    // ORDER BY X, Y, Z
    vtkmSql.SortBy(0, "Z" );
    vtkmSql.SortBy(0, "Y" );
    vtkmSql.SortBy(0, "X" );

    xyzIdx2HistTable = vtkmSql.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());
    vtkm::worklet::VtkmSQL::PrintVtkmSqlTable(xyzIdx2HistTable, 10);
  }

  //Resample from histogram
  {
    vtkm::cont::ArrayHandle<vtkm::Id> histStIdx;
    vtkm::worklet::VtkmSQL::GetColumn(xyzIdx2HistTable, "BlkHistSt", histStIdx);
    vtkm::cont::ArrayHandle<vtkm::Id> histLen;
    vtkm::worklet::VtkmSQL::GetColumn(xyzIdx2HistTable, "BlkHistLen", histLen);
    vtkm::cont::ArrayHandle<vtkm::Id> bins;
    vtkm::worklet::VtkmSQL::GetColumn(histogramTable, "Bin", bins);
    vtkm::cont::ArrayHandle<vtkm::Id> freqs;
    vtkm::worklet::VtkmSQL::GetColumn(histogramTable, "Freq", freqs);

    //Generate Random Number in CPU
    std::default_random_engine rng;
    std::uniform_real_distribution<vtkm::Float64> dr(0.0f, 1.0f);
    std::vector<vtkm::Float64> uniformRandom;
    for( int i=0; i<histStIdx.GetNumberOfValues() ; i++ ){
        uniformRandom.push_back( dr(rng) );
    }
    vtkm::cont::ArrayHandle<vtkm::Float64> uniformRandomHandle = vtkm::cont::make_ArrayHandle(uniformRandom);

    //Call worklet to sample from histograms
    vtkm::cont::ArrayHandle<vtkm::Id> samples;
    vtkm::worklet::DispatcherMapField<Sampling, VTKM_DEFAULT_DEVICE_ADAPTER_TAG> samplingDispatcher( Sampling{} );
    samplingDispatcher.Invoke(histStIdx, histLen, uniformRandomHandle, samples, bins, freqs);

    //write rawdata to file
    {
      std::vector<vtkm::Float32> rawArray;
      vtkm::worklet::VtkmSQL::GetColumn(rawDataSet, "V", rawArray);
      
      std::ofstream ofs;
      ofs.open("HistReconstructionRaw.bin", std::ios::out | std::ios::binary);
      ofs.write(reinterpret_cast<char*>(&rawArray[0]), rawArray.size()*sizeof(vtkm::Float32)); 
      ofs.close();
    }

    //write resample result to file
    {
      std::vector<vtkm::Float32> fileArray(samples.GetPortalConstControl().GetNumberOfValues());
      std::copy(vtkm::cont::ArrayPortalToIteratorBegin(samples.GetPortalConstControl()),
                vtkm::cont::ArrayPortalToIteratorEnd(samples.GetPortalConstControl()),
                fileArray.begin());
      std::ofstream ofs;
      ofs.open("HistReconstruction.bin", std::ios::out | std::ios::binary);
      ofs.write(reinterpret_cast<char*>(&fileArray[0]), fileArray.size()*sizeof(vtkm::Float32)); 
      ofs.close();
    }
  }

} // TestFieldHistogram
}

int UnitTestHistModelAndReconstruction(int, char* [])
{
  return vtkm::cont::testing::Testing::Run(TestHistModelAndReconstruction);
}
