#include <vtkm/cont/Initialize.h>
#include <vtkm/cont/Algorithm.h>
#include <vtkm/cont/testing/Testing.h>
#include <vtkm/worklet/AplusB.h>

#include <stdio.h>
using namespace std;
void TestPlus()
{
    vector<vtkm::Float32> inpBuffer{2, 1, 0, 3, 5, 4, 6, 7, 10, 9};
    vector<vtkm::Float32> keyBuffer{1, 1, 1, 4, 4, 4, 5, 8, 8, 1};

    vtkm::cont::ArrayHandle<vtkm::Float32> keys = vtkm::cont::make_ArrayHandle(keyBuffer);
    vtkm::cont::ArrayHandle<vtkm::Float32> inps = vtkm::cont::make_ArrayHandle(inpBuffer);

    cout<<"Before keys: ";
    for(vtkm::Float32 i =0; i < keys.GetNumberOfValues(); i++)
    {
        cout<<keys.GetPortalConstControl().Get(i)<<" ";
    }
	cout << endl;
    cout<<"Before inps: ";
    for(vtkm::Float32 i =0; i < inps.GetNumberOfValues(); i++)
    {
        cout<<inps.GetPortalConstControl().Get(i)<<" ";
    }

	vtkm::cont::ArrayHandle<vtkm::Float32> ans;

    vtkm::worklet::AplusB aPlusb;
	aPlusb.Run(keys, inps, ans);
	cout << endl;

	for (vtkm::Float32 i = 0; i < ans.GetNumberOfValues(); i++)
	{
		cout << ans.GetPortalConstControl().Get(i) << " ";
	}
    
}
int main(int argc, char* argv[])
{
	return vtkm::cont::testing::Testing::Run(TestPlus, argc, argv);
}


