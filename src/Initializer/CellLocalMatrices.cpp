/**
 * @file
 * This file is part of SeisSol.
 *
 * @author Carsten Uphoff (c.uphoff AT tum.de, http://www5.in.tum.de/wiki/index.php/Carsten_Uphoff,_M.Sc.)
 *
 * @section LICENSE
 * Copyright (c) 2015, SeisSol Group
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 * Setup of SeisSol's cell local matrices.
 **/

#include "CellLocalMatrices.h"
#include <Numerical_aux/Transformation.h>
#include <Model/Setup.h>
#include <Geometry/MeshTools.h>
#include <generated_code/init.h>

void setStarMatrix( real* i_AT,
                    real* i_BT,
                    real* i_CT,
                    real  i_grad[3],
                    real* o_starMatrix )
{
  for (unsigned idx = 0; idx < seissol::model::AstarT::reals; ++idx) {
    o_starMatrix[idx] = i_grad[0] * i_AT[idx];
  }
  
  for (unsigned idx = 0; idx < seissol::model::BstarT::reals; ++idx) {
    o_starMatrix[idx] += i_grad[1] * i_BT[idx];
  }
  
  for (unsigned idx = 0; idx < seissol::model::CstarT::reals; ++idx) {
    o_starMatrix[idx] += i_grad[2] * i_CT[idx];
  }
}

