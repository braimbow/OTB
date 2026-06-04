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

#include "otbBiomassImageMetadataInterface.h"

#include "otbBiomassCalibrationLookupData.h"
#include "otbMacro.h"
#include "otbMissingMetadataException.h"
#include "otbStringUtilities.h"
#include "otb_tinyxml.h"

#include "gdal_priv.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct BiomassProductFiles
{
  std::string productDirectory;
  std::string annotationFile;
  std::string productAnnotationFile;
  std::string lutFile;
  std::string orbitFile;
  std::string mainFile;
};

std::string LocalName(const TiXmlElement* elem)
{
  if (elem == nullptr)
  {
    return "";
  }
  std::string name = elem->ValueStr();
  const auto pos = name.find(':');
  if (pos != std::string::npos)
  {
    name = name.substr(pos + 1);
  }
  return name;
}

const TiXmlElement* Child(const TiXmlElement* parent, const std::string& localName)
{
  if (parent == nullptr)
  {
    return nullptr;
  }
  for (const TiXmlElement* elem = parent->FirstChildElement(); elem != nullptr; elem = elem->NextSiblingElement())
  {
    if (LocalName(elem) == localName)
    {
      return elem;
    }
  }
  return nullptr;
}

std::vector<const TiXmlElement*> Children(const TiXmlElement* parent, const std::string& localName)
{
  std::vector<const TiXmlElement*> output;
  if (parent == nullptr)
  {
    return output;
  }
  for (const TiXmlElement* elem = parent->FirstChildElement(); elem != nullptr; elem = elem->NextSiblingElement())
  {
    if (LocalName(elem) == localName)
    {
      output.push_back(elem);
    }
  }
  return output;
}

const TiXmlElement* FindFirstRecursive(const TiXmlElement* parent, const std::string& localName)
{
  if (parent == nullptr)
  {
    return nullptr;
  }
  if (LocalName(parent) == localName)
  {
    return parent;
  }
  for (const TiXmlElement* elem = parent->FirstChildElement(); elem != nullptr; elem = elem->NextSiblingElement())
  {
    const TiXmlElement* found = FindFirstRecursive(elem, localName);
    if (found != nullptr)
    {
      return found;
    }
  }
  return nullptr;
}

std::string Text(const TiXmlElement* elem)
{
  if (elem == nullptr || elem->GetText() == nullptr)
  {
    return "";
  }
  std::string value = elem->GetText();
  boost::algorithm::trim(value);
  return value;
}

std::string RequiredText(const TiXmlElement* parent, const std::string& localName)
{
  const std::string value = Text(Child(parent, localName));
  if (value.empty())
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Missing BIOMASS metadata field " << localName);
  }
  return value;
}

double RequiredDouble(const TiXmlElement* parent, const std::string& localName)
{
  return std::stod(RequiredText(parent, localName));
}

unsigned long RequiredUnsignedLong(const TiXmlElement* parent, const std::string& localName)
{
  return static_cast<unsigned long>(std::stoul(RequiredText(parent, localName)));
}

std::vector<double> ParseDoubleVector(const std::string& input)
{
  std::vector<double> output;
  const auto parts = otb::split_on(input, ' ');
  for (const auto& elem : parts)
  {
    if (!elem.empty())
    {
      output.push_back(otb::to<double>(elem, "Cannot cast BIOMASS vector value"));
    }
  }
  return output;
}

std::string StripUtcPrefix(std::string value)
{
  boost::algorithm::trim(value);
  constexpr const char* prefix = "UTC=";
  if (value.find(prefix) == 0)
  {
    value = value.substr(std::string(prefix).size());
  }
  return value;
}

otb::MetaData::TimePoint ReadBiomassTime(const std::string& value)
{
  return otb::MetaData::ReadFormattedDate(StripUtcPrefix(value));
}

