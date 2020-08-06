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

#include <vtkm/worklet/NDimsHistogram.h>
#include <vtkm/worklet/NDimsHistMarginalization.h>
#include <vtkm/worklet/VtkmTable.h>

#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/testing/Testing.h>
#include <vtkm/cont/DeviceAdapterAlgorithm.h>

#include <vtkm/cont/Timer.h>

#include <random>
#include <iomanip>

namespace{
  // PrintResultDataSet
  struct PrintArrayContentsFunctor
  {
    public:
    vtkm::Id PrintElementId;
    PrintArrayContentsFunctor(vtkm::Id _printElementId) : PrintElementId(_printElementId) {}

    template<typename T, typename  Storage >
    VTKM_CONT
    void  operator()(const vtkm::cont:: ArrayHandle <T,Storage > &array) const
    {
      this->PrintArrayPortal(array.GetPortalConstControl ());
    }
  private:
    template <typename  PortalType >
    VTKM_CONT
    void PrintArrayPortal(const PortalType &portal) const
    {
      typedef typename  PortalType :: ValueType  ValueType;
      ValueType  value = portal.Get(PrintElementId);
      std::cout << std::setprecision(2) << value << "\t";
    }
  };

  void PrintDataSet(vtkm::cont::DataSet ds)
  {
    for( int i=0; i<ds.GetNumberOfFields(); i++){
      std::cout << ds.GetField(i).GetName() << "\t";
    }
    std::cout << std::endl;

    vtkm::Id fieldLen = ds.GetField(0).GetData().GetNumberOfValues();
    for( int j=0; j<fieldLen; j++ ){
      for( int i=0; i<ds.GetNumberOfFields(); i++){
        ds.GetField(i).GetData().CastAndCall(PrintArrayContentsFunctor(j));
      }
      std::cout << std::endl;
    }
  }

// Construct an input data set using the tangle field worklet
vtkm::cont::DataSet MakeIsabelDataSet( vtkm::Id3& vdims, char* filepath )
{
  vtkm::cont::DataSet dataSet;

  //const vtkm::Id3 vdims(25, 125, 125);
  std::cout << vdims[0] << vdims[1] << vdims[2] << std::endl;
  vtkm::Id size = vdims[0] * vdims[1] * vdims[2];

  vtkm::Id* x; x = (vtkm::Id*)malloc(sizeof(vtkm::Id)*size);
  vtkm::Id* y; y = (vtkm::Id*)malloc(sizeof(vtkm::Id)*size);
  vtkm::Id* z; z = (vtkm::Id*)malloc(sizeof(vtkm::Id)*size);
  vtkm::Float32* v; 
  v = (vtkm::Float32*)malloc(sizeof(vtkm::Float32)*size);

  std::cout << filepath << std::endl;
  FILE* fp = fopen(filepath, "rb");
  float  value;

  int cnt = 0;
  for( int d0=0; d0<vdims[0]; d0++){
    for( int d1=0; d1<vdims[1]; d1++){
      for( int d2=0; d2<vdims[2]; d2++){
         fread(&value, sizeof(float), 1, fp);
         x[cnt] = d0;
         y[cnt] = d1;
         z[cnt] = d2;
         v[cnt] = value;
         cnt ++;
        }
      }
    }

    dataSet.AddField(vtkm::cont::Field("X", vtkm::cont::Field::ASSOC_POINTS, x, size));
    dataSet.AddField(vtkm::cont::Field("Y", vtkm::cont::Field::ASSOC_POINTS, y, size));
    dataSet.AddField(vtkm::cont::Field("Z", vtkm::cont::Field::ASSOC_POINTS, z, size));
    dataSet.AddField(vtkm::cont::Field("V", vtkm::cont::Field::ASSOC_POINTS, v, size));

  return dataSet;
}


struct MyStruct
{
  vtkm::Id blkx;
  vtkm::Id blky;
  vtkm::Id blkz;
  vtkm::Id binId;
  vtkm::Id x;
  vtkm::Id y;
  vtkm::Id z;
};

struct less_than_key
{
    inline bool operator() (const MyStruct& s0, const MyStruct& s1)
    {
      if( s0.blkx < s1.blkx )return true;
      else if( s0.blkx >  s1.blkx )return false;
      else{
        if( s0.blky < s1.blky )return true;
        else if( s0.blky >  s1.blky )return false;
        else{
          if( s0.blkz < s1.blkz )return true;
          else if( s0.blkz >  s1.blkz )return false;
          else{
            if( s0.binId < s1.binId )return true;
            else return false;
            
          }
          
        }

      }
      
    }
};

//
// Create a dataset with known point data and cell data (statistical distributions)
// Extract arrays of point and cell fields
// Create output structure to hold histogram bins
// Run FieldHistogram filter
//
void TestVtkmTable()
{

  // Data attached is the poisson distribution
  vtkm::Id3 vdims(25, 125, 125);
  std::cout << vdims[0] << vdims[1] << vdims[2] << std::endl;
  char filepath[200] = "/home/caseywng777/Desktop/smallpf20.bin";

  vtkm::cont::DataSet ds = MakeIsabelDataSet( vdims,  filepath );
  //PrintDataSet(ds); ///// print all fields of original dataset

  vtkm::cont::ArrayHandle<vtkm::Id> xary; 
  ds.GetField("X").GetData().CopyTo(xary);
  vtkm::cont::ArrayHandle<vtkm::Id> yary; 
  ds.GetField("Y").GetData().CopyTo(yary);
  vtkm::cont::ArrayHandle<vtkm::Id> zary; 
  ds.GetField("Z").GetData().CopyTo(zary);
  vtkm::cont::ArrayHandle<vtkm::Float32> vary; 
  ds.GetField("V").GetData().CopyTo(vary);

  vtkm::cont::Timer<> cpuTimer;
  std::vector<MyStruct> data;

  float min = 999999.0;
  float max = -999999.0;
  float delta;
  for( int i=0; i<vary.GetNumberOfValues(); i++ ){
    float value = vary.GetPortalConstControl().Get(i);
    if( value < min )min = value;
    if( value > max )max = value;
  }
  delta = (max - min) / 128.0;

  for( int i=0; i<vary.GetNumberOfValues(); i++ ){
    MyStruct ms;
    ms.blkx = static_cast<vtkm::Id> ( ( xary.GetPortalConstControl().Get(i) - 0 ) / 16.0 );
    ms.blky = static_cast<vtkm::Id> ( ( yary.GetPortalConstControl().Get(i) - 0 ) / 16.0 );
    ms.blkz = static_cast<vtkm::Id> ( ( zary.GetPortalConstControl().Get(i) - 0 ) / 16.0 );
    ms.binId = static_cast<vtkm::Id> ( ( vary.GetPortalConstControl().Get(i) - min ) / delta );
    ms.x = xary.GetPortalConstControl().Get(i);
    ms.y = yary.GetPortalConstControl().Get(i);
    ms.z = zary.GetPortalConstControl().Get(i);
    if( ms.binId < 0 )ms.binId = 0;
    if(ms.binId>=128)ms.binId = 127;

    data.push_back(ms);
    
  }
  std::sort(data.begin(), data.end(), less_than_key());
  std::cout << "cpu Time: " << cpuTimer.GetElapsedTime() << std::endl;

  // for( int i=0; i<vary.GetNumberOfValues(); i++ ){
  //   std::cout << data[i].blkx << " " << data[i].blky << " " << data[i].blkz << " " << data[i].binId << " "
  //    << data[i].x << " " << data[i].y << " " << data[i].z << std::endl; 
  // }

  // From
  vtkm::worklet::VtkmTable vtkmTable;
  vtkmTable.From(ds);

  // Select
  std::vector<std::string> selectFieldAndMethods = {"0,16(X)", "0,16(Y)","0,16(Z)", "128(V)", "X", "Y", "Z"  };
  vtkmTable.Select(selectFieldAndMethods);

  //Sort by 
  std::vector<std::string> sortFieldName = {"0,16(X)", "0,16(Y)","0,16(Z)", "128(V)"};
  vtkmTable.SortBy(sortFieldName);

  //Query
  vtkm::cont::Timer<> vtkmTableTimer;
  vtkm::cont::DataSet resultDS = vtkmTable.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());
  std::cout << "vtkmTable Time: " << vtkmTableTimer.GetElapsedTime() << std::endl;