void seissol::initializers::initializeCellLocalMatrices( MeshReader const&      i_meshReader,                                                    
                                                         LTSTree*               io_ltsTree,
                                                         LTS*                   i_lts,
                                                         Lut*                   i_ltsLut )
{
  std::vector<Element> const& elements = i_meshReader.getElements();
  std::vector<Vertex> const& vertices = i_meshReader.getVertices();
  
  assert(seissol::model::AplusT::rows == seissol::model::AminusT::rows);
  assert(seissol::model::AplusT::cols == seissol::model::AminusT::cols);
  assert(seissol::model::AplusT::rows == seissol::model::AplusT::cols);
  
  real AT[seissol::model::AstarT::reals];
  real BT[seissol::model::BstarT::reals];
  real CT[seissol::model::CstarT::reals];
  real FlocalData[seissol::model::AplusT::rows * seissol::model::AplusT::cols];
  real FneighborData[seissol::model::AminusT::rows * seissol::model::AminusT::cols];
  real TData[seissol::model::AplusT::rows * seissol::model::AplusT::cols];
  real TinvData[seissol::model::AplusT::rows * seissol::model::AplusT::cols];

  unsigned* ltsToMesh = i_ltsLut->getLtsToMeshLut(i_lts->material.mask);
  
  assert(LayerMask(Ghost) == i_lts->material.mask);
  assert(LayerMask(Ghost) == i_lts->localIntegration.mask);
  assert(LayerMask(Ghost) == i_lts->neighboringIntegration.mask);
  
  assert(ltsToMesh      == i_ltsLut->getLtsToMeshLut(i_lts->localIntegration.mask));
  assert(ltsToMesh      == i_ltsLut->getLtsToMeshLut(i_lts->neighboringIntegration.mask));
  
  for (LTSTree::leaf_iterator it = io_ltsTree->beginLeaf(LayerMask(Ghost)); it != io_ltsTree->endLeaf(); ++it) {
    CellMaterialData*           material                = it->var(i_lts->material);
    LocalIntegrationData*       localIntegration        = it->var(i_lts->localIntegration);
    NeighboringIntegrationData* neighboringIntegration  = it->var(i_lts->neighboringIntegration);
    CellLocalInformation*       cellInformation         = it->var(i_lts->cellInformation);
    
#ifdef _OPENMP
  #pragma omp parallel for private(AT, BT, CT, FlocalData, FneighborData, TData, TinvData) schedule(static)
#endif
    for (unsigned cell = 0; cell < it->getNumberOfCells(); ++cell) {
      unsigned meshId = ltsToMesh[cell];
      
      real x[4];
      real y[4];
      real z[4];
      real gradXi[3];
      real gradEta[3];
      real gradZeta[3];
      
      // Iterate over all 4 vertices of the tetrahedron
      for (unsigned vertex = 0; vertex < 4; ++vertex) {
        VrtxCoords const& coords = vertices[ elements[meshId].vertices[vertex] ].coords;
        x[vertex] = coords[0];
        y[vertex] = coords[1];
        z[vertex] = coords[2];
      }
      
      seissol::transformations::tetrahedronGlobalToReferenceJacobian( x, y, z, gradXi, gradEta, gradZeta );

      seissol::model::getTransposedCoefficientMatrix( material[cell].local, 0, AT );
      seissol::model::getTransposedCoefficientMatrix( material[cell].local, 1, BT );
      seissol::model::getTransposedCoefficientMatrix( material[cell].local, 2, CT );
      setStarMatrix(AT, BT, CT, gradXi, localIntegration[cell].starMatrices[0]);
      setStarMatrix(AT, BT, CT, gradEta, localIntegration[cell].starMatrices[1]);
      setStarMatrix(AT, BT, CT, gradZeta, localIntegration[cell].starMatrices[2]);
      
      double volume = MeshTools::volume(elements[meshId], vertices);

      for (unsigned side = 0; side < 4; ++side) {
        DenseMatrixView<seissol::model::AplusT::rows, seissol::model::AplusT::cols> Flocal(FlocalData);
        DenseMatrixView<seissol::model::AminusT::rows, seissol::model::AminusT::cols> Fneighbor(FneighborData);
        DenseMatrixView<seissol::model::AplusT::rows, seissol::model::AplusT::cols> T(TData);
        DenseMatrixView<seissol::model::AplusT::cols, seissol::model::AplusT::rows> Tinv(TinvData);

        seissol::model::getTransposedRiemannSolver( material[cell].local,
                                                    material[cell].neighbor[side],
                                                    cellInformation[cell].faceTypes[side],
                                                    //AT,
                                                    Flocal,
                                                    Fneighbor );

        VrtxCoords normal;
        VrtxCoords tangent1;
        VrtxCoords tangent2;
        MeshTools::normalAndTangents(elements[meshId], side, vertices, normal, tangent1, tangent2);
        double surface = MeshTools::surface(normal);
        MeshTools::normalize(normal, normal);
        MeshTools::normalize(tangent1, tangent1);
        MeshTools::normalize(tangent2, tangent2);

        // Calculate transposed T instead
        seissol::model::getFaceRotationMatrix(normal, tangent1, tangent2, T, Tinv);
        
        MatrixView nApNm1(localIntegration[cell].nApNm1[side], seissol::model::AplusT::reals, seissol::model::AplusT::index);
        MatrixView nAmNm1(neighboringIntegration[cell].nAmNm1[side], seissol::model::AminusT::reals, seissol::model::AminusT::index);
        
        nApNm1.setZero();
        nAmNm1.setZero();
        
        // Scale with |S_side|/|J| and multiply with -1 as the flux matrices
        // must be subtracted.
        real fluxScale = -2.0 * surface / (6.0 * volume);
        
        // \todo Generate a kernel for this
        // Calculates  Tinv^T * F * T^T
        for (unsigned j = 0; j < seissol::model::AplusT::cols; ++j) {
          for (unsigned i = 0; i < 9; ++i) {
            for (unsigned k = 0; k < seissol::model::AplusT::rows; ++k) {
              for (unsigned l = 0; l < seissol::model::AplusT::cols; ++l) {
                nApNm1(i, j) += Tinv(k, i) * Flocal(k, l) * T(j, l);
                nAmNm1(i, j) += Tinv(k, i) * Fneighbor(k, l) * T(j, l);
              }
            }
            nApNm1(i, j) *= fluxScale;
            nAmNm1(i, j) *= fluxScale;
          }
        }
      }

      seissol::model::initializeSpecificLocalData(  material[cell].local,
                                                    &localIntegration[cell].specific );

      seissol::model::initializeSpecificNeighborData( material[cell].local,
                                                      material[cell].neighbor,
                                                      &neighboringIntegration[cell].specific );
    }
    
    ltsToMesh += it->getNumberOfCells();
  }
}