std::string FindFirstFile(const boost::filesystem::path& directory,
                          const std::string& contains,
                          const std::string& extension)
{
  if (!boost::filesystem::exists(directory) || !boost::filesystem::is_directory(directory))
  {
    return "";
  }

  for (const auto& item : boost::make_iterator_range(boost::filesystem::directory_iterator(directory), {}))
  {
    if (!boost::filesystem::is_regular_file(item.status()))
    {
      continue;
    }
    const auto path = item.path();
    const std::string filename = boost::algorithm::to_lower_copy(path.filename().string());
    if ((contains.empty() || filename.find(contains) != std::string::npos) &&
        (extension.empty() || path.extension().string() == extension))
    {
      return path.string();
    }
  }
  return "";
}

BiomassProductFiles ResolveProductFiles(const std::string& resourceFile)
{
  boost::filesystem::path path(resourceFile);
  if (path.empty())
  {
    otbGenericExceptionMacro(otb::MissingMetadataException, << "Empty BIOMASS resource filename");
  }
  path = boost::filesystem::absolute(path);
  boost::filesystem::path cursor = boost::filesystem::is_directory(path) ? path : path.parent_path();

  for (int depth = 0; depth < 5 && !cursor.empty(); ++depth, cursor = cursor.parent_path())
  {
    if (boost::filesystem::exists(cursor / "annotation") &&
        boost::filesystem::exists(cursor / "measurement"))
    {
      BiomassProductFiles files;
      files.productDirectory = cursor.string();
      files.annotationFile = FindFirstFile(cursor / "annotation", "_annot", ".xml");
      files.productAnnotationFile = files.annotationFile;
      files.lutFile = FindFirstFile(cursor / "annotation", "_lut", ".nc");
      files.orbitFile = FindFirstFile(cursor / "annotation" / "navigation", "_orb", ".xml");
      files.mainFile = FindFirstFile(cursor, "", ".xml");

      if (files.annotationFile.empty() || files.lutFile.empty() || files.orbitFile.empty())
      {
        otbGenericExceptionMacro(otb::MissingMetadataException,
                                 << "Incomplete BIOMASS product structure under " << cursor.string());
      }
      return files;
    }

    if (boost::filesystem::exists(cursor / "annotation_primary") &&
        boost::filesystem::exists(cursor / "annotation_coregistered") &&
        boost::filesystem::exists(cursor / "measurement"))
    {
      BiomassProductFiles files;
      files.productDirectory = cursor.string();
      files.annotationFile = FindFirstFile(cursor / "annotation_primary", "_annot", ".xml");
      files.productAnnotationFile = FindFirstFile(cursor / "annotation_coregistered", "_annot", ".xml");
      files.lutFile = FindFirstFile(cursor / "annotation_coregistered", "_lut", ".nc");
      files.orbitFile = FindFirstFile(cursor / "annotation_primary" / "navigation", "_orb", ".xml");
      files.mainFile = FindFirstFile(cursor, "", ".xml");

      if (files.annotationFile.empty() || files.productAnnotationFile.empty() ||
          files.lutFile.empty() || files.orbitFile.empty())
      {
        otbGenericExceptionMacro(otb::MissingMetadataException,
                                 << "Incomplete BIOMASS L1C/STA product structure under " << cursor.string());
      }
      return files;
    }
  }

  otbGenericExceptionMacro(otb::MissingMetadataException,
                           << "Cannot locate BIOMASS product root from " << resourceFile);
}

void CheckLoadedXML(const TiXmlDocument& doc, const std::string& filename)
{
  if (doc.RootElement() == nullptr)
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Cannot read BIOMASS XML metadata file " << filename);
  }
}

