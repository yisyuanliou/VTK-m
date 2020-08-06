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

vtkm::cont::DataSet MakeDataSetA()
{
  vtkm::cont::DataSet dataSet;

  const int nVerts = 9;

  vtkm::Id Idx[nVerts] = {
    14,12,16,24,20,8,10,7,12
  };

  vtkm::Id Dt[nVerts] = {
    0,1,2,3,4,5,6,7,8
  };

  // Set point scalars
  dataSet.AddField(vtkm::cont::Field("A-Idx", vtkm::cont::Field::ASSOC_POINTS, Idx, nVerts));
  dataSet.AddField(vtkm::cont::Field("A-Dt", vtkm::cont::Field::ASSOC_POINTS, Dt, nVerts));

  return dataSet;
}

vtkm::cont::DataSet MakeDataSetB()
{
  vtkm::cont::DataSet dataSet;

  const int nVerts = 12;

  vtkm::Id Idx[nVerts] = {
    21,17,11,21,10,12,10,14,20,10,19,14
  };

  vtkm::Id Dt[nVerts] = {
    9,10,11,12,13,14,15,16,17,18,19,20
  };

  // Set point scalars
  dataSet.AddField(vtkm::cont::Field("B-Idx", vtkm::cont::Field::ASSOC_POINTS, Idx, nVerts));
  dataSet.AddField(vtkm::cont::Field("B-Dt", vtkm::cont::Field::ASSOC_POINTS, Dt, nVerts));

  return dataSet;
}

//
// Create a dataset with known point data and cell data (statistical distributions)
// Extract arrays of point and cell fields
// Create output structure to hold histogram bins
// Run FieldHistogram filter
//
void TestEqualJoin()
{
  vtkm::cont::DataSet dsA = MakeDataSetA();
  vtkm::cont::DataSet dsB = MakeDataSetB();

  // From
  std::vector<vtkm::cont::DataSet> datasets = {dsA, dsB};
  vtkm::worklet::VtkmSQL vtkmSql(datasets);

  vtkmSql.Select(0, "A-Idx");
  vtkmSql.Select(0, "A-Dt");
  vtkmSql.Select(1, "B-Idx");
  vtkmSql.Select(1, "B-Dt");

  // Cross Join
  vtkmSql.EqualJoin(0, 0);

  //Query
  vtkm::cont::DataSet resultDS = vtkmSql.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());

  // //Print query result
  std::cout <<"****************Query result******************" << std::endl;
  PrintDataSet(resultDS);
  
} // TestNDimsHistMarginalization
}

int UnitTestEqualJoin(int, char* [])
{ 
  return vtkm::cont::testing::Testing::Run(TestEqualJoin);
}