  //std::cout << resultDS.GetNumberOfFields() << std::endl;
  //std::cout << resultDS.GetField(0).GetData().GetNumberOfValues() << std::endl;

  //Verify 
  {
    
    vtkm::cont::ArrayHandle<vtkm::Id> xary; 
    resultDS.GetField("0,16(X)").GetData().CopyTo(xary);
    vtkm::cont::ArrayHandle<vtkm::Id> yary; 
    resultDS.GetField("0,16(Y)").GetData().CopyTo(yary);
    vtkm::cont::ArrayHandle<vtkm::Id> zary; 
    resultDS.GetField("0,16(Z)").GetData().CopyTo(zary);
    vtkm::cont::ArrayHandle<vtkm::Id> vary; 
    resultDS.GetField("128(V)").GetData().CopyTo(vary);
    vtkm::cont::ArrayHandle<vtkm::Id> xcoordiary; 
    resultDS.GetField("X").GetData().CopyTo(xcoordiary);
    vtkm::cont::ArrayHandle<vtkm::Id> ycoordiary; 
    resultDS.GetField("Y").GetData().CopyTo(ycoordiary);
    vtkm::cont::ArrayHandle<vtkm::Id> zcoordiary; 
    resultDS.GetField("Z").GetData().CopyTo(zcoordiary);

    for( int i=0; i<xary.GetPortalConstControl().GetNumberOfValues(); i++ ){
      vtkm::Id x = xary.GetPortalConstControl().Get(i);
      vtkm::Id y = yary.GetPortalConstControl().Get(i);
      vtkm::Id z = zary.GetPortalConstControl().Get(i);
      vtkm::Id b = vary.GetPortalConstControl().Get(i);
      vtkm::Id xc = xcoordiary.GetPortalConstControl().Get(i);
      vtkm::Id yc = ycoordiary.GetPortalConstControl().Get(i);
      vtkm::Id zc = zcoordiary.GetPortalConstControl().Get(i);

      if( data[i].blkx - x != 0)std::cout << data[i].blkx << " "<<  x << std::endl;
      if( data[i].blky - y != 0)std::cout << data[i].blky << " "<<  y << std::endl;
      if( data[i].blkz - z != 0)std::cout << data[i].blkz << " "<<  z << std::endl;
      if( data[i].binId - b != 0)std::cout << data[i].binId << " "<<  b << std::endl;
      //if( data[i].x - xc != 0)std::cout << "x " << data[i].x << " "<<  xc  << std::endl;
      //if( data[i].y - yc != 0)std::cout << "y " <<data[i].y << " "<<  yc << std::endl;
      //if( data[i].z - zc != 0)std::cout << "z " <<data[i].z << " "<<  zc << std::endl;
    }
  }

  // //Print query result
  // std::cout <<"****************Query result******************" << std::endl;
  //PrintDataSet(resultDS);
  
} // TestNDimsHistMarginalization
}

int UnitTestVtkmTableGmmPreparation(int, char* [])
{ 
  return vtkm::cont::testing::Testing::Run(TestVtkmTable);
}
