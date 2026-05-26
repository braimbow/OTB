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

#ifndef otbBiomassCalibrationLookupData_h
#define otbBiomassCalibrationLookupData_h

#include "OTBMetadataExport.h"
#include "otbSarCalibrationLookupData.h"

#include <string>
#include <vector>

namespace otb
{

/** \class BiomassCalibrationLookupData
 *
 * \brief ESA BIOMASS L1 calibration lookup table.
 *
 * BIOMASS SCS pixels are delivered as beta-nought complex samples. The
 * radiometry NetCDF variables sigmaNought and gammaNought contain the
 * multiplicative factors that convert beta-nought power to sigma-nought or
 * gamma-nought power. OTB SARCalibration divides power by LUT^2, so this class
 * exposes 1 / sqrt(factor) to keep the existing calibration filter unchanged.
 *
 * \ingroup OTBMetadata
 */
class OTBMetadata_EXPORT BiomassCalibrationLookupData : public SarCalibrationLookupData
{
public:
  typedef BiomassCalibrationLookupData      Self;
  typedef SarCalibrationLookupData          Superclass;
  typedef itk::SmartPointer<Self>           Pointer;
  typedef itk::SmartPointer<const Self>     ConstPointer;

  itkNewMacro(Self);
  itkTypeMacro(BiomassCalibrationLookupData, SarCalibrationLookupData);

  typedef Superclass::IndexValueType IndexValueType;

  void Initialize(short type,
                  double imageFirstAzimuthTime,
                  double imageAzimuthTimeSpacing,
                  double imageFirstSlantRangeTime,
                  double imageSlantRangeTimeSpacing,
                  std::vector<double> lutAzimuthTimes,
                  std::vector<double> lutSlantRangeTimes,
                  std::vector<float> lutValues,
                  bool valuesAreBetaToTargetFactors = true);

  void InitializeFromNetCDF(short type,
                            double imageFirstAzimuthTime,
                            double imageAzimuthTimeSpacing,
                            double imageFirstSlantRangeTime,
                            double imageSlantRangeTimeSpacing,
                            const std::string& sourceFile,
                            const std::string& sourceVariable,
                            bool valuesAreBetaToTargetFactors = true);

  double GetValue(const IndexValueType x, const IndexValueType y) const override;

  void ToKeywordlist(MetaData::Keywordlist& kwl, const std::string& prefix) const override;

  void FromKeywordlist(const MetaData::Keywordlist& kwl, const std::string& prefix) override;

protected:
  BiomassCalibrationLookupData() = default;
  ~BiomassCalibrationLookupData() override = default;

private:
  BiomassCalibrationLookupData(const Self&) = delete;
  void operator=(const Self&) = delete;

  void LoadFromNetCDF(const std::string& sourceFile, const std::string& sourceVariable);
  double InterpolateFactor(double azimuthTime, double slantRangeTime) const;

  double m_ImageFirstAzimuthTime = 0.0;
  double m_ImageAzimuthTimeSpacing = 1.0;
  double m_ImageFirstSlantRangeTime = 0.0;
  double m_ImageSlantRangeTimeSpacing = 1.0;
  bool m_ValuesAreBetaToTargetFactors = true;

  std::string m_SourceFile;
  std::string m_SourceVariable;
  std::vector<double> m_LUTAzimuthTimes;
  std::vector<double> m_LUTSlantRangeTimes;
  std::vector<float> m_LUTValues;
};

} // end namespace otb

#endif
