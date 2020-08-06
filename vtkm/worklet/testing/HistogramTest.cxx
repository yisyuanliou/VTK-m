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

#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/testing/Testing.h>

#include <fstream>      

//
// Make a simple 2D, 1000 point dataset populated with stat distributions
//

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
void TestHistogram()
{
	vtkm::Id3 vdims(100, 500, 500);
	char filepath[200] = "pf22.bin";
	vtkm::Id blksize = 10;
	vtkm::cont::DataSet data = MakeTestDataSet(vdims, filepath);
	vtkm::cont::ArrayHandle<vtkm::Float32> ValueHandle;
	data.GetField("data").GetData().CopyTo(ValueHandle);
	std::cout << "ok!" << std::endl;
	int blockCount = 1;
	for (int a = 0; a < vdims[0]/blksize; ++a)
	{
		for (int b = 0; b < vdims[1] / blksize; ++b)
		{
			for (int c = 0; c < vdims[2] / blksize; ++c)
			{
				std::cout << "block" << blockCount << std::endl;
				blockCount += 1;
				vtkm::Float32 *block = (vtkm::Float32 *)malloc(8000 * sizeof(vtkm::Float32));
				int index = 0;
				for (int d0 = a*blksize; d0 < (a+1)*blksize; d0++) {
					for (int d1 = b * blksize; d1 < (b+1)* blksize; d1++) {
						for (int d2 = c * blksize; d2 < (c+1)* blksize; d2++) {
							int i = d0 * vdims[2] * vdims[1] + d1 * vdims[2] + d2;
							//std::cout << ValueHandle.GetPortalConstControl().Get(i) << " ";
							block[index] = ValueHandle.GetPortalConstControl().Get(i);
							index += 1;
						}
					}
				}

				// Create the output bin array
				vtkm::Id numberOfBins = 10;
				vtkm::Range range;
				vtkm::Float32 delta;
				vtkm::cont::ArrayHandle<vtkm::Id> bins;
				bins.Allocate(numberOfBins);

				// Get point data
				//vtkm::cont::ArrayHandle<vtkm::Float32> p_data;
				//dataSet.GetField("p_data").GetData().CopyTo(p_data);
				vtkm::cont::ArrayHandle<vtkm::Float32> p_data = vtkm::cont::make_ArrayHandle(block, blksize*blksize*blksize);

				vtkm::worklet::FieldHistogram histogram;
				// Run data
				histogram.Run(p_data, numberOfBins, range, delta, bins);

				free(block);
				//PrintHistogram(bins, numberOfBins, range, delta);

				vtkm::cont::ArrayHandle<vtkm::Id> CDFbins;
				vtkm::cont::Algorithm::ScanInclusive(bins, CDFbins);

				std::vector<vtkm::Int64> number;
				for (int i = 0; i < blksize*blksize*blksize; ++i)
				{
					number.push_back(rand() % 9);
				}
				vtkm::cont::ArrayHandle<vtkm::Int64> value = vtkm::cont::make_ArrayHandle(number);
				vtkm::cont::ArrayHandle<vtkm::Id> result;

				vtkm::cont::Algorithm::LowerBounds(CDFbins, value, result);
				int randomindex = 0;
				for (int d0 = a * blksize; d0 < (a+1)* blksize; d0++) {
					for (int d1 = b * blksize; d1 < (b+1)* blksize; d1++) {
						for (int d2 = c * blksize; d2 < (c+1)* blksize; d2++) {
							int blkindex = result.GetPortalConstControl().Get(randomindex);
							vtkm::Float64 lo = range.Min + (static_cast<vtkm::Float64>(blkindex) * delta);
							vtkm::Float64 hi = lo + delta;
							vtkm::Float64 average = (lo + hi) / 2;
							int i = d0 * vdims[2] * vdims[1] + d1 * vdims[2] + d2;
							ValueHandle.GetPortalControl().Set(i, average);
							randomindex += 1;
						}
					}
				}
			}
		}
	}

	FILE* fOut = fopen("ouput2.bin", "wb");
	std::cout << ValueHandle.GetPortalConstControl().GetNumberOfValues() << std::endl;
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
	return vtkm::cont::testing::Testing::Run(TestHistogram, argc, argv);
}