double ReadNetCDFCenterValue(const std::string& sourceFile, const std::string& variable)
{
  GDALAllRegister();
  const std::string datasetName = "NETCDF:\"" + sourceFile + "\":" + variable;
  GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(datasetName.c_str(), GA_ReadOnly));
  if (dataset == nullptr)
  {
    return std::numeric_limits<double>::quiet_NaN();
  }

  const int x = dataset->GetRasterXSize() / 2;
  const int y = dataset->GetRasterYSize() / 2;
  double value = std::numeric_limits<double>::quiet_NaN();
  const auto err = dataset->GetRasterBand(1)->RasterIO(GF_Read, x, y, 1, 1, &value, 1, 1,
                                                       GDT_Float64, 0, 0, nullptr);
  GDALClose(dataset);
  return err == CE_None ? value : std::numeric_limits<double>::quiet_NaN();
}

otb::InfoSceneCoord MakeSceneCoord(const otb::GCP& gcp,
                                   const otb::MetaData::TimePoint& firstAzimuthTime,
                                   double azimuthTimeInterval,
                                   double firstSlantRangeTime,
                                   double rangeTimeInterval,
                                   double incidenceAngle)
{
  otb::InfoSceneCoord coord;
  coord.referenceRow = static_cast<unsigned long>(std::max(0.0, gcp.m_GCPRow));
  coord.referenceColumn = static_cast<unsigned long>(std::max(0.0, gcp.m_GCPCol));
  coord.latitude = gcp.m_GCPY;
  coord.longitude = gcp.m_GCPX;
  coord.azimuthTime = firstAzimuthTime + otb::MetaData::Duration::Seconds(gcp.m_GCPRow * azimuthTimeInterval);
  coord.rangeTime = firstSlantRangeTime + gcp.m_GCPCol * rangeTimeInterval;
  coord.incidenceAngle = incidenceAngle;
  return coord;
}

const otb::GCP& FindNearestGCP(const otb::Projection::GCPParam& gcpParam, double col, double row)
{
  if (gcpParam.GCPs.empty())
  {
    otbGenericExceptionMacro(itk::ExceptionObject, << "BIOMASS product has no GCP metadata");
  }

  return *std::min_element(gcpParam.GCPs.begin(), gcpParam.GCPs.end(),
                           [col, row](const otb::GCP& lhs, const otb::GCP& rhs)
                           {
                             const double lhsDist = std::pow(lhs.m_GCPCol - col, 2.0) + std::pow(lhs.m_GCPRow - row, 2.0);
                             const double rhsDist = std::pow(rhs.m_GCPCol - col, 2.0) + std::pow(rhs.m_GCPRow - row, 2.0);
                             return lhsDist < rhsDist;
                           });
}

std::vector<otb::Orbit> ReadOrbits(const TiXmlElement* orbitRoot)
{
  std::vector<otb::Orbit> orbits;
  const TiXmlElement* dataBlock = Child(orbitRoot, "Data_Block");
  const TiXmlElement* osvList = Child(dataBlock, "List_of_OSVs");
  for (const TiXmlElement* osv : Children(osvList, "OSV"))
  {
    otb::Orbit orbit;
    orbit.time = ReadBiomassTime(RequiredText(osv, "UTC"));
    orbit.position[0] = RequiredDouble(osv, "X");
    orbit.position[1] = RequiredDouble(osv, "Y");
    orbit.position[2] = RequiredDouble(osv, "Z");
    orbit.velocity[0] = RequiredDouble(osv, "VX");
    orbit.velocity[1] = RequiredDouble(osv, "VY");
    orbit.velocity[2] = RequiredDouble(osv, "VZ");
    orbits.push_back(orbit);
  }
  return orbits;
}

