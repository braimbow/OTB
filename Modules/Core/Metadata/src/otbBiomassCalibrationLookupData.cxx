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

#include "otbBiomassCalibrationLookupData.h"
#include "otbMacro.h"
#include "otbStringUtilities.h"

#include "gdal_priv.h"
#include "cpl_conv.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

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

std::string FloatVectorToString(const std::vector<float>& input)
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
      output.push_back(otb::to<double>(elem, "Cannot cast BIOMASS calibration axis value"));
    }
  }
  return output;
}

std::vector<float> StringToFloatVector(const std::string& input)
{
  std::vector<float> output;
  const auto parts = otb::split_on(input, ' ');
  for (const auto& elem : parts)
  {
    if (!elem.empty())
    {
      output.push_back(otb::to<float>(elem, "Cannot cast BIOMASS calibration LUT value"));
    }
  }
  return output;
}

bool StringToBool(const std::string& value)
{
  return value == "1" || value == "true" || value == "True";
}

std::string BuildNetCDFDatasetName(const std::string& sourceFile, const std::string& variable)
{
  return "NETCDF:\"" + sourceFile + "\":" + variable;
}

std::vector<double> ReadNetCDFVector(const std::string& sourceFile, const std::string& variable)
{
  GDALAllRegister();
  const std::string datasetName = BuildNetCDFDatasetName(sourceFile, variable);
  GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(datasetName.c_str(), GA_ReadOnly));
  if (dataset == nullptr)
  {
    otbGenericExceptionMacro(itk::ExceptionObject, << "Cannot open BIOMASS NetCDF variable " << datasetName);
  }

  const int width = dataset->GetRasterXSize();
  const int height = dataset->GetRasterYSize();
  std::vector<double> values(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  const auto err = dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, width, height,
                                                       values.data(), width, height,
                                                       GDT_Float64, 0, 0, nullptr);
  GDALClose(dataset);

  if (err != CE_None)
  {
    otbGenericExceptionMacro(itk::ExceptionObject, << "Cannot read BIOMASS NetCDF variable " << datasetName);
  }
  return values;
}

} // end anonymous namespace

namespace otb
{

void BiomassCalibrationLookupData::Initialize(short type,
                                              double imageFirstAzimuthTime,
                                              double imageAzimuthTimeSpacing,
                                              double imageFirstSlantRangeTime,
                                              double imageSlantRangeTimeSpacing,
                                              std::vector<double> lutAzimuthTimes,
                                              std::vector<double> lutSlantRangeTimes,
                                              std::vector<float> lutValues,
                                              bool valuesAreBetaToTargetFactors)
{
  this->SetType(type);
  m_ImageFirstAzimuthTime = imageFirstAzimuthTime;
  m_ImageAzimuthTimeSpacing = imageAzimuthTimeSpacing;
  m_ImageFirstSlantRangeTime = imageFirstSlantRangeTime;
  m_ImageSlantRangeTimeSpacing = imageSlantRangeTimeSpacing;
  m_ValuesAreBetaToTargetFactors = valuesAreBetaToTargetFactors;
  m_LUTAzimuthTimes = std::move(lutAzimuthTimes);
  m_LUTSlantRangeTimes = std::move(lutSlantRangeTimes);
  m_LUTValues = std::move(lutValues);
  m_SourceFile.clear();
  m_SourceVariable.clear();

  if (m_LUTValues.size() != m_LUTAzimuthTimes.size() * m_LUTSlantRangeTimes.size())
  {
    otbGenericExceptionMacro(itk::ExceptionObject,
                             << "Invalid BIOMASS calibration LUT size: got " << m_LUTValues.size()
                             << " values for " << m_LUTAzimuthTimes.size() << " azimuth samples and "
                             << m_LUTSlantRangeTimes.size() << " range samples");
  }
}

void BiomassCalibrationLookupData::InitializeFromNetCDF(short type,
                                                        double imageFirstAzimuthTime,
                                                        double imageAzimuthTimeSpacing,
                                                        double imageFirstSlantRangeTime,
                                                        double imageSlantRangeTimeSpacing,
                                                        const std::string& sourceFile,
                                                        const std::string& sourceVariable,
                                                        bool valuesAreBetaToTargetFactors)
{
  this->SetType(type);
  m_ImageFirstAzimuthTime = imageFirstAzimuthTime;
  m_ImageAzimuthTimeSpacing = imageAzimuthTimeSpacing;
  m_ImageFirstSlantRangeTime = imageFirstSlantRangeTime;
  m_ImageSlantRangeTimeSpacing = imageSlantRangeTimeSpacing;
  m_ValuesAreBetaToTargetFactors = valuesAreBetaToTargetFactors;
  m_SourceFile = sourceFile;
  m_SourceVariable = sourceVariable;
  this->LoadFromNetCDF(sourceFile, sourceVariable);
}

void BiomassCalibrationLookupData::LoadFromNetCDF(const std::string& sourceFile, const std::string& sourceVariable)
{
  m_LUTSlantRangeTimes = ReadNetCDFVector(sourceFile, "slantRangeTimeRGC");
  m_LUTAzimuthTimes = ReadNetCDFVector(sourceFile, "relativeAzimuthTimeRGC");

  GDALAllRegister();
  const std::string datasetName = BuildNetCDFDatasetName(sourceFile, sourceVariable);
  GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(datasetName.c_str(), GA_ReadOnly));
  if (dataset == nullptr)
  {
    otbGenericExceptionMacro(itk::ExceptionObject, << "Cannot open BIOMASS calibration variable " << datasetName);
  }

