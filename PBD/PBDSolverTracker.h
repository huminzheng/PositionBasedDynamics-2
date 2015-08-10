#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

#include <Eigen\Dense>

struct PBDSolverTracker
{
	PBDSolverTracker()
	{
		filenameS = "SecondPiolaKirchoffTensor.m";
	}

	std::string filenameS;
	std::vector<Eigen::Matrix3f> S;

	void writeMatlabFile_S()
	{
		array3d_2_matlab(S, filenameS, "S");
	}

	void writeAll()
	{
		std::cout << "WRITING TRACKED SOLVER DATA..." << std::endl;
		writeMatlabFile_S();

		std::cout << "-------------------" << std::endl;
	}

private:
	bool array3d_2_matlab(const std::vector<Eigen::Matrix3f>& cArray, const std::string& fileName, const std::string& arrayName)
	{
		std::ofstream file;
		file.open(fileName);
		if (!file.is_open())
		{
			std::cout << "ERROR: Could not write [ " << fileName << " ]." << std::endl;
			return false;
		}

		file << arrayName << " = zeros(3, 3, " << cArray.size() << ");" << std::endl;
		for (int d = 0; d < cArray.size(); ++d)
		{
			file << arrayName << "(:, :, " << d + 1 << ") = ";
			array2d_to_matlab(cArray[d], file);
		}

		file.close();
		file.clear();

		return true;
	}


	void array2d_to_matlab(const Eigen::Matrix3f& cArray, std::ofstream& file)
	{
		file << "[ ";
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				file << cArray(row, col);

				if (col == 2)
				{
					file << ";" << std::endl;
				}
				else
				{
					file << ", ";
				}
			}

		}
		file << "];" << std::endl;
	}
};