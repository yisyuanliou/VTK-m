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
#include <vtkm/worklet/VtkmSQL.h>

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

    // for( int i=0; i<ds.GetNumberOfFields(); i++){
    //   std::cout << ds.GetField(i).GetName() << ":\t";
    //   ds.GetField(i).GetData().CastAndCall(PrintArrayContentsFunctor());
    //   std::cout << std::endl;
    // }
  }

// Make testing dataset with three fields(variables), each one has 1000 values
vtkm::cont::DataSet MakeTestDataSet()
{
  vtkm::cont::DataSet dataSet;

  const int nVerts = 9;

  vtkm::Id X[nVerts] = {
    0, 1, 2, 0, 1, 2, 0, 1, 2
  };

  vtkm::Id Y[nVerts] = {
    0, 0, 0, 1, 1, 1, 2, 2, 2
  };

  vtkm::Float32 V[nVerts] = {
    0, 1, 2, 3,4 ,5 ,6 ,7 ,8 
  };

  // Set point scalars
  dataSet.AddField(vtkm::cont::Field("X", vtkm::cont::Field::ASSOC_POINTS, X, nVerts));
  dataSet.AddField(vtkm::cont::Field("Y", vtkm::cont::Field::ASSOC_POINTS, Y, nVerts));
  dataSet.AddField(vtkm::cont::Field("V", vtkm::cont::Field::ASSOC_POINTS, V, nVerts));

  return dataSet;
}

// Make testing dataset with three fields(variables), each one has 1000 values
vtkm::cont::DataSet MakeParticleTestDataSet()
{
  vtkm::cont::DataSet dataSet;

  const vtkm::Float32 randRange = 10.0f;
  const int nVerts = 9;

  vtkm::Id ids[nVerts];
  vtkm::Float32 x[nVerts];
  vtkm::Float32 y[nVerts];
  vtkm::Float64 weight[nVerts];

  std::default_random_engine dre;
  std::uniform_real_distribution<vtkm::Float32> dr(0.0f, randRange);

  for( int i=0; i<nVerts; i++ ){
    ids[i] = i;
    x[i] = dr(dre);
    y[i] = dr(dre);
    weight[i] = dr(dre);
  }

  // Set point scalars
  dataSet.AddField(vtkm::cont::Field("ID", vtkm::cont::Field::ASSOC_POINTS, ids, nVerts));
  dataSet.AddField(vtkm::cont::Field("X", vtkm::cont::Field::ASSOC_POINTS, x, nVerts));
  dataSet.AddField(vtkm::cont::Field("Y", vtkm::cont::Field::ASSOC_POINTS, y, nVerts));
  dataSet.AddField(vtkm::cont::Field("Weight", vtkm::cont::Field::ASSOC_POINTS, weight, nVerts));

  return dataSet;
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
  vtkm::Float32* v; v = (vtkm::Float32*)malloc(sizeof(vtkm::Float32)*size);

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


//
// Create a dataset with known point data and cell data (statistical distributions)
// Extract arrays of point and cell fields
// Create output structure to hold histogram bins
// Run FieldHistogram filter
//
void TestVtkmSQL()
{
  vtkm::cont::DataSet ds0 = MakeTestDataSet();
  vtkm::cont::DataSet ds1 = MakeParticleTestDataSet();

  // From
  std::vector<vtkm::cont::DataSet> datasets = {ds0, ds1};
  vtkm::worklet::VtkmSQL vtkmSql(datasets);

  // Select
  // std::vector<std::string> selectFieldAndMethods = {"0,16(X)", "0,16(Y)","0,16(Z)", "128(V)", "COUNT(V)" };
  // vtkmTable.Select(selectFieldAndMethods);
  // vtkm::worklet::Argument arg0;
  // arg0.fieldName = "X"; arg0.op = "QuantizeByMinDelta"; arg0.op0 = "0"; arg0.op1 = "16"; 
  // vtkm::worklet::Argument arg1;
  // arg1.fieldName = "Y"; arg1.op = "QuantizeByMinDelta"; arg1.op0 = "0"; arg1.op1 = "16"; 
  // vtkm::worklet::Argument arg2;
  // arg2.fieldName = "V"; arg2.op = "QuantizeByBin"; arg2.op0 = "128"; arg2.op1 = "";
  // std::vector<vtkm::worklet::Argument> selectedArgument = {arg0, arg1, arg2};
  // vtkmTable.Select(selectedArgument);

  // vtkm::worklet::Argument arg0;
  // arg0.fieldName = "X"; arg0.op = "QuantizeByBin"; arg0.op0 = "2"; arg0.op1 = ""; 
  // vtkm::worklet::Argument arg1;
  // arg1.fieldName = "Y"; arg1.op = "QuantizeByMinDelta"; arg1.op0 = "0"; arg1.op1 = "1.5"; 
  // vtkm::worklet::Argument arg2;
  // arg2.fieldName = "V"; arg2.op = "ADD"; arg2.op0 = ""; arg2.op1 = "";
  // vtkm::worklet::Argument arg3;
  // arg3.fieldName = "X"; arg3.op = "COUNT"; arg3.op0 = ""; arg3.op1 = ""; 
  // std::vector<vtkm::worklet::Argument> selectedArgument = {arg0, arg1, arg2, arg3};
  // vtkmTable.Select(selectedArgument);
  vtkmSql.Select(0, "X");
  vtkmSql.Select(0, "Y");
  vtkmSql.Select(0, "V");
  vtkmSql.Select(1, "Y");
  vtkmSql.Select(1, "Weight");

  // Where
  // std::vector<std::string> whereFieldName = {"X", "Y"};
  // std::vector<vtkm::Range> rangeX = {vtkm::Range(1.0f, 3.99999f), vtkm::Range(7.0f, 9.99999f)};
  // std::vector<vtkm::Range> rangeY = {vtkm::Range(1.0f, 3.99999f), vtkm::Range(7.0f, 9.99999f)};
  // std::vector<std::vector<vtkm::Range>> whereRanges = {rangeX, rangeY};
  // vtkmTable.Where(whereFieldName, whereRanges);

  //Group By
  // std::vector<std::string> groupFieldName = {"X", "Y"};
  // vtkmTable.GroupBy(groupFieldName);
  // vtkmTable.GroupBy("X", "QuantizeByBin", "2" );
  // vtkmTable.GroupBy("Y", "QuantizeByMinDelta", "0", "1.5" );

  //Sort by 
  // std::vector<std::string> sortFieldName = {"X", "Y", "V"};
  // vtkmTable.SortBy(sortFieldName);
  // vtkmTable.SortBy("X", "QuantizeByBin", "2" );
  // vtkmTable.SortBy("Y", "QuantizeByMinDelta", "0", "1.5" );
  // vtkmTable.SortBy("V", "SUM" );

  // Cross Join
  vtkmSql.CrossJoin();

  //Query
  // vtkm::cont::Timer<> vtkmTableTimer;
  vtkm::cont::DataSet resultDS = vtkmSql.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());
  // vtkm::cont::DataSet resultDS = vtkmTable.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());
  // std::cout << "vtkmTable Time: " << vtkmTableTimer.GetElapsedTime() << std::endl;

  

  // //Print query result
  std::cout <<"****************Query result******************" << std::endl;
  PrintDataSet(resultDS);
  
} // TestNDimsHistMarginalization
}

int UnitTestVtkmSQL(int, char* [])
{ 
  return vtkm::cont::testing::Testing::Run(TestVtkmSQL);
}