  const int width = dataset->GetRasterXSize();
  const int height = dataset->GetRasterYSize();
  m_LUTValues.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  const auto err = dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, width, height,
                                                       m_LUTValues.data(), width, height,
                                                       GDT_Float32, 0, 0, nullptr);
  GDALClose(dataset);

  if (err != CE_None)
  {
    otbGenericExceptionMacro(itk::ExceptionObject, << "Cannot read BIOMASS calibration variable " << datasetName);
  }
  if (m_LUTSlantRangeTimes.size() != static_cast<std::size_t>(width) ||
      m_LUTAzimuthTimes.size() != static_cast<std::size_t>(height) ||
      m_LUTValues.size() != m_LUTSlantRangeTimes.size() * m_LUTAzimuthTimes.size())
  {
    otbGenericExceptionMacro(itk::ExceptionObject,
                             << "Invalid BIOMASS calibration NetCDF dimensions in " << sourceFile);
  }
}

double BiomassCalibrationLookupData::InterpolateFactor(double azimuthTime, double slantRangeTime) const
{
  if (m_LUTValues.empty() || m_LUTAzimuthTimes.empty() || m_LUTSlantRangeTimes.empty())
  {
    return 1.0;
  }

  auto locate = [](const std::vector<double>& axis, double value)
  {
    if (axis.size() < 2)
    {
      return std::pair<std::size_t, double>(0, 0.0);
    }
    if (value <= axis.front())
    {
      return std::pair<std::size_t, double>(0, 0.0);
    }
    if (value >= axis.back())
    {
      return std::pair<std::size_t, double>(axis.size() - 2, 1.0);
    }
    const auto upper = std::upper_bound(axis.begin(), axis.end(), value);
    const auto idx = static_cast<std::size_t>(std::distance(axis.begin(), upper) - 1);
    const double denom = axis[idx + 1] - axis[idx];
    const double weight = denom == 0.0 ? 0.0 : (value - axis[idx]) / denom;
    return std::pair<std::size_t, double>(idx, weight);
  };

  const auto az = locate(m_LUTAzimuthTimes, azimuthTime);
  const auto rg = locate(m_LUTSlantRangeTimes, slantRangeTime);
  const std::size_t width = m_LUTSlantRangeTimes.size();

  const auto valueAt = [&](std::size_t azIdx, std::size_t rgIdx)
  {
    return static_cast<double>(m_LUTValues[azIdx * width + rgIdx]);
  };

  const double v00 = valueAt(az.first, rg.first);
  const double v10 = valueAt(az.first, rg.first + 1);
  const double v01 = valueAt(az.first + 1, rg.first);
  const double v11 = valueAt(az.first + 1, rg.first + 1);

  const double v0 = v00 * (1.0 - rg.second) + v10 * rg.second;
  const double v1 = v01 * (1.0 - rg.second) + v11 * rg.second;
  return v0 * (1.0 - az.second) + v1 * az.second;
}

