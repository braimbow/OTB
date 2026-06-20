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

#ifndef otbCapellaCalibrationLookupData_h
#define otbCapellaCalibrationLookupData_h

#include "OTBMetadataExport.h"
#include "otbSarCalibrationLookupData.h"

#include <vector>

namespace otb
{

/** \class CapellaCalibrationLookupData
 *
 * \brief CAPELLA SICD radiometric polynomial lookup helper.
 *
 * CAPELLA GeoTIFF products expose SICD radiometric scale factor polynomials.
 * OTB SARCalibration divides power by LUT^2, so for sigma/beta/gamma scale
 * factors this class returns 1 / sqrt(scale_factor). For noise it returns the
 * SICD absolute noise polynomial value directly.
 *
 * \ingroup OTBMetadata
 */
class OTBMetadata_EXPORT CapellaCalibrationLookupData : public SarCalibrationLookupData
{
public:
  typedef CapellaCalibrationLookupData  Self;
  typedef SarCalibrationLookupData      Superclass;
  typedef itk::SmartPointer<Self>       Pointer;
  typedef itk::SmartPointer<const Self> ConstPointer;

  itkNewMacro(Self);
  itkTypeMacro(CapellaCalibrationLookupData, SarCalibrationLookupData);

  typedef Superclass::IndexValueType IndexValueType;

  CapellaCalibrationLookupData();
  ~CapellaCalibrationLookupData() override = default;

  void Initialize(short type,
                  std::vector<double> coefficients,
                  unsigned int rowCoefficientCount,
                  unsigned int columnCoefficientCount,
                  double rowReferencePixel,
                  double columnReferencePixel,
                  double rowSpacing,
                  double columnSpacing);

  double GetValue(const IndexValueType x, const IndexValueType y) const override;

  void ToKeywordlist(MetaData::Keywordlist& kwl, const std::string& prefix) const override;
  void FromKeywordlist(const MetaData::Keywordlist& kwl, const std::string& prefix) override;

private:
  CapellaCalibrationLookupData(const Self&) = delete;
  void operator=(const Self&) = delete;

  double EvaluateScaleFactor(double x, double y) const;

  std::vector<double> m_Coefficients;
  unsigned int m_RowCoefficientCount;
  unsigned int m_ColumnCoefficientCount;
  double m_RowReferencePixel;
  double m_ColumnReferencePixel;
  double m_RowSpacing;
  double m_ColumnSpacing;
};

} // end namespace otb

#endif
