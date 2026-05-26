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

#ifndef otbBiomassImageMetadataInterfaceFactory_h
#define otbBiomassImageMetadataInterfaceFactory_h

#include "OTBMetadataExport.h"
#include "itkObjectFactoryBase.h"

namespace otb
{

class OTBMetadata_EXPORT BiomassImageMetadataInterfaceFactory : public itk::ObjectFactoryBase
{
public:
  typedef BiomassImageMetadataInterfaceFactory Self;
  typedef itk::ObjectFactoryBase               Superclass;
  typedef itk::SmartPointer<Self>              Pointer;
  typedef itk::SmartPointer<const Self>        ConstPointer;

  const char* GetITKSourceVersion(void) const override;
  const char* GetDescription(void) const override;

  itkFactorylessNewMacro(Self);
  itkTypeMacro(BiomassImageMetadataInterfaceFactory, itk::ObjectFactoryBase);

  static void RegisterOneFactory(void)
  {
    BiomassImageMetadataInterfaceFactory::Pointer factory = BiomassImageMetadataInterfaceFactory::New();
    itk::ObjectFactoryBase::RegisterFactory(factory);
  }

protected:
  BiomassImageMetadataInterfaceFactory();
  ~BiomassImageMetadataInterfaceFactory() override;

private:
  BiomassImageMetadataInterfaceFactory(const Self&) = delete;
  void operator=(const Self&) = delete;
};

} // end namespace otb

#endif
