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

#include "otbCapellaCalibrationLookupData.h"

#include "otbMacro.h"
#include "otbStringUtilities.h"

#include <boost/lexical_cast.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace
{
constexpr int STRING_PRECISION = 20;

std::string ToStringWithPrecision(double value)
{
  std::ostringstream oss;
  oss << std::setprecision(STRING_PRECISION) << value;
  return oss.str();
}

std::string DoubleVectorToString(const std::vector<double>& input)
{
  std::ostringstream oss;
  oss << std::setprecision(STRING_PRECISION);
  for (const auto value : input)
  {
    oss << value << " ";
  }
  return oss.str();
}

std::vector<double> StringToDoubleVector(const std::string& input)
{
  std::vector<double> output;
  const auto parts = otb::split_on(input, ' ');
  for (const auto& elem : parts)
  {
    if (!elem.empty())
    {
      output.push_back(otb::to<double>(elem, "Cannot cast CAPELLA calibration coefficient"));
    }
  }
  return output;
}

double Power(double value, unsigned int exponent)
{
  double output = 1.0;
  for (unsigned int i = 0; i < exponent; ++i)
  {
    output *= value;
  }
  return output;
}

} // end anonymous namespace

namespace otb
{

CapellaCalibrationLookupData::CapellaCalibrationLookupData()
  : m_RowCoefficientCount(0),
    m_ColumnCoefficientCount(0),
    m_RowReferencePixel(0.0),
    m_ColumnReferencePixel(0.0),
    m_RowSpacing(1.0),
    m_ColumnSpacing(1.0)
{
}

void CapellaCalibrationLookupData::Initialize(short type,
                                              std::vector<double> coefficients,
                                              unsigned int rowCoefficientCount,
                                              unsigned int columnCoefficientCount,
                                              double rowReferencePixel,
                                              double columnReferencePixel,
                                              double rowSpacing,
                                              double columnSpacing)
{
  this->SetType(type);
  m_Coefficients = std::move(coefficients);
  m_RowCoefficientCount = rowCoefficientCount;
  m_ColumnCoefficientCount = columnCoefficientCount;
  m_RowReferencePixel = rowReferencePixel;
  m_ColumnReferencePixel = columnReferencePixel;
  m_RowSpacing = rowSpacing;
  m_ColumnSpacing = columnSpacing;

  const auto expectedSize = static_cast<std::size_t>(m_RowCoefficientCount) * static_cast<std::size_t>(m_ColumnCoefficientCount);
  if (expectedSize == 0 || expectedSize != m_Coefficients.size())
  {
    otbGenericExceptionMacro(itk::ExceptionObject,
                             << "Invalid CAPELLA calibration polynomial size: got "
                             << m_Coefficients.size() << " values for "
                             << m_RowCoefficientCount << " row coefficients and "
                             << m_ColumnCoefficientCount << " column coefficients");
  }
}

double CapellaCalibrationLookupData::EvaluateScaleFactor(double x, double y) const
{
  if (m_Coefficients.empty())
  {
    return 1.0;
  }

  // SICD radiometric polynomials are evaluated in image-grid row/column
  // coordinates relative to SCPPixel and scaled by Grid.Row/Col.SS.
  const double rowCoordinate = (x - m_RowReferencePixel) * m_RowSpacing;
  const double columnCoordinate = (y - m_ColumnReferencePixel) * m_ColumnSpacing;

  double value = 0.0;
  for (unsigned int rowOrder = 0; rowOrder < m_RowCoefficientCount; ++rowOrder)
  {
    for (unsigned int columnOrder = 0; columnOrder < m_ColumnCoefficientCount; ++columnOrder)
    {
      const auto index = static_cast<std::size_t>(rowOrder) * m_ColumnCoefficientCount + columnOrder;
      value += m_Coefficients[index] * Power(rowCoordinate, rowOrder) * Power(columnCoordinate, columnOrder);
    }
  }
  return value;
}

double CapellaCalibrationLookupData::GetValue(const IndexValueType x, const IndexValueType y) const
{
  if (this->GetType() == DN)
  {
    return 1.0;
  }

  const double scaleFactor = this->EvaluateScaleFactor(static_cast<double>(x), static_cast<double>(y));
  if (this->GetType() == NOISE)
  {
    return std::isfinite(scaleFactor) && scaleFactor > 0.0 ? scaleFactor : 0.0;
  }

  if (!std::isfinite(scaleFactor) || scaleFactor <= 0.0)
  {
    return 1.0;
  }

  return 1.0 / std::sqrt(scaleFactor);
}

void CapellaCalibrationLookupData::ToKeywordlist(MetaData::Keywordlist& kwl, const std::string& prefix) const
{
  kwl.insert({prefix + "Sensor", "Capella"});
  kwl.insert({prefix + "Type", boost::lexical_cast<std::string>(this->GetType())});
  kwl.insert({prefix + "RowCoefficientCount", boost::lexical_cast<std::string>(m_RowCoefficientCount)});
  kwl.insert({prefix + "ColumnCoefficientCount", boost::lexical_cast<std::string>(m_ColumnCoefficientCount)});
  kwl.insert({prefix + "RowReferencePixel", ToStringWithPrecision(m_RowReferencePixel)});
  kwl.insert({prefix + "ColumnReferencePixel", ToStringWithPrecision(m_ColumnReferencePixel)});
  kwl.insert({prefix + "RowSpacing", ToStringWithPrecision(m_RowSpacing)});
  kwl.insert({prefix + "ColumnSpacing", ToStringWithPrecision(m_ColumnSpacing)});
  kwl.insert({prefix + "Coefficients", DoubleVectorToString(m_Coefficients)});
}

void CapellaCalibrationLookupData::FromKeywordlist(const MetaData::Keywordlist& kwl, const std::string& prefix)
{
  Superclass::FromKeywordlist(kwl, prefix);
  m_RowCoefficientCount = static_cast<unsigned int>(std::stoul(kwl.at(prefix + "RowCoefficientCount")));
  m_ColumnCoefficientCount = static_cast<unsigned int>(std::stoul(kwl.at(prefix + "ColumnCoefficientCount")));
  m_RowReferencePixel = std::stod(kwl.at(prefix + "RowReferencePixel"));
  m_ColumnReferencePixel = std::stod(kwl.at(prefix + "ColumnReferencePixel"));
  m_RowSpacing = std::stod(kwl.at(prefix + "RowSpacing"));
  m_ColumnSpacing = std::stod(kwl.at(prefix + "ColumnSpacing"));
  m_Coefficients = StringToDoubleVector(kwl.at(prefix + "Coefficients"));

  const auto expectedSize = static_cast<std::size_t>(m_RowCoefficientCount) * static_cast<std::size_t>(m_ColumnCoefficientCount);
  if (expectedSize != m_Coefficients.size())
  {
    otbGenericExceptionMacro(itk::ExceptionObject,
                             << "Invalid CAPELLA calibration polynomial size while reading keywordlist.");
  }
}

} // end namespace otb