double BiomassCalibrationLookupData::GetValue(const IndexValueType x, const IndexValueType y) const
{
  if (this->GetType() == BETA || this->GetType() == DN || this->GetType() == NOISE)
  {
    return 1.0;
  }

  const double azimuthTime = m_ImageFirstAzimuthTime + static_cast<double>(y) * m_ImageAzimuthTimeSpacing;
  const double slantRangeTime = m_ImageFirstSlantRangeTime + static_cast<double>(x) * m_ImageSlantRangeTimeSpacing;
  const double factor = this->InterpolateFactor(azimuthTime, slantRangeTime);

  if (!std::isfinite(factor) || factor <= 0.0)
  {
    return 1.0;
  }

  return m_ValuesAreBetaToTargetFactors ? 1.0 / std::sqrt(factor) : factor;
}

void BiomassCalibrationLookupData::ToKeywordlist(MetaData::Keywordlist& kwl, const std::string& prefix) const
{
  kwl.insert({prefix + "Sensor", "Biomass"});
  kwl.insert({prefix + "Type", boost::lexical_cast<std::string>(this->GetType())});
  kwl.insert({prefix + "ImageFirstAzimuthTime", ToStringWithPrecision(m_ImageFirstAzimuthTime)});
  kwl.insert({prefix + "ImageAzimuthTimeSpacing", ToStringWithPrecision(m_ImageAzimuthTimeSpacing)});
  kwl.insert({prefix + "ImageFirstSlantRangeTime", ToStringWithPrecision(m_ImageFirstSlantRangeTime)});
  kwl.insert({prefix + "ImageSlantRangeTimeSpacing", ToStringWithPrecision(m_ImageSlantRangeTimeSpacing)});
  kwl.insert({prefix + "ValuesAreBetaToTargetFactors", boost::lexical_cast<std::string>(m_ValuesAreBetaToTargetFactors)});

  if (!m_SourceFile.empty() && !m_SourceVariable.empty())
  {
    kwl.insert({prefix + "SourceFile", m_SourceFile});
    kwl.insert({prefix + "SourceVariable", m_SourceVariable});
  }
  else
  {
    kwl.insert({prefix + "LUTAzimuthTimes", DoubleVectorToString(m_LUTAzimuthTimes)});
    kwl.insert({prefix + "LUTSlantRangeTimes", DoubleVectorToString(m_LUTSlantRangeTimes)});
    kwl.insert({prefix + "LUTValues", FloatVectorToString(m_LUTValues)});
  }
}

void BiomassCalibrationLookupData::FromKeywordlist(const MetaData::Keywordlist& kwl, const std::string& prefix)
{
  this->SetType(boost::lexical_cast<short>(kwl.at(prefix + "Type")));
  m_ImageFirstAzimuthTime = boost::lexical_cast<double>(kwl.at(prefix + "ImageFirstAzimuthTime"));
  m_ImageAzimuthTimeSpacing = boost::lexical_cast<double>(kwl.at(prefix + "ImageAzimuthTimeSpacing"));
  m_ImageFirstSlantRangeTime = boost::lexical_cast<double>(kwl.at(prefix + "ImageFirstSlantRangeTime"));
  m_ImageSlantRangeTimeSpacing = boost::lexical_cast<double>(kwl.at(prefix + "ImageSlantRangeTimeSpacing"));

  const auto valuesAreFactorIt = kwl.find(prefix + "ValuesAreBetaToTargetFactors");
  m_ValuesAreBetaToTargetFactors = valuesAreFactorIt == kwl.end() ? true : StringToBool(valuesAreFactorIt->second);

  const auto sourceFileIt = kwl.find(prefix + "SourceFile");
  const auto sourceVariableIt = kwl.find(prefix + "SourceVariable");
  if (sourceFileIt != kwl.end() && sourceVariableIt != kwl.end())
  {
    m_SourceFile = sourceFileIt->second;
    m_SourceVariable = sourceVariableIt->second;
    this->LoadFromNetCDF(m_SourceFile, m_SourceVariable);
  }
  else
  {
    m_SourceFile.clear();
    m_SourceVariable.clear();
    m_LUTAzimuthTimes = StringToDoubleVector(kwl.at(prefix + "LUTAzimuthTimes"));
    m_LUTSlantRangeTimes = StringToDoubleVector(kwl.at(prefix + "LUTSlantRangeTimes"));
    m_LUTValues = StringToFloatVector(kwl.at(prefix + "LUTValues"));
  }
}

} // end namespace otb