std::vector<otb::DopplerCentroid> ReadDopplerCentroids(const TiXmlElement* annotationRoot)
{
  std::vector<otb::DopplerCentroid> centroids;
  const TiXmlElement* doppler = Child(annotationRoot, "dopplerParameters");
  const TiXmlElement* dcList = Child(doppler, "dcEstimateList");
  for (const TiXmlElement* estimate : Children(dcList, "dcEstimate"))
  {
    otb::DopplerCentroid centroid;
    centroid.azimuthTime = ReadBiomassTime(RequiredText(estimate, "azimuthTime"));
    centroid.t0 = RequiredDouble(estimate, "t0");
    centroid.geoDopCoef = ParseDoubleVector(RequiredText(estimate, "geometryDCPolynomial"));
    const auto combined = Text(Child(estimate, "combinedDCPolynomial"));
    centroid.dopCoef = combined.empty() ? centroid.geoDopCoef : ParseDoubleVector(combined);
    centroids.push_back(centroid);
  }
  return centroids;
}

std::vector<otb::AzimuthFmRate> ReadAzimuthFmRates(const TiXmlElement* annotationRoot)
{
  std::vector<otb::AzimuthFmRate> rates;
  const TiXmlElement* doppler = Child(annotationRoot, "dopplerParameters");
  const TiXmlElement* fmList = Child(doppler, "fmRateEstimateList");
  for (const TiXmlElement* estimate : Children(fmList, "fmRateEstimate"))
  {
    otb::AzimuthFmRate rate;
    rate.azimuthTime = ReadBiomassTime(RequiredText(estimate, "azimuthTime"));
    rate.t0 = RequiredDouble(estimate, "t0");
    rate.azimuthFmRatePolynomial = ParseDoubleVector(RequiredText(estimate, "polynomial"));
    rates.push_back(rate);
  }
  return rates;
}

void FillGCPTimes(otb::SARParam& sarParam,
                  const otb::Projection::GCPParam& gcpParam,
                  const otb::MetaData::TimePoint& firstAzimuthTime,
                  double azimuthTimeInterval,
                  double firstSlantRangeTime,
                  double rangeTimeInterval)
{
  for (const auto& gcp : gcpParam.GCPs)
  {
    otb::GCPTime gcpTime;
    gcpTime.azimuthTime = firstAzimuthTime + otb::MetaData::Duration::Seconds(gcp.m_GCPRow * azimuthTimeInterval);
    gcpTime.slantRangeTime = firstSlantRangeTime + gcp.m_GCPCol * rangeTimeInterval;
    sarParam.gcpTimes[gcp.m_Id] = gcpTime;
  }
}

void FillSarCalibration(otb::SARCalib& sarCalib,
                        const BiomassProductFiles& files,
                        const otb::MetaData::TimePoint& startTime,
                        const otb::MetaData::TimePoint& stopTime,
                        double firstSlantRangeTime,
                        double rangeTimeInterval,
                        double azimuthTimeInterval)
{
  sarCalib.calibrationLookupFlag = true;
  sarCalib.rescalingFactor = 1.0;
  sarCalib.calibrationStartTime = startTime;
  sarCalib.calibrationStopTime = stopTime;

  auto setArray = [](otb::SARCalib::ArrayType& array)
  {
    array[0] = 0;
    array[1] = 0;
  };
  setArray(sarCalib.radiometricCalibrationNoisePolynomialDegree);
  setArray(sarCalib.radiometricCalibrationAntennaPatternNewGainPolynomialDegree);
  setArray(sarCalib.radiometricCalibrationAntennaPatternOldGainPolynomialDegree);
  setArray(sarCalib.radiometricCalibrationIncidenceAnglePolynomialDegree);
  setArray(sarCalib.radiometricCalibrationRangeSpreadLossPolynomialDegree);

  auto beta = otb::SarCalibrationLookupData::New();
  beta->SetType(otb::SarCalibrationLookupData::BETA);
  sarCalib.calibrationLookupData[otb::SarCalibrationLookupData::BETA] = beta;

  auto dn = otb::SarCalibrationLookupData::New();
  dn->SetType(otb::SarCalibrationLookupData::DN);
  sarCalib.calibrationLookupData[otb::SarCalibrationLookupData::DN] = dn;

  auto sigma = otb::BiomassCalibrationLookupData::New();
  sigma->InitializeFromNetCDF(otb::SarCalibrationLookupData::SIGMA,
                              0.0, azimuthTimeInterval,
                              firstSlantRangeTime, rangeTimeInterval,
                              files.lutFile, "/radiometry/sigmaNought");
  sarCalib.calibrationLookupData[otb::SarCalibrationLookupData::SIGMA] = sigma;

  auto gamma = otb::BiomassCalibrationLookupData::New();
  gamma->InitializeFromNetCDF(otb::SarCalibrationLookupData::GAMMA,
                              0.0, azimuthTimeInterval,
                              firstSlantRangeTime, rangeTimeInterval,
                              files.lutFile, "/radiometry/gammaNought");
  sarCalib.calibrationLookupData[otb::SarCalibrationLookupData::GAMMA] = gamma;
}

} // end anonymous namespace

