#include "PBDGPU_Solver.h"

#include <iostream>
#include <memory>
#include <vector>

#include "Parameters.h"
#include "CUDA_WRAPPER.h"

PBDGPU_Solver::PBDGPU_Solver()
{
	m_isSetup = false;
}


PBDGPU_Solver::~PBDGPU_Solver()
{
	std::cout << "Destroying CUDA Solver..." << std::endl;
}

void
PBDGPU_Solver::determineCUDALaunchParameters(int numParticles)
{
	CUDA_TRUE_NUM_CONSTRAINTS = numParticles;

	CUDA_NUM_THREADS_PER_BLOCK = 64;

	CUDA_NUM_BLOCKS = (numParticles / CUDA_NUM_THREADS_PER_BLOCK) + 1;

	CUDA_NUM_PARTICLES = CUDA_NUM_BLOCKS * CUDA_NUM_THREADS_PER_BLOCK;

	std::cout << "Determined (from " << numParticles << " tendered tetrahedra):" << std::endl;
	std::cout << "	NUM_BLOCKS           : " << CUDA_NUM_BLOCKS << std::endl;
	std::cout << "	NUM_THREADS_PER_BLOCK: " << CUDA_NUM_THREADS_PER_BLOCK << std::endl;
	std::cout << "	This adds " << CUDA_NUM_PARTICLES - numParticles << " to the solver." << std::endl;
}

void
PBDGPU_Solver::setup(std::vector<PBDTetrahedra3d>& tetrahedra,
std::shared_ptr<std::vector<PBDParticle>>& particles)
{
	//0. Determine CUDA Launch parameters
	determineCUDALaunchParameters(tetrahedra.size());

	//1. Inverse masses
	for (int i = 0; i < tetrahedra.size(); ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			m_inverseMasses.push_back((*particles)[tetrahedra[i].getVertexIndices()[j]].inverseMass());
		}
	}


	//2. Indices
	for (int i = 0; i < tetrahedra.size(); ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			m_indices.push_back(tetrahedra[i].getVertexIndices()[j]);
		}
	}


	//3. Undeformed Volume
	for (int i = 0; i < tetrahedra.size(); ++i)
	{
		m_undeformedVolumes.push_back(tetrahedra[i].getUndeformedVolume());
	}

	Eigen::Matrix3f temp;
	//4. Reference Shape Matrices
	for (int i = 0; i < tetrahedra.size(); ++i)
	{
		temp = tetrahedra[i].getReferenceShapeMatrixInverseTranspose().transpose();
		//std::cout << "HOST RefShape [" << i << "]: " << std::endl;
		//std::cout << temp << std::endl;
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				//std::cout << tetrahedra[i].getReferenceShapeMatrixInverseTranspose()(row, col) << ", ";
				m_referenceShapeMatrices.push_back(temp(row, col));
			}
		}

		//std::cout << std::endl;
	}

	m_positions.resize(m_inverseMasses.size());

	queryCUDADevices();

	std::cout << "CUDA Solver successfully initialised..." << std::endl;
	m_isSetup = true;
}

void
PBDGPU_Solver::advanceSystem(std::shared_ptr<std::vector<PBDParticle>>& particles,
Parameters& settings)
{
	//check that the system is set up correctly
	if (!m_isSetup)
	{
		printError("Cannot advance system, setup() must be called first!");
	}

	advanceVelocities(particles, settings);
	advancePositions(particles, settings);

	//1. Get Positions
	for (int i = 0; i < particles->size(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			m_positions[i * 3 + j] = (*particles)[i].position()[j];
		}
	}

	//2. Determine Settings
	settings.numBlocks = CUDA_NUM_BLOCKS;
	settings.numThreadsPerBlock = CUDA_NUM_THREADS_PER_BLOCK;
	settings.trueNumberOfConstraints = CUDA_TRUE_NUM_CONSTRAINTS;

	//3. Advance System
	settings.calculateMu();
	settings.calculateLambda();
	//std::cout << "Solving System with CUDA..." << std::endl;
	CUDA_projectConstraints(m_indices, m_positions, m_inverseMasses,
		m_referenceShapeMatrices, m_undeformedVolumes, settings);

	//4. Copy Positions back
	//std::cout << "Copying Positions back to particles..." << std::endl;

	for (int i = 0; i < particles->size(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			(*particles)[i].position()[j] = m_positions[i * 3 + j];
		}
	}

	//std::cout << "...done" << std::endl;

	//Update Velocities
	updateVelocities(particles, settings);

	//swap particles states
	for (auto& p : *particles)
	{
		p.swapStates();
	}
}

void
PBDGPU_Solver::printError(const std::string& message)
{
	std::cerr << "GPUPBD Solver Error: " << message << std::endl;
}

void
PBDGPU_Solver::advanceVelocities(std::shared_ptr<std::vector<PBDParticle>>& particles,
const Parameters& settings)
{
	for (auto& p : *particles)
	{
		float temp = settings.timeStep * p.inverseMass() * settings.gravity;
		p.velocity().x() = p.previousVelocity().x() + 0;
		p.velocity().y() = p.previousVelocity().y() + temp;
		p.velocity().z() = p.previousVelocity().z() + 0;
	}
}

void
PBDGPU_Solver::advancePositions(std::shared_ptr<std::vector<PBDParticle>>& particles,
const Parameters& settings)
{
	for (auto& p : *particles)
	{
		p.position() = p.previousPosition() + settings.timeStep * p.velocity();
		//std::cout << p.velocity().transpose() << std::endl;
	}
}

void
PBDGPU_Solver::updateVelocities(std::shared_ptr<std::vector<PBDParticle>>& particles,
const Parameters& settings)
{
	for (auto& p : *particles)
	{
		p.velocity() = (1.0 / settings.timeStep) * (p.position() - p.previousPosition());
	}
}
