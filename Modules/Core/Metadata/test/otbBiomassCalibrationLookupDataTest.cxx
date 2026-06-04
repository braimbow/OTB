/*
 * Copyright (C) 2005-2024 Centre National d'Etudes Spatiales (CNES)
 *
 * This file is part of Orfeo Toolbox
 *
 *     https://www.orfeo-toolbox.org/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "itkMacro.h"
#include "otbBiomassCalibrationLookupData.h"

#include <cmath>
#include <iostream>

namespace
{

bool IsClose(double lhs, double rhs, double tol = 1e-6)
{
  return std::abs(lhs - rhs) <= tol;
}

} // end anonymous namespace

int otbBiomassCalibrationLookupDataTest(int, char*[])
{
  auto lut = otb::BiomassCalibrationLookupData::New();
  lut->Initialize(otb::SarCalibrationLookupData::SIGMA,
                  0.0, 1.0,
                  10.0, 10.0,
                  {0.0, 1.0},
                  {10.0, 20.0},
                  {0.25f, 0.36f,
                   0.49f, 0.64f});

  if (!IsClose(lut->GetValue(0, 0), 2.0))
  {
    std::cerr << "Unexpected BIOMASS calibration value at origin" << std::endl;
    return EXIT_FAILURE;
  }
  if (!IsClose(lut->GetValue(1, 1), 1.25))
  {
    std::cerr << "Unexpected BIOMASS calibration value at opposite corner" << std::endl;
    return EXIT_FAILURE;
  }

  otb::MetaData::Keywordlist kwl;
  lut->ToKeywordlist(kwl, "SARCalib.CalibrationLookupData_0_");

  auto roundTrip = otb::BiomassCalibrationLookupData::New();
  roundTrip->FromKeywordlist(kwl, "SARCalib.CalibrationLookupData_0_");
  if (!IsClose(roundTrip->GetValue(0, 0), 2.0) ||
      !IsClose(roundTrip->GetValue(1, 1), 1.25))
  {
    std::cerr << "BIOMASS calibration keywordlist round-trip failed" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