namespace otb
{

double BiomassImageMetadataInterface::GetCenterIncidenceAngle(const MetadataSupplierInterface& mds) const
{
  const auto files = ResolveProductFiles(mds.GetResourceFile());
  return ReadNetCDFCenterValue(files.lutFile, "/geometry/incidenceAngle");
}

void BiomassImageMetadataInterface::Parse(ImageMetadata& imd)
{
  this->ParseGdal(imd);
}

void BiomassImageMetadataInterface::ParseGeom(ImageMetadata& imd)
{
  this->ParseGdal(imd);
}

void BiomassImageMetadataInterface::ParseGdal(ImageMetadata& imd)
{
  if (m_MetadataSupplierInterface == nullptr)
  {
    otbGenericExceptionMacro(MissingMetadataException, << "BIOMASS parser has no metadata supplier");
  }

  const auto files = ResolveProductFiles(m_MetadataSupplierInterface->GetResourceFile());
  TiXmlDocument annotationDoc(files.annotationFile.c_str());
  TiXmlDocument productAnnotationDoc(files.productAnnotationFile.c_str());
  TiXmlDocument orbitDoc(files.orbitFile.c_str());
  if (!annotationDoc.LoadFile())
  {
    otbGenericExceptionMacro(MissingMetadataException, << "Cannot read BIOMASS XML metadata file " << files.annotationFile);
  }
  if (!files.productAnnotationFile.empty() && files.productAnnotationFile != files.annotationFile &&
      !productAnnotationDoc.LoadFile())
  {
    otbGenericExceptionMacro(MissingMetadataException, << "Cannot read BIOMASS XML metadata file " << files.productAnnotationFile);
  }
  if (!orbitDoc.LoadFile())
  {
    otbGenericExceptionMacro(MissingMetadataException, << "Cannot read BIOMASS XML metadata file " << files.orbitFile);
  }
  CheckLoadedXML(annotationDoc, files.annotationFile);
  if (!files.productAnnotationFile.empty() && files.productAnnotationFile != files.annotationFile)
  {
    CheckLoadedXML(productAnnotationDoc, files.productAnnotationFile);
  }
  CheckLoadedXML(orbitDoc, files.orbitFile);

  TiXmlDocument mainDoc(files.mainFile.c_str());
  if (!files.mainFile.empty())
  {
    if (!mainDoc.LoadFile())
    {
      otbGenericExceptionMacro(MissingMetadataException, << "Cannot read BIOMASS XML metadata file " << files.mainFile);
    }
    CheckLoadedXML(mainDoc, files.mainFile);
  }

  const TiXmlElement* annotationRoot = annotationDoc.RootElement();
  const TiXmlElement* productAnnotationRoot =
      (files.productAnnotationFile.empty() || files.productAnnotationFile == files.annotationFile) ?
      annotationRoot : productAnnotationDoc.RootElement();
  const TiXmlElement* orbitRoot = orbitDoc.RootElement();
  const TiXmlElement* acquisitionInformation = Child(annotationRoot, "acquisitionInformation");
  const TiXmlElement* productAcquisitionInformation = Child(productAnnotationRoot, "acquisitionInformation");
  const TiXmlElement* sarImage = Child(annotationRoot, "sarImage");
  const TiXmlElement* instrumentParameters = Child(annotationRoot, "instrumentParameters");
  const TiXmlElement* processingParameters = Child(annotationRoot, "processingParameters");
  const TiXmlElement* rangeProcessing = Child(processingParameters, "rangeProcessingParameters");
  const TiXmlElement* azimuthProcessing = Child(processingParameters, "azimuthProcessingParameters");

  const std::string mission = RequiredText(acquisitionInformation, "mission");
  if (mission != "BIOMASS")
  {
    otbGenericExceptionMacro(MissingMetadataException,
                             << "Not a BIOMASS product: mission is " << mission);
  }
  const std::string projection = RequiredText(sarImage, "projection");
  const std::string groundProjectionFlag = RequiredText(processingParameters, "groundProjectionFlag");
  if (projection != "Slant Range" || groundProjectionFlag != "false")
  {
    otbGenericExceptionMacro(MissingMetadataException,
                             << "Only BIOMASS non-ground-projected SCS/STA products are supported");
  }

  const double firstSlantRangeTime = RequiredDouble(sarImage, "firstSampleSlantRangeTime");
  const double lastSlantRangeTime = RequiredDouble(sarImage, "lastSampleSlantRangeTime");
  const double rangeTimeInterval = RequiredDouble(sarImage, "rangeTimeInterval");
  const double azimuthTimeInterval = RequiredDouble(sarImage, "azimuthTimeInterval");
  const double rangePixelSpacing = RequiredDouble(sarImage, "rangePixelSpacing");
  const double azimuthPixelSpacing = RequiredDouble(sarImage, "azimuthPixelSpacing");
  const unsigned long numberOfSamples = RequiredUnsignedLong(sarImage, "numberOfSamples");
  const unsigned long numberOfLines = RequiredUnsignedLong(sarImage, "numberOfLines");
  const auto firstAzimuthTime = ReadBiomassTime(RequiredText(sarImage, "firstLineAzimuthTime"));
  const auto lastAzimuthTime = ReadBiomassTime(RequiredText(sarImage, "lastLineAzimuthTime"));
  const double centerIncidenceAngle = ReadNetCDFCenterValue(files.lutFile, "/geometry/incidenceAngle");

  imd.Add(MDStr::SensorID, "BIOMASS");
  imd.Add(MDStr::Mission, "BIOMASS");
  imd.Add(MDStr::Instrument, "P-SAR");
  const std::string productType = RequiredText(productAcquisitionInformation, "productType");
  imd.Add(MDStr::ProductType, productType);
  imd.Add(MDStr::Swath, RequiredText(acquisitionInformation, "swath"));
  imd.Add(MDStr::OrbitDirection, RequiredText(acquisitionInformation, "orbitPass"));
  imd.Add(MDStr::Polarization, "HH HV VH VV");
  imd.Add(MDStr::Mode, productType);
  imd.Add(MDNum::NumberOfColumns, static_cast<double>(numberOfSamples));
  imd.Add(MDNum::NumberOfLines, static_cast<double>(numberOfLines));
  imd.Add(MDNum::LineSpacing, azimuthPixelSpacing);
  imd.Add(MDNum::PixelSpacing, rangePixelSpacing);
  imd.Add(MDNum::RangeTimeFirstPixel, firstSlantRangeTime);
  imd.Add(MDNum::RangeTimeLastPixel, lastSlantRangeTime);
  imd.Add(MDNum::RSF, 1.0 / rangeTimeInterval);
  imd.Add(MDNum::PRF, RequiredDouble(Child(instrumentParameters, "prfList")->FirstChildElement(), "value"));
  imd.Add(MDNum::RadarFrequency, RequiredDouble(instrumentParameters, "radarCarrierFrequency"));
  imd.Add(MDNum::CenterIncidenceAngle, centerIncidenceAngle);
  imd.Add(MDNum::CalScale, 1.0);
  imd.Add(MDNum::CalFactor, 1.0);
  imd.Add(MDNum::NoData, RequiredDouble(sarImage, "noDataValue"));
  imd.Add(MDTime::AcquisitionStartTime, firstAzimuthTime);
  imd.Add(MDTime::AcquisitionStopTime, lastAzimuthTime);

  SARCalib sarCalib;
  FillSarCalibration(sarCalib, files, firstAzimuthTime, lastAzimuthTime,
                     firstSlantRangeTime, rangeTimeInterval, azimuthTimeInterval);
  imd.Add(MDGeom::SARCalib, sarCalib);

  SARParam sarParam;
  sarParam.azimuthTimeInterval = MetaData::Duration::Seconds(azimuthTimeInterval);
  sarParam.nearRangeTime = firstSlantRangeTime;
  sarParam.rangeSamplingRate = 1.0 / rangeTimeInterval;
  sarParam.rangeResolution = rangePixelSpacing;
  sarParam.numberOfLinesPerBurst = numberOfLines;
  sarParam.numberOfSamplesPerBurst = numberOfSamples;
  sarParam.rangeBandwidth = RequiredDouble(rangeProcessing, "processingBandwidth");
  sarParam.azimuthBandwidth = RequiredDouble(azimuthProcessing, "processingBandwidth");
  sarParam.azimuthSteeringRate = 0.0;

  std::string lookDirection = "LEFT";
  if (mainDoc.RootElement() != nullptr)
  {
    const TiXmlElement* lookDirectionElem = FindFirstRecursive(mainDoc.RootElement(), "antennaLookDirection");
    const auto value = Text(lookDirectionElem);
    if (!value.empty())
    {
      lookDirection = value;
    }
  }
  sarParam.rightLookingFlag = lookDirection == "RIGHT";

  BurstRecord burst;
  burst.azimuthStartTime = firstAzimuthTime;
  burst.azimuthStopTime = lastAzimuthTime;
  burst.startLine = 0;
  burst.endLine = numberOfLines - 1;
  burst.startSample = 0;
  burst.endSample = numberOfSamples - 1;
  burst.azimuthAnxTime = 0.0;
  sarParam.burstRecords.push_back(burst);

  sarParam.orbits = ReadOrbits(orbitRoot);
  sarParam.dopplerCentroids = ReadDopplerCentroids(annotationRoot);
  sarParam.azimuthFmRates = ReadAzimuthFmRates(annotationRoot);

  const auto& gcpParam = imd.GetGCPParam();
  FillGCPTimes(sarParam, gcpParam, firstAzimuthTime, azimuthTimeInterval,
               firstSlantRangeTime, rangeTimeInterval);

  const auto& ulGcp = FindNearestGCP(gcpParam, 0.0, 0.0);
  const auto& urGcp = FindNearestGCP(gcpParam, static_cast<double>(numberOfSamples - 1), 0.0);
  const auto& llGcp = FindNearestGCP(gcpParam, 0.0, static_cast<double>(numberOfLines - 1));
  const auto& lrGcp = FindNearestGCP(gcpParam, static_cast<double>(numberOfSamples - 1), static_cast<double>(numberOfLines - 1));
  const auto& centerGcp = FindNearestGCP(gcpParam, static_cast<double>(numberOfSamples - 1) / 2.0,
                                         static_cast<double>(numberOfLines - 1) / 2.0);

  sarParam.ulSceneCoord = MakeSceneCoord(ulGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.urSceneCoord = MakeSceneCoord(urGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.llSceneCoord = MakeSceneCoord(llGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.lrSceneCoord = MakeSceneCoord(lrGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.centerSceneCoord = MakeSceneCoord(centerGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);

  imd.Add(MDGeom::SAR, sarParam);
}

} // end namespace otb
