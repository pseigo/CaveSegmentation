#include <ICaveData.h>

#include <ChamberAnalyzation/CurvatureBasedQPBO.h>
#include <ChamberAnalyzation/Utils.h>
#include <ChamberAnalyzation/energies.h>
#include <FileInputOutput.h>

#include <boost/filesystem.hpp>

#include <iostream>
#include <CurveSkeleton.h>

void PrintHelp()
{
	std::cout << "Usage: CaveSegmentationCommandLine [options]" << std::endl;
	std::cout << "Obligatory Options: " << std::endl;
	std::cout << "\t-d [dataDirectory]     The data directory must contain a \"model.off\"." << std::endl;	
	std::cout << "\t                       The skeleton location is \"[dataDirectory]/model.skel\"." << std::endl;
	std::cout << "\t                       The distance data location is \"[dataDirectory]/distances.bin\"." << std::endl;
	std::cout << "Optional Options: " << std::endl;
	std::cout << "\t--calcSkel             Specify this to calculate the skeleton and save it to file. Otherwise, it will be loaded from a file." << std::endl;
	std::cout << "\t--calcDist             Specify this to calculate distance data and save them to file. Otherwise, they will be loaded from a file." << std::endl;
	std::cout << "\t--edgeLength [float]   Specify the edge collapse threshold for skeleton calculation in percent of the model's bounding box diagonal." << std::endl;
	std::cout << "\t--wsmooth [float]      Specify the smoothing weight for skeleton calculation." << std::endl;
	std::cout << "\t--wvelocity [float]    Specify the velocity weight for skeleton calculation." << std::endl;
	std::cout << "\t--wmedial [float]      Specify the medial weight for skeleton calculation." << std::endl;
	std::cout << "\t--exp [float]          Specify the exponent for distance calculation." << std::endl;
	std::cout << "\t--scaleKernel [float]  Specify the width of the cave scale kernel (mu_scale from paper)." << std::endl;
	std::cout << "\t--sizeKernel [float]   Specify the width of the cave size kernel (mu_size from the paper)." << std::endl;
	std::cout << "\t--derivKernel [float]  Specify the width of the cave size derivative kernel (mu_size' from the paper)." << std::endl;
	std::cout << "\t--curvatureTip [float] Specify the curvature tipping point (theta_tip from the paper)." << std::endl;
	std::cout << "\t--dirTol [float]       Specify the direction tolerance (theta_dir from the paper)." << std::endl;
	std::cout << "All output will be saved in \"[dataDirectory]/output\"." << std::endl;
}

