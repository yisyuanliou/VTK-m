//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================
#include <vtkm/cont/Initialize.h>
#include <vtkm/worklet/FieldHistogram.h>
#include <vtkm/worklet/GMMTraining.h>
#include <vtkm/worklet/KMeanPP.h>
#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/testing/Testing.h>
#include <fstream>      

//
// Make a simple 2D, 1000 point dataset populated with stat distributions
//

typedef vtkm::Float64 Real;
const int VARs = 1;
const int nGauComps = 3;
const int maxEmIterations = 100;
const int maxKMeanppIterations = 100;

void PrintHistogram(vtkm::cont::ArrayHandle<vtkm::Id> bins,
	vtkm::Id numberOfBins,
	const vtkm::Range& range,
	vtkm::Float32 delta)
{
	vtkm::cont::ArrayHandle<vtkm::Id>::ReadPortalType binPortal = bins.ReadPortal();

	vtkm::Id sum = 0;
	for (vtkm::Id i = 0; i < numberOfBins; i++)
	{
		vtkm::Float64 lo = range.Min + (static_cast<vtkm::Float64>(i) * delta);
		vtkm::Float64 hi = lo + delta;
		sum += binPortal.Get(i);
		std::cout << "  BIN[" << i << "] Range[" << lo << ", " << hi << "] = " << binPortal.Get(i)
			<< std::endl;
	}
	//VTKM_TEST_ASSERT(test_equal(sum, 1000), "Histogram not full");
}
vtkm::cont::DataSet MakeTestDataSet(vtkm::Id3& vdims, char* filepath)
{
	vtkm::cont::DataSet dataSet;

	std::cout << vdims[0] << vdims[1] << vdims[2] << std::endl;
	vtkm::Id size = vdims[0] * vdims[1] * vdims[2];

	vtkm::Id *x = (vtkm::Id *)malloc(size * sizeof(vtkm::Id));
	vtkm::Id *y = (vtkm::Id *)malloc(size * sizeof(vtkm::Id));
	vtkm::Id *z = (vtkm::Id *)malloc(size * sizeof(vtkm::Id));
	vtkm::Float32 *data = (vtkm::Float32 *)malloc(size * sizeof(vtkm::Float32));
	std::vector<vtkm::Float32> v;

	std::cout << filepath << std::endl;
	FILE* fp = fopen(filepath, "rb");
	if (fp == NULL)
	{
		std::cout << "file open failed!" << std::endl;
	}

	float value;
	int index = 0;
	for (int d0 = 0; d0 < vdims[0]; d0++) {
		for (int d1 = 0; d1 < vdims[1]; d1++) {
			for (int d2 = 0; d2 < vdims[2]; d2++) {
				int tmp = fread(&value, sizeof(float), 1, fp);
				x[index] = d0;
				y[index] = d1;
				z[index] = d2;
				data[index] = value;
				index += 1;
			}
		}
	}
	
	dataSet.AddField(vtkm::cont::make_Field(
		"x", vtkm::cont::Field::Association::POINTS, x, size, vtkm::CopyFlag::On));
	dataSet.AddField(vtkm::cont::make_Field(
		"y", vtkm::cont::Field::Association::POINTS, y, size, vtkm::CopyFlag::On));
	dataSet.AddField(vtkm::cont::make_Field(
		"z", vtkm::cont::Field::Association::POINTS, z, size, vtkm::CopyFlag::On));
	dataSet.AddField(vtkm::cont::make_Field(
		"data", vtkm::cont::Field::Association::POINTS, data, size, vtkm::CopyFlag::On));

	return dataSet;
}
void TestGMMTraining()
{
	vtkm::Id3 vdims(100, 500, 500);
	char filepath[200] = "pf22.bin";
	vtkm::Id blksize = 20;
	vtkm::cont::DataSet data = MakeTestDataSet(vdims, filepath);
	vtkm::cont::ArrayHandle<vtkm::Float32> ValueHandle;
	data.GetField("data").GetData().CopyTo(ValueHandle);
	std::cout << "ok!" << std::endl;

	std::vector<vtkm::Int32> gmmIds;
	vtkm::Id gmmCnt = 0;

	std::vector< vtkm::Vec<Real, VARs> > trainData;
	trainData.clear();
	for (int a = 0; a < vdims[0]/blksize; ++a)
	{
		for (int b = 0; b < vdims[1] / blksize; ++b)
		{
			for (int c = 0; c < vdims[2] / blksize; ++c)
			{	
				// make a gmmIds
				for (int d0 = a*blksize; d0 < (a+1)*blksize; d0++) {
					for (int d1 = b * blksize; d1 < (b+1)* blksize; d1++) {
						for (int d2 = c * blksize; d2 < (c+1)* blksize; d2++) {
							int i = d0 * vdims[2] * vdims[1] + d1 * vdims[2] + d2;
							vtkm::Vec<Real, VARs> trainSample;
							trainSample[0] = ValueHandle.GetPortalConstControl().Get(i);
							trainData.push_back(trainSample);
							gmmIds.push_back(gmmCnt);
						}
					}
				}
				gmmCnt += 1;
				if (gmmCnt % 100 == 0)
				{
					std::cout << gmmCnt << std::endl;
				}
			}
		}
	}
	
	std::vector< vtkm::Int32 > clusterLabel;
	vtkm::worklet::KMeanPP<nGauComps, VARs, Real> kmeanpp;
	kmeanpp.Run(trainData, gmmIds, gmmCnt, maxKMeanppIterations, clusterLabel);
	std::cout << "KmeanPP" << std::endl;

	std::cout << "mewo" << std::endl;
	vtkm::worklet::GMMTraining<nGauComps, VARs, Real> em;
	em.Run(trainData, gmmIds, gmmCnt, maxEmIterations, clusterLabel, 0, 2, 0.001, 0.00000001);
	std::cout << "gmm" << std::endl;

	std::vector< vtkm::Vec<Real, VARs> > resamples;
	
	//std::vector<vtkm::Int32> reSampleGmmIds;
	//std::vector<float> reSampleData(vdims[0]* vdims[1] * vdims[2], 50);
	std::cout << "sample" << std::endl;
	em.SamplingFromMultipleGMMs(gmmIds, resamples); //resample
	std::cout << "hihi";
	
	int idx = 0;
	for (int a = 0; a < vdims[0] / blksize; ++a)
	{
		for (int b = 0; b < vdims[1] / blksize; ++b)
		{
			for (int c = 0; c < vdims[2] / blksize; ++c)
			{
				for (int d0 = a * blksize; d0 < (a + 1)*blksize; d0++) {
					for (int d1 = b * blksize; d1 < (b + 1)* blksize; d1++) {
						for (int d2 = c * blksize; d2 < (c + 1)* blksize; d2++) {
							int i = d0 * vdims[2] * vdims[1] + d1 * vdims[2] + d2;
							if(i%1000000==0)
							{
								std::cout << resamples[idx][0] << " " << gmmIds[idx] << std::endl;
							}
							ValueHandle.GetPortalControl().Set(i, resamples[idx][0]);
							idx += 1;
						}
					}
				}
			}
		}
	}
	
	FILE* fOut = fopen("ouput.bin", "wb");
	//std::cout << ValueHandle.GetPortalConstControl().GetNumberOfValues() << std::endl;
	std::vector<vtkm::Float32> blkLensVec(ValueHandle.GetPortalConstControl().GetNumberOfValues());
	std::copy(vtkm::cont::ArrayPortalToIteratorBegin(ValueHandle.GetPortalConstControl()),
		vtkm::cont::ArrayPortalToIteratorEnd(ValueHandle.GetPortalConstControl()),
		blkLensVec.begin());
	fwrite(&blkLensVec[0], sizeof(vtkm::Float32), vdims[0] * vdims[1] * vdims[2], fOut);
	fclose(fOut);
	
}

int main(int argc, char* argv[])
{

	//vtkm::cont::DataSet inData = reader.ReadDataSet();
	return vtkm::cont::testing::Testing::Run(TestGMMTraining, argc, argv);
}
