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

  vtkm::Id X[nVerts] = {
    -1,5,1,0,0,5,-1,1,0
  };

  vtkm::Id Y[nVerts] = {
    0,7,-3,8,0,7,-3,-3,0
  };

  vtkm::Id V[nVerts] = {
    101,102,103,104,105,106,107,108,109
  };

  vtkm::Id T[nVerts] = {
    201,202,203,204,205,206,207,208,209
  };

  // Set point scalars
  dataSet.AddField(vtkm::cont::Field("A-X", vtkm::cont::Field::ASSOC_POINTS, X, nVerts));
  dataSet.AddField(vtkm::cont::Field("A-Y", vtkm::cont::Field::ASSOC_POINTS, Y, nVerts));
  dataSet.AddField(vtkm::cont::Field("A-V", vtkm::cont::Field::ASSOC_POINTS, V, nVerts));
  dataSet.AddField(vtkm::cont::Field("A-T", vtkm::cont::Field::ASSOC_POINTS, T, nVerts));

  return dataSet;
}

vtkm::cont::DataSet MakeDataSetB()
{
  vtkm::cont::DataSet dataSet;

  const int nVerts = 8;

  vtkm::Id X[nVerts] = {
    1,-1,7,0,1,5,-1,0
  };

  vtkm::Id Y[nVerts] = {
    -3,0,8,0,-3,7,0,10
  };

  vtkm::Id H[nVerts] = {
    1001,1002,1003,1004,1005,1006,1007,1008
  };

  // Set point scalars
  dataSet.AddField(vtkm::cont::Field("B-X", vtkm::cont::Field::ASSOC_POINTS, X, nVerts));
  dataSet.AddField(vtkm::cont::Field("B-Y", vtkm::cont::Field::ASSOC_POINTS, Y, nVerts));
  dataSet.AddField(vtkm::cont::Field("B-H", vtkm::cont::Field::ASSOC_POINTS, H, nVerts));

  return dataSet;
}

//
// Create a dataset with known point data and cell data (statistical distributions)
// Extract arrays of point and cell fields
// Create output structure to hold histogram bins
// Run FieldHistogram filter
//
void TestEqualJoin2D()
{
  vtkm::cont::DataSet dsA = MakeDataSetA();
  vtkm::cont::DataSet dsB = MakeDataSetB();

  // From
  std::vector<vtkm::cont::DataSet> datasets = {dsA, dsB};
  vtkm::worklet::VtkmSQL vtkmSql(datasets);

  vtkmSql.Select(0, "A-X");
  vtkmSql.Select(0, "A-Y");
  vtkmSql.Select(0, "A-V");
  vtkmSql.Select(0, "A-T");
  vtkmSql.Select(1, "B-X");
  vtkmSql.Select(1, "B-Y");
  vtkmSql.Select(1, "B-H");

  // Cross Join
  vtkmSql.EqualJoin(0, 0);
  vtkmSql.EqualJoin(1, 1);

  //Query
  vtkm::cont::DataSet resultDS = vtkmSql.Query(VTKM_DEFAULT_DEVICE_ADAPTER_TAG());

  // //Print query result
  std::cout <<"****************Query result******************" << std::endl;
  PrintDataSet(resultDS);
  
} // TestNDimsHistMarginalization
}

int UnitTestEqualJoin2D(int, char* [])
{ 
  return vtkm::cont::testing::Testing::Run(TestEqualJoin2D);
}
