//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

// Stocastic Tools Includes
#include "SamplerData.h"

// MOOSE includes
#include "Sampler.h"

registerMooseObject("StochasticToolsApp", SamplerData);

defineLegacyParams(SamplerData);

InputParameters
SamplerData::validParams()
{
  InputParameters params = GeneralVectorPostprocessor::validParams();
  params.addClassDescription(
      "Tool for extracting Sampler object data and storing in VectorPostprocessor vectors.");
  params += SamplerInterface::validParams();
  params.addRequiredParam<SamplerName>("sampler",
                                       "The sample from which to extract distribution data.");

  // The execute method computes the complete vectors on all processes, so broadcasting the data
  // is not required.
  // params.set<bool>("_is_broadcast") = false;

  // Control for
  MooseEnum method("get_global_samples get_local_samples get_next_local_row", "get_next_local_row");
  params.addParam<MooseEnum>(
      "sampler_method",
      method,
      "Control the method of data retrival from the Sampler object; this is mainly for testing.");

  return params;
}

SamplerData::SamplerData(const InputParameters & parameters)
  : GeneralVectorPostprocessor(parameters),
    SamplerInterface(this),
    _sampler(getSampler("sampler")),
    _sampler_method(getParam<MooseEnum>("sampler_method"))
{
  for (dof_id_type j = 0; j < _sampler.getNumberOfCols(); ++j)
    _sample_vectors.push_back(
        &declareVector(getParam<SamplerName>("sampler") + "_" + std::to_string(j)));
}

void
SamplerData::initialize()
{
  dof_id_type n = (_sampler_method == "get_global_samples") ? _sampler.getNumberOfRows()
                                                            : _sampler.getNumberOfLocalRows();
  for (auto & ppv_ptr : _sample_vectors)
    ppv_ptr->resize(n, 0);
}

void
SamplerData::execute()
{
  if (_sampler_method == "get_global_samples")
  {
    DenseMatrix<Real> data = _sampler.getGlobalSamples();
    for (unsigned int j = 0; j < data.n(); ++j)
      for (unsigned int i = 0; i < data.m(); ++i)
        (*_sample_vectors[j])[i] = data(i, j);
  }

  else if (_sampler_method == "get_local_samples")
  {
    DenseMatrix<Real> data = _sampler.getLocalSamples();
    for (unsigned int j = 0; j < data.n(); ++j)
      for (unsigned int i = 0; i < data.m(); ++i)
        (*_sample_vectors[j])[i] = data(i, j);
  }

  else if (_sampler_method == "get_next_local_row")
  {
    for (dof_id_type i = _sampler.getLocalRowBegin(); i < _sampler.getLocalRowEnd(); ++i)
    {
      std::vector<Real> data = _sampler.getNextLocalRow();
      for (std::size_t j = 0; j < data.size(); ++j)
        (*_sample_vectors[j])[i - _sampler.getLocalRowBegin()] = data[j];
    }
  }
}

void
SamplerData::finalize()
{
  if (_sampler_method != "get_global_samples")
    for (auto & ppv_ptr : _sample_vectors)
      _communicator.gather(0, *ppv_ptr);
}

void
SamplerData::threadJoin(const UserObject & /*uo*/)
{
  /// TODO: Use this when the Sampler objects become threaded
  /*
  if (_use_local_samples)
  {
    const SamplerData & obj = static_cast<const SamplerData &>(uo);
    for (std::size_t i = 0; i < _sample_vectors.size(); ++i)
      (*_sample_vectors[i]).insert(_sample_vectors[i]->end(), obj._sample_vectors[i]->begin(),
                                   obj._sample_vectors[i]->end());
  }
  */
}