int main(int argc, char* argv[])
{	
	bool calculateSkeleton = false;
	bool calculateDistances = false;
	std::string dataDirectory;

	float edgeCollapseThresholdPercent = 0.2f;
	float w_smooth = 1.0f;
	float w_velocity = 20.0f;
	float w_medial = 1.0f;

	float exponent = 1.0f;

	auto data = CreateCaveData();

	if (argc < 2)
	{
		PrintHelp();

		return -1;
	}
	else
	{
		for (int i = 1; i < argc; ++i)
		{
			if (strcmp(argv[i], "-d") == 0)
			{
				dataDirectory = std::string(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--calcSkel") == 0)
				calculateSkeleton = true;
			else if (strcmp(argv[i], "--calcDist") == 0)
				calculateDistances = true;
			else if (strcmp(argv[i], "--edgeLength") == 0)
			{
				edgeCollapseThresholdPercent = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--wsmooth") == 0)
			{
				w_smooth = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--wvelocity") == 0)
			{
				w_velocity = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--wmedial") == 0)
			{
				w_medial = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--exp") == 0)
			{
				exponent = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--scaleKernel") == 0)
			{
				data->CaveScaleKernelFactor() = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--sizeKernel") == 0)
			{
				data->CaveSizeKernelFactor() = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--derivKernel") == 0)
			{
				data->CaveSizeDerivativeKernelFactor() = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--curvatureTip") == 0)
			{
				Energies::CURVATURE_TIP_POINT = std::stof(argv[i + 1]);
				++i;
			}
			else if (strcmp(argv[i], "--dirTol") == 0)
			{
				Energies::DIRECTION_TOLERANCE = std::stof(argv[i + 1]);
				++i;
			}
		}
	}

	if (dataDirectory.empty())
	{
		std::cout << "You did not specify a data directory." << std::endl;
		PrintHelp();
		return 1;
	}

	const std::string outputDirectory = dataDirectory + "/output";

	std::wstring outputDirectoryW(outputDirectory.length(), L' ');
	std::copy(outputDirectory.begin(), outputDirectory.end(), outputDirectoryW.begin());
	
	boost::filesystem::path outputDir(outputDirectory);
	boost::filesystem::create_directory(outputDir);

	const std::string offFile = dataDirectory + "/model.off";
	const std::string skeletonFile = dataDirectory + "/model.skel";
	const std::string distancesFile = dataDirectory + "/distances.bin";

	const std::string calculatedSkeletonFile = outputDirectory + "/skeleton.obj";
	const std::string calculatedSkeletonCorrFile = outputDirectory + "/skeletonCorr.obj";
	const std::string segmentationFile = outputDirectory + "/segmentation.seg";

	std::cout << "Model file: " << offFile << std::endl;
	
	try
	{
		data->LoadMesh(offFile);
	}
	catch (...)
	{
		std::cerr << "Cannot load mesh from " << offFile << std::endl;
		return 2;
	}
	data->SetOutputDirectory(outputDirectoryW);

	ICaveData::StartCaveSeg();	

	CurveSkeleton* skeleton;
	if (calculateSkeleton)
	{
		std::cout << "Calculating skeleton..." << std::endl;
		AbortHandle abort;
		float edgeCollapseThreshold = edgeCollapseThresholdPercent * (data->GetMax() - data->GetMin()).norm() / 100.0f;
		std::cout << "Edge collapse threshold: " << edgeCollapseThreshold << std::endl;
		std::cout << "w_smooth: " << w_smooth << std::endl;
		std::cout << "w_velocity: " << w_velocity << std::endl;
		std::cout << "w_medial: " << w_medial << std::endl;
		skeleton = ComputeCurveSkeleton(offFile, &abort, edgeCollapseThreshold, w_smooth, w_velocity, w_medial);
		std::cout << "Saving skeleton to " << skeletonFile << std::endl;
		skeleton->Save(skeletonFile.c_str());
		std::cout << "Saving skeleton OBJ to " << calculatedSkeletonFile << std::endl;
		skeleton->SaveToObj(calculatedSkeletonFile.c_str());
		std::cout << "Saving skeleton OBJ with correspondences to " << calculatedSkeletonCorrFile << std::endl;
		skeleton->SaveToObjWithCorrespondences(calculatedSkeletonCorrFile.c_str(), offFile);
	}
	else
	{
		try
		{
			std::cout << "Loading skeleton from " << skeletonFile << std::endl;
			skeleton = LoadCurveSkeleton(skeletonFile.c_str());
		}
		catch (std::exception& e)
		{
			std::cerr << "Cannot load skeleton from " << skeletonFile << ": " << e.what() << std::endl;
			return 3;
		}
	}
	data->SetSkeleton(skeleton);
	
	
	if (calculateDistances)
	{		
		std::cout << "Calculating distances..." << std::endl;
		std::cout << "Using exponent " << exponent << std::endl;
		data->CalculateDistances(exponent);

		std::cout << "Saving distances to " << distancesFile << std::endl;
		data->SaveDistances(distancesFile);
	}
	else
	{
		try
		{
			std::cout << "Loading distances from " << distancesFile << std::endl;
			data->LoadDistances(distancesFile);
		}
		catch (std::exception& e)
		{
			std::cerr << "Cannot load distances from " << distancesFile << ": " << e.what() << std::endl;
			return 4;
		}
	}
	std::cout << "Smoothing distances and deriving additional measures..." << std::endl;
	std::cout << "Using cave scale kernel factor " << data->CaveScaleKernelFactor() << std::endl;
	std::cout << "Using cave size kernel factor " << data->CaveSizeKernelFactor() << std::endl;
	std::cout << "Using cave size derivative kernel factor " << data->CaveSizeDerivativeKernelFactor() << std::endl;
	data->SmoothAndDeriveDistances();
	
	std::cout << "Finding chambers..." << std::endl;
	std::cout << "Using curvature tipping point " << Energies::CURVATURE_TIP_POINT << std::endl;
	std::cout << "Using direction tolerance " << Energies::DIRECTION_TOLERANCE << std::endl;

	std::vector<int> segmentation;
	CurvatureBasedQPBO::FindChambers(*data, segmentation);

	AssignUniqueChamberIndices(*data, segmentation);
	
	std::cout << "Writing segmentation to " << segmentationFile << std::endl;
	WriteSegmentation(segmentationFile, segmentation);

	auto colorFunc = [&](int i, int& r, int& g, int& b)
	{
		auto color = ICaveData::GetSegmentColor(segmentation[i]);
		r = color[0];
		g = color[1];
		b = color[2];

		/* curvature visualization
		r = g = b = 100;	
		if (data.caveSizeCurvatures[i] > 0)
		{
			r += (int)(data.caveSizeCurvatures[i] * 155 / 0.2);
			if (r > 255)
				r = 255;
		}*/
		/* size visualization 
		r = g = b = (int)(data.caveSizes[i] * 255 / 30); */
		/* entrance prob visualization 
		r = g = b = 0;
		double prob = entranceProbability(data.caveSizeCurvatures[i]);
		if (prob > 0.5)
			r = 128 + 255 * (prob - 0.5);
		else
			b = 128 + 255 * prob;*/
		
	};
	auto colorVertexFunc = [&](const CurveSkeleton::Vertex& v, int i, int& r, int& g, int& b){ colorFunc(i, r, g, b); };

	std::string corrsFile = outputDirectory + "/Correspondences.obj";
	std::string skeletonObjFile = outputDirectory + "/Skeleton.obj";
	std::string skeletonHeightCodedFile = outputDirectory + "/SkeletonWithHeightCodedSize.obj";
	std::string segmentedMeshFile = outputDirectory + "/segmentedMesh.off";

	std::cout << "Saving skeleton with colored correspondences to " << corrsFile << std::endl;
	skeleton->SaveToObjWithCorrespondences(corrsFile.c_str(), offFile, colorVertexFunc);

	std::cout << "Saving colored skeleton to " << skeletonObjFile << std::endl;
	skeleton->SaveToObj(skeletonObjFile.c_str(), colorVertexFunc);
	std::cout << "Saving skeleton with cave size encoded as z-coordinates to " << skeletonHeightCodedFile << std::endl;
	skeleton->SaveToObj(skeletonHeightCodedFile.c_str(), colorVertexFunc, [&](const CurveSkeleton::Vertex& v, int i, float& newX, float& newY, float& newZ) { newX = v.position.x(), newY = v.position.y(), newZ = (float)data->CaveSize(i); });

	std::cout << "Saving segmented model to " << segmentedMeshFile << std::endl;

	data->WriteMesh(segmentedMeshFile.c_str(), [&](int i, int& r, int& g, int& b) {colorFunc(data->MeshVertexCorrespondsTo(i), r, g, b); } );

	DestroySkeleton(skeleton);

	ICaveData::StopCaveSeg();

	return 0;
}