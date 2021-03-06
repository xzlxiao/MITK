/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#ifndef __itkFiberExtractionFilter_cpp
#define __itkFiberExtractionFilter_cpp

#include "itkFiberExtractionFilter.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <boost/progress.hpp>
#include <mitkDiffusionFunctionCollection.h>

namespace itk{


template< class PixelType >
FiberExtractionFilter< PixelType >::FiberExtractionFilter()
  : m_DontResampleFibers(false)
  , m_Mode(MODE::OVERLAP)
  , m_InputType(INPUT::SCALAR_MAP)
  , m_BothEnds(true)
  , m_OverlapFraction(0.8)
  , m_NoNegatives(false)
  , m_NoPositives(false)
  , m_Interpolate(false)
  , m_Threshold(0.5)
  , m_Labels({1})
{
  m_Interpolator = itk::LinearInterpolateImageFunction< itk::Image< PixelType, 3 >, float >::New();
}

template< class PixelType >
FiberExtractionFilter< PixelType >::~FiberExtractionFilter()
{

}

template< class PixelType >
void FiberExtractionFilter< PixelType >::SetRoiImages(const std::vector<ItkInputImgType*> &rois)
{
  for (auto roi : rois)
  {
    if (roi==nullptr)
    {
      MITK_INFO << "ROI image is NULL!";
      return;
    }
  }
  m_RoiImages = rois;
}

template< class PixelType >
mitk::FiberBundle::Pointer FiberExtractionFilter< PixelType >::CreateFib(std::vector< long >& ids)
{
  vtkSmartPointer<vtkFloatArray> weights = vtkSmartPointer<vtkFloatArray>::New();
  vtkSmartPointer<vtkPolyData> pTmp = m_InputFiberBundle->GeneratePolyDataByIds(ids, weights);
  mitk::FiberBundle::Pointer fib = mitk::FiberBundle::New(pTmp);
  fib->SetFiberWeights(weights);
  return fib;
}

template< class PixelType >
bool FiberExtractionFilter< PixelType >::IsPositive(const itk::Point<float, 3>& itkP, itk::Image<PixelType, 3>* image)
{
  m_Interpolator->SetInputImage(image);
  if( m_InputType == INPUT::SCALAR_MAP )
    return mitk::imv::IsInsideMask<PixelType>(itkP, m_Interpolate, m_Interpolator, m_Threshold);
  else if( m_InputType == INPUT::LABEL_MAP )
  {
    auto val = mitk::imv::GetImageValue<PixelType>(itkP, m_Interpolate, m_Interpolator);
    for (auto l : m_Labels)
      if (l==val)
        return true;
  }
  else
    mitkThrow() << "No valid input type selected!";
  return false;
}

template< class PixelType >
void FiberExtractionFilter< PixelType >::ExtractOverlap(mitk::FiberBundle::Pointer fib)
{
  MITK_INFO << "Extracting fibers (min. overlap " << m_OverlapFraction << ")";
  vtkSmartPointer<vtkPolyData> polydata = fib->GetFiberPolyData();

  std::vector< std::vector< long > > positive_ids;
  positive_ids.resize(m_RoiImages.size());

  std::vector< long > negative_ids; // fibers not overlapping with ANY mask

  boost::progress_display disp(m_InputFiberBundle->GetNumFibers());
  for (int i=0; i<m_InputFiberBundle->GetNumFibers(); i++)
  {
    ++disp;
    vtkCell* cell = polydata->GetCell(i);
    int numPoints = cell->GetNumberOfPoints();
    vtkPoints* points = cell->GetPoints();

    bool positive = false;
    for (unsigned int m=0; m<m_RoiImages.size(); ++m)
    {
      auto roi = m_RoiImages.at(m);
      int inside = 0;
      int outside = 0;
      for (int j=0; j<numPoints; j++)
      {
        double* p = points->GetPoint(j);

        itk::Point<float, 3> itkP;
        itkP[0] = p[0]; itkP[1] = p[1]; itkP[2] = p[2];

        if ( IsPositive(itkP, roi) )
          inside++;
        else
          outside++;

        if ((float)inside/numPoints > m_OverlapFraction)
        {
          positive = true;
          positive_ids[m].push_back(i);
          break;
        }
      }
    }
    if (!positive)
      negative_ids.push_back(i);
  }

  if (!m_NoNegatives)
    m_Negatives.push_back(CreateFib(negative_ids));
  if (!m_NoPositives)
    for (auto ids : positive_ids)
      m_Positives.push_back(CreateFib(ids));
}

template< class PixelType >
void FiberExtractionFilter< PixelType >::ExtractEndpoints(mitk::FiberBundle::Pointer fib)
{
  MITK_INFO << "Extracting fibers (endpoints in mask)";
  vtkSmartPointer<vtkPolyData> polydata = fib->GetFiberPolyData();

  std::vector< std::vector< long > > positive_ids;
  positive_ids.resize(m_RoiImages.size());

  std::vector< long > negative_ids; // fibers not overlapping with ANY mask

  boost::progress_display disp(m_InputFiberBundle->GetNumFibers());
  for (int i=0; i<m_InputFiberBundle->GetNumFibers(); i++)
  {
    ++disp;
    vtkCell* cell = polydata->GetCell(i);
    int numPoints = cell->GetNumberOfPoints();
    vtkPoints* points = cell->GetPoints();

    bool positive = false;
    if (numPoints>1)
      for (unsigned int m=0; m<m_RoiImages.size(); ++m)
      {
        auto roi = m_RoiImages.at(m);

        int inside = 0;

        // check first fiber point
        {
          double* p = points->GetPoint(0);
          itk::Point<float, 3> itkP;
          itkP[0] = p[0]; itkP[1] = p[1]; itkP[2] = p[2];

          if ( IsPositive(itkP, roi) )
            inside++;
        }

        // check second fiber point
        {
          double* p = points->GetPoint(numPoints-1);
          itk::Point<float, 3> itkP;
          itkP[0] = p[0]; itkP[1] = p[1]; itkP[2] = p[2];
          itk::Index<3> idx;
          roi->TransformPhysicalPointToIndex(itkP, idx);
          if ( IsPositive(itkP, roi) )
            inside++;
        }

        if (inside==2 || (inside==1 && !m_BothEnds))
        {
          positive = true;
          positive_ids[m].push_back(i);
        }
      }
    if (!positive)
      negative_ids.push_back(i);
  }

  if (!m_NoNegatives)
    m_Negatives.push_back(CreateFib(negative_ids));
  if (!m_NoPositives)
    for (auto ids : positive_ids)
      m_Positives.push_back(CreateFib(ids));
}

template< class PixelType >
void FiberExtractionFilter< PixelType >::SetLabels(const std::vector<unsigned short> &Labels)
{
  m_Labels = Labels;
}

template< class PixelType >
std::vector<mitk::FiberBundle::Pointer> FiberExtractionFilter< PixelType >::GetNegatives() const
{
  return m_Negatives;
}

template< class PixelType >
std::vector<mitk::FiberBundle::Pointer> FiberExtractionFilter< PixelType >::GetPositives() const
{
  return m_Positives;
}

template< class PixelType >
void FiberExtractionFilter< PixelType >::GenerateData()
{
  mitk::FiberBundle::Pointer fib = m_InputFiberBundle;
  if (fib->GetNumFibers()<=0)
  {
    MITK_INFO << "No fibers in tractogram!";
    return;
  }

  if (m_Mode==MODE::OVERLAP && !m_DontResampleFibers)
  {
    float minSpacing = 1;
    for (auto mask : m_RoiImages)
    {
      for (int i=0; i<3; ++i)
        if(mask->GetSpacing()[i]<minSpacing)
          minSpacing = mask->GetSpacing()[i];
    }

    fib = m_InputFiberBundle->GetDeepCopy();
    fib->ResampleLinear(minSpacing/5);
  }

  if (m_Mode == MODE::OVERLAP)
    ExtractOverlap(fib);
  else if (m_Mode == MODE::ENDPOINTS)
    ExtractEndpoints(fib);
}

}

#endif // __itkFiberExtractionFilter_cpp



