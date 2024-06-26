// Definitions of class used to decode and
// read files written by fnirt, and potentially
// by other pieces of software as long as they
// are valid displacement-field files.
//
// fnirt_file_reader.cpp
//
// Jesper Andersson, FMRIB Image Analysis Group
//
// Copyright (C) 2007-2012 University of Oxford
//
/*  CCOPYRIGHT  */
#include <string>
#include <vector>
#include <memory>
#include "armawrap/newmat.h"

#ifndef EXPOSE_TREACHEROUS
#define EXPOSE_TREACHEROUS           // To allow us to use .sampling_mat()
#endif

#include "newimage/newimageall.h"
#include "basisfield/basisfield.h"
#include "basisfield/splinefield.h"
#include "basisfield/dctfield.h"
#include "warpfns.h"
#include "fnirt_file_reader.h"

using namespace std;
using namespace NEWMAT;
using namespace BASISFIELD;

namespace NEWIMAGE {

/////////////////////////////////////////////////////////////////////
//
// Copy constructor
//
/////////////////////////////////////////////////////////////////////

FnirtFileReader::FnirtFileReader(const FnirtFileReader& src)
: _fname(src._fname), _type(src._type), _aff(src._aff), _coef_rep(3)
{
  for (unsigned int i=0; i<src._coef_rep.size(); i++) {
    if (_type == FnirtSplineDispType) {
      if (src._coef_rep[i]) {
        const splinefield& tmpref = dynamic_cast<const splinefield&>(*(src._coef_rep[i]));
        _coef_rep[i] = std::shared_ptr<basisfield>(new splinefield(tmpref));
      }
    }
    if (_type == FnirtDCTDispType) {
      if (src._coef_rep[i]) {
        const dctfield& tmpref = dynamic_cast<const dctfield&>(*(src._coef_rep[i]));
        _coef_rep[i] = std::shared_ptr<basisfield>(new dctfield(tmpref));
      }
    }
  }
  if (src._vol_rep) _vol_rep = std::shared_ptr<volume4D<float> >(new volume4D<float>(*(src._vol_rep)));
}

/////////////////////////////////////////////////////////////////////
//
// Return matrix-size of field
//
/////////////////////////////////////////////////////////////////////

vector<unsigned int> FnirtFileReader::FieldSize() const
{
  vector<unsigned int>  ret(3,0);

  switch (_type) {
  case FnirtFieldDispType: case UnknownDispType:
    ret[0] = static_cast<unsigned int>(_vol_rep->xsize());
    ret[1] = static_cast<unsigned int>(_vol_rep->ysize());
    ret[2] = static_cast<unsigned int>(_vol_rep->zsize());
    break;
 case FnirtSplineDispType: case FnirtDCTDispType:
   ret[0] = _coef_rep[0]->FieldSz_x(); ret[1] = _coef_rep[0]->FieldSz_y(); ret[2] = _coef_rep[0]->FieldSz_z();
   break;
  default:
    throw FnirtFileReaderException("FieldSize: Invalid _type");
  }
  return(ret);
}

/////////////////////////////////////////////////////////////////////
//
// Return voxel-size of field
//
/////////////////////////////////////////////////////////////////////

vector<double> FnirtFileReader::VoxelSize() const
{
  vector<double>  ret(3,0);

  switch (_type) {
  case FnirtFieldDispType: case UnknownDispType:
    ret[0] = _vol_rep->xdim(); ret[1] = _vol_rep->ydim(); ret[2] = _vol_rep->zdim();
    break;
 case FnirtSplineDispType: case FnirtDCTDispType:
   ret[0] = _coef_rep[0]->Vxs_x(); ret[1] = _coef_rep[0]->Vxs_y(); ret[2] = _coef_rep[0]->Vxs_z();
   break;
  default:
    throw FnirtFileReaderException("VoxelSize: Invalid _type");
  }
  return(ret);
}

/////////////////////////////////////////////////////////////////////
//
// Return knot-spacing provided field is splinefield
//
/////////////////////////////////////////////////////////////////////

vector<unsigned int> FnirtFileReader::KnotSpacing() const
{
  if (_type == FnirtSplineDispType) {
    vector<unsigned int>  ret(3,0);
    const splinefield&    tmp = dynamic_cast<const splinefield&>(*(_coef_rep[0]));
    ret[0] = tmp.Ksp_x(); ret[1] = tmp.Ksp_y(); ret[2] = tmp.Ksp_z();
    return(ret);
  }
  else {
    throw FnirtFileReaderException("KnotSpacing: Field not a splinefield");
  }
}

/////////////////////////////////////////////////////////////////////
//
// Return spline order provided field is splinefield
//
/////////////////////////////////////////////////////////////////////

unsigned int FnirtFileReader::SplineOrder() const
{
  if (_type == FnirtSplineDispType) {
    const splinefield&    tmp = dynamic_cast<const splinefield&>(*(_coef_rep[0]));
    return(tmp.Order());
  }
  else {
    throw FnirtFileReaderException("KnotSpacing: Field not a splinefield");
  }
}

/////////////////////////////////////////////////////////////////////
//
// Return DCT-order provided field is dctfield
//
/////////////////////////////////////////////////////////////////////

vector<unsigned int> FnirtFileReader::DCTOrder() const
{
  vector<unsigned int>  ret(3,0);

  switch (_type) {
  case FnirtFieldDispType: case UnknownDispType: case FnirtSplineDispType:
    throw FnirtFileReaderException("DCTOrder: Field not a dctfield");
    break;
  case FnirtDCTDispType:
   ret[0] = _coef_rep[0]->CoefSz_x(); ret[1] = _coef_rep[0]->CoefSz_y(); ret[2] = _coef_rep[0]->CoefSz_z();
   break;
  default:
    throw FnirtFileReaderException("DCTOrder: Invalid _type");
  }
  return(ret);
}

/////////////////////////////////////////////////////////////////////
//
// Return field as a NEWMAT matrix/vector. Optionally with the affine
// part of the transform included in the field.
//
/////////////////////////////////////////////////////////////////////

ReturnMatrix FnirtFileReader::FieldAsNewmatMatrix(int indx, bool inc_aff) const
{
  if (indx > 2) throw FnirtFileReaderException("FieldAsNewmatMatrix: indx out of range");

  if (indx == -1) {  // Means we want full 4D shabang
    volume4D<float>  volfield = FieldAsNewimageVolume4D(inc_aff);
    Matrix omat(volfield.nvoxels(),3);
    for (unsigned int i=0; i<3; i++) omat.Column(i+1) = volfield[i].vec();
    omat.Release();
    return(omat);
  }
  else {
    volume<float> volfield = FieldAsNewimageVolume(indx,inc_aff);
    ColumnVector omat = volfield.vec();
    omat.Release();
    return(omat);
  }
}

/////////////////////////////////////////////////////////////////////
//
// Return one of the three "fields" as NEWIMAGE volume.
// Optionally with the affine part of the transform included in the field.
//
/////////////////////////////////////////////////////////////////////

volume<float> FnirtFileReader::FieldAsNewimageVolume(unsigned int indx, bool inc_aff) const
{
  if (indx > 2) throw FnirtFileReaderException("FieldAsNewimageVolume: indx out of range");
  volume<float> vol(FieldSize()[0],FieldSize()[1],FieldSize()[2]);
  switch (_type) {
  case FnirtFieldDispType: case UnknownDispType:
    vol=(*_vol_rep)[indx];
    if (inc_aff) add_affine_part(_aff,indx,vol);
    return(vol);
    break;
  case FnirtSplineDispType: case FnirtDCTDispType:
    vol.setdims(VoxelSize()[0],VoxelSize()[1],VoxelSize()[2]);
    _coef_rep[indx]->AsVolume(vol);
    if (inc_aff) add_affine_part(_aff,indx,vol);
    return(vol);
    break;
  default:
    throw FnirtFileReaderException("FieldAsNewimageVolume: Invalid _type");
  }
}

/////////////////////////////////////////////////////////////////////
//
// Return field as 4D volume. Optionally with the affine
// part of the transform included in the field.
//
/////////////////////////////////////////////////////////////////////

volume4D<float> FnirtFileReader::FieldAsNewimageVolume4D(bool inc_aff) const
{
  volume4D<float> vol(FieldSize()[0],FieldSize()[1],FieldSize()[2],3);
  switch (_type) {
  case FnirtFieldDispType: case UnknownDispType:
    vol = *_vol_rep;
    for (unsigned int i=0; i<3; i++) {
      ShadowVolume<float> voli(vol[i]);
      if (inc_aff) add_affine_part(_aff,i,voli);
    }
    return(vol);
    break;
  case FnirtSplineDispType: case FnirtDCTDispType:
    vol.setdims(VoxelSize()[0],VoxelSize()[1],VoxelSize()[2],1.0);
    for (unsigned int i=0; i<3; i++) {
      ShadowVolume<float> voli(vol[i]);
      _coef_rep[i]->AsVolume(voli);
      if (inc_aff) add_affine_part(_aff,i,voli);
    }
    return(vol);
    break;
  default:
    throw FnirtFileReaderException("FieldAsNewimageVolume4D: Invalid _type");
  }
}

/////////////////////////////////////////////////////////////////////
//
// Return the Jacobian determinant of the field as a NEWIMAGE volume.
//
/////////////////////////////////////////////////////////////////////

volume<float> FnirtFileReader::Jacobian(bool inc_aff) const
{
  if (_type==FnirtFieldDispType || _type==UnknownDispType) {
    if (inc_aff) throw FnirtFileReaderException("Jacobian: No affine to include for non-basis representations");
    NEWIMAGE::volume4D<float> xwarpgrad(*_vol_rep); fin_diff_gradient_on_voxel_centres((*_vol_rep)[0],xwarpgrad);
    NEWIMAGE::volume4D<float> ywarpgrad(*_vol_rep); fin_diff_gradient_on_voxel_centres((*_vol_rep)[1],ywarpgrad);
    NEWIMAGE::volume4D<float> zwarpgrad(*_vol_rep); fin_diff_gradient_on_voxel_centres((*_vol_rep)[2],zwarpgrad);
    volume<float>  jac((*_vol_rep)[0].xsize(),(*_vol_rep)[0].ysize(),(*_vol_rep)[0].zsize());
    jac.setdims((*_vol_rep)[0].xdim(),(*_vol_rep)[0].ydim(),(*_vol_rep)[0].zdim());
    fin_diff_gradient2jacobian(xwarpgrad,ywarpgrad,zwarpgrad,jac);
    /*
    NEWIMAGE::volume4D<float> out((*_vol_rep)[0].xsize(),(*_vol_rep)[0].ysize(),(*_vol_rep)[0].zsize(),9);
    out.setdims((*_vol_rep)[0].xdim(),(*_vol_rep)[0].ydim(),(*_vol_rep)[0].zdim(),1.0);
    out[0] = xwarpgrad[0]; out[1] = ywarpgrad[0]; out[2] = zwarpgrad[0];
    out[3] = xwarpgrad[1]; out[4] = ywarpgrad[1]; out[5] = zwarpgrad[1];
    out[6] = xwarpgrad[2]; out[7] = ywarpgrad[2]; out[8] = zwarpgrad[2];
    NEWIMAGE::write_volume4D(out,"numeric_derivs");
    */
    return(jac);
  }
  else if (_type==FnirtSplineDispType || _type==FnirtDCTDispType) {
    volume<float>  jac(FieldSize()[0],FieldSize()[1],FieldSize()[2]);
    jac.setdims(VoxelSize()[0],VoxelSize()[1],VoxelSize()[2]);
    if (inc_aff) deffield2jacobian(*(_coef_rep[0]),*(_coef_rep[1]),*(_coef_rep[2]),AffineMat(),jac);
    else deffield2jacobian(*(_coef_rep[0]),*(_coef_rep[1]),*(_coef_rep[2]),jac);
    return(jac);
  }
  else throw FnirtFileReaderException("Jacobian: Invalid _type");
}

/////////////////////////////////////////////////////////////////////
//
// Return the Jacobian matrices of the field as a NEWIMAGE 4D volume.
//
/////////////////////////////////////////////////////////////////////
volume4D<float> FnirtFileReader::JacobianMatrix(bool inc_aff) const
{
  if (_type==FnirtFieldDispType || _type==UnknownDispType) {
    throw FnirtFileReaderException("JacobianMatrix: Not yet implemented for non-basis representations");
  }
  else if (_type==FnirtSplineDispType || _type==FnirtDCTDispType) {
    volume4D<float>  jac(FieldSize()[0],FieldSize()[1],FieldSize()[2],9);
    jac.setdims(VoxelSize()[0],VoxelSize()[1],VoxelSize()[2],1.0);
    if (inc_aff) deffield2jacobian_matrix(*(_coef_rep[0]),*(_coef_rep[1]),*(_coef_rep[2]),AffineMat(),jac);
    else deffield2jacobian_matrix(*(_coef_rep[0]),*(_coef_rep[1]),*(_coef_rep[2]),jac);
    return(jac);
  }
  else throw FnirtFileReaderException("Jacobian: Invalid _type");
}

/////////////////////////////////////////////////////////////////////
//
// Return field as an instance of splinefield class.
//
/////////////////////////////////////////////////////////////////////

splinefield FnirtFileReader::FieldAsSplinefield(unsigned int indx, vector<unsigned int> ksp, unsigned int order) const
{
  if (!ksp.size() && _type != FnirtSplineDispType) {
    throw FnirtFileReaderException("FieldAsSplineField: Must specify ksp if spline is not native type");
  }
  if (indx > 2) throw FnirtFileReaderException("FieldAsSplineField: indx out of range");
  if (_type == FnirtSplineDispType) {
    if ((!ksp.size() || ksp==KnotSpacing()) && (!order || order==SplineOrder())) {
      const splinefield& tmpref = dynamic_cast<const splinefield&>(*(_coef_rep[indx]));
      return(tmpref);
    }
    else {
      if (!order || order==SplineOrder()) {  // If we are keeping the order
        order = SplineOrder();
        std::shared_ptr<basisfield>  tmpptr = _coef_rep[indx]->ZoomField(FieldSize(),VoxelSize(),ksp);
        const splinefield&      tmpref = dynamic_cast<const splinefield&>(*tmpptr);
        return(tmpref);
      }
      else { // New order and (possibly) ksp
        volume<float>       vol(FieldAsNewimageVolume(indx));
        splinefield         rval(FieldSize(),VoxelSize(),ksp,order);
        rval.Set(vol);
        return(rval);
      }
    }
  }
  else {
    if (!order) order = 3;   // Cubic splines default
    volume<float>       vol(FieldAsNewimageVolume(indx));
    splinefield         rval(FieldSize(),VoxelSize(),ksp,order);
    rval.Set(vol);
    return(rval);
  }
}

/////////////////////////////////////////////////////////////////////
//
// Return field as an instance of dctfield class.
//
/////////////////////////////////////////////////////////////////////

dctfield FnirtFileReader::FieldAsDctfield(unsigned int indx, vector<unsigned int> order) const
{
  if (!order.size() && _type != FnirtDCTDispType) {
    throw FnirtFileReaderException("FieldAsDctfield: Must specify order if DCT is not native type");
  }
  if (indx > 2) throw FnirtFileReaderException("FieldAsSplineField: indx out of range");
  std::shared_ptr<volume<float> >   volp;
  std::shared_ptr<dctfield>         rvalp;
  if (_type == FnirtDCTDispType) {
    if (!order.size() || order==DCTOrder()) {
      const dctfield& tmpref = dynamic_cast<const dctfield&>(*(_coef_rep[indx]));
      return(tmpref);
    }
    else {
      std::shared_ptr<basisfield>  tmpptr = _coef_rep[indx]->ZoomField(FieldSize(),VoxelSize(),order);
      const dctfield& tmpref = dynamic_cast<const dctfield&>(*tmpptr);
      return(tmpref);
    }
  }
  else {
    volume<float>       vol(FieldAsNewimageVolume(indx));
    dctfield            rval(FieldSize(),VoxelSize(),order);
    rval.Set(vol);
    return(rval);
  }
}

/////////////////////////////////////////////////////////////////////
//
// Here starts globally declared "helper" routine.
//
/////////////////////////////////////////////////////////////////////

void deffield2jacobian(const BASISFIELD::basisfield&   dx,
                       const BASISFIELD::basisfield&   dy,
                       const BASISFIELD::basisfield&   dz,
                       volume<float>&                  jac)
{
  NEWMAT::IdentityMatrix  eye(4);
  deffield2jacobian(dx,dy,dz,eye,jac);
}

/////////////////////////////////////////////////////////////////////
//
// This version of deffield2jacobian will take a 3D displacement
// field and an affine matrix that can be non-unity. This means 
// that it performs quite a lot of calculations, and can be quite
// time consuming. Below there are faster versions for 1D 
// displacement fields and when the affine matrix is the unity
// matrix.
//
/////////////////////////////////////////////////////////////////////

void deffield2jacobian(const BASISFIELD::basisfield&   dx,
                       const BASISFIELD::basisfield&   dy,
                       const BASISFIELD::basisfield&   dz,
                       const NEWMAT::Matrix&           aff,
                       volume<float>&                  jac)
{
  std::shared_ptr<ColumnVector>  dxdx = dx.Get(BASISFIELD::FieldIndex(1));
  std::shared_ptr<ColumnVector>  dxdy = dx.Get(BASISFIELD::FieldIndex(2));
  std::shared_ptr<ColumnVector>  dxdz = dx.Get(BASISFIELD::FieldIndex(3));
  std::shared_ptr<ColumnVector>  dydx = dy.Get(BASISFIELD::FieldIndex(1));
  std::shared_ptr<ColumnVector>  dydy = dy.Get(BASISFIELD::FieldIndex(2));
  std::shared_ptr<ColumnVector>  dydz = dy.Get(BASISFIELD::FieldIndex(3));
  std::shared_ptr<ColumnVector>  dzdx = dz.Get(BASISFIELD::FieldIndex(1));
  std::shared_ptr<ColumnVector>  dzdy = dz.Get(BASISFIELD::FieldIndex(2));
  std::shared_ptr<ColumnVector>  dzdz = dz.Get(BASISFIELD::FieldIndex(3));

  /*
  volume4D<float>  out(dx.FieldSz_x(),dx.FieldSz_y(),dx.FieldSz_z(),9);
  out.setdims(dx.Vxs_x(),dx.Vxs_y(),dx.Vxs_z(),1.0);
  out[0].insert_vec(*dxdx); out[1].insert_vec(*dydx); out[2].insert_vec(*dzdx);
  out[3].insert_vec(*dxdy); out[4].insert_vec(*dydy); out[5].insert_vec(*dzdy);
  out[6].insert_vec(*dxdz); out[7].insert_vec(*dydz); out[8].insert_vec(*dzdz);
  NEWIMAGE::write_volume4D(out,"spline_derivs");
  */

  NEWMAT::Matrix iaff = aff.i();
  double a11=iaff(1,1), a21=iaff(2,1), a31=iaff(3,1);
  double a12=iaff(1,2), a22=iaff(2,2), a32=iaff(3,2);
  double a13=iaff(1,3), a23=iaff(2,3), a33=iaff(3,3);
  for (unsigned int indx=0, k=0; k<dx.FieldSz_z(); k++) {
    for (unsigned int j=0; j<dx.FieldSz_y(); j++) {
      for (unsigned int i=0; i<dx.FieldSz_x(); i++) {
	jac(i,j,k) = (a11+(1.0/dx.Vxs_x())*dxdx->element(indx)) * (a22+(1.0/dy.Vxs_y())*dydy->element(indx)) * (a33+(1.0/dz.Vxs_z())*dzdz->element(indx));
        jac(i,j,k) += (a12+(1.0/dx.Vxs_y())*dxdy->element(indx)) * (a23+(1.0/dy.Vxs_z())*dydz->element(indx)) * (a31+(1.0/dz.Vxs_x())*dzdx->element(indx));
	jac(i,j,k) += (a13+(1.0/dx.Vxs_z())*dxdz->element(indx)) * (a21+(1.0/dy.Vxs_x())*dydx->element(indx)) * (a32+(1.0/dz.Vxs_y())*dzdy->element(indx));
        jac(i,j,k) -= (a31+(1.0/dz.Vxs_x())*dzdx->element(indx)) * (a22+(1.0/dy.Vxs_y())*dydy->element(indx)) * (a13+(1.0/dx.Vxs_z())*dxdz->element(indx));
        jac(i,j,k) -= (a32+(1.0/dz.Vxs_y())*dzdy->element(indx)) * (a23+(1.0/dy.Vxs_z())*dydz->element(indx)) * (a11+(1.0/dx.Vxs_x())*dxdx->element(indx));
        jac(i,j,k) -= (a33+(1.0/dz.Vxs_z())*dzdz->element(indx)) * (a21+(1.0/dy.Vxs_x())*dydx->element(indx)) * (a12+(1.0/dx.Vxs_y())*dxdy->element(indx));
	indx++;
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////
//
// This version of deffield2jacobian will take a 1D displacement
// field and assume the the affine matrix is unity. This means 
// that it is a lot faster, and can be used for topup.
//
/////////////////////////////////////////////////////////////////////

void deffield2jacobian(const BASISFIELD::basisfield&   field,
		       BASISFIELD::FieldIndex          fi,
                       volume<float>&                  jac)
{
  if (fi != 1 && fi != 2) throw FnirtFileReaderException("deffield2jacobian: fi must be 1 or 2");

  std::shared_ptr<ColumnVector>  df = field.Get(fi);

  double vxs = (fi == 1) ? field.Vxs_x() : field.Vxs_y();
  for (unsigned int indx=0, k=0; k<field.FieldSz_z(); k++) {
    for (unsigned int j=0; j<field.FieldSz_y(); j++) {
      for (unsigned int i=0; i<field.FieldSz_x(); i++) {
	jac(i,j,k) = (1.0 + (1.0/vxs)*df->element(indx));
	indx++;
      }
    }
  }
}

/****************************************************************//**
*
*  A global function that calculates the Jacoban matrix given
*  displacement fields in the x-, y- and z-directions.
*  \param[in] dx Displacement field (in mm) in x-direction
*  \param[in] dx Displacement field (in mm) in y-direction
*  \param[in] dx Displacement field (in mm) in z-direction
*  \param[out] jacmat A 4D file with 9 volumes specifying the
*  full Jacobian matrix. It is organised in the same way as
*  you would get in Matlab with J(:). In other words the order
*  is [W_x/dx W_y/dx W_z/dx W_x/dy W_y/dy ... W_z/dz]
*
********************************************************************/
void deffield2jacobian_matrix(const BASISFIELD::basisfield&   dx,
			      const BASISFIELD::basisfield&   dy,
			      const BASISFIELD::basisfield&   dz,
			      volume4D<float>&                jacmat)
{
  NEWMAT::IdentityMatrix  eye(4);
  deffield2jacobian_matrix(dx,dy,dz,eye,jacmat);
}

/****************************************************************//**
*
*  A global function that calculates the Jacoban matrix given
*  displacement fields in the x-, y- and z-directions.
*  \param[in] dx Displacement field (in mm) in x-direction
*  \param[in] dx Displacement field (in mm) in y-direction
*  \param[in] dx Displacement field (in mm) in z-direction
*  \param[in] aff Affine matrix that gets added to the
*  non-linear warps.
*  \param[out] jacmat A 4D file with 9 volumes specifying the
*  full Jacobian matrix. It is organised in the same way as
*  you would get in Matlab with J(:). In other words the order
*  is [W_x/dx W_y/dx W_z/dx W_x/dy W_y/dy ... W_z/dz]
*
********************************************************************/
void deffield2jacobian_matrix(const BASISFIELD::basisfield&   dx,
			      const BASISFIELD::basisfield&   dy,
			      const BASISFIELD::basisfield&   dz,
			      const NEWMAT::Matrix&           aff,
			      volume4D<float>&                jacmat)
{
  const std::shared_ptr<ColumnVector>  dxdx = dx.Get(BASISFIELD::FieldIndex(1));
  const std::shared_ptr<ColumnVector>  dxdy = dx.Get(BASISFIELD::FieldIndex(2));
  const std::shared_ptr<ColumnVector>  dxdz = dx.Get(BASISFIELD::FieldIndex(3));
  const std::shared_ptr<ColumnVector>  dydx = dy.Get(BASISFIELD::FieldIndex(1));
  const std::shared_ptr<ColumnVector>  dydy = dy.Get(BASISFIELD::FieldIndex(2));
  const std::shared_ptr<ColumnVector>  dydz = dy.Get(BASISFIELD::FieldIndex(3));
  const std::shared_ptr<ColumnVector>  dzdx = dz.Get(BASISFIELD::FieldIndex(1));
  const std::shared_ptr<ColumnVector>  dzdy = dz.Get(BASISFIELD::FieldIndex(2));
  const std::shared_ptr<ColumnVector>  dzdz = dz.Get(BASISFIELD::FieldIndex(3));

  NEWMAT::Matrix iaff = aff.i();
  double a11=iaff(1,1), a21=iaff(2,1), a31=iaff(3,1);
  double a12=iaff(1,2), a22=iaff(2,2), a32=iaff(3,2);
  double a13=iaff(1,3), a23=iaff(2,3), a33=iaff(3,3);
  for (unsigned int indx=0, k=0; k<dx.FieldSz_z(); k++) {
    for (unsigned int j=0; j<dx.FieldSz_y(); j++) {
      for (unsigned int i=0; i<dx.FieldSz_x(); i++) {
	jacmat(i,j,k,0) = a11+(1.0/dx.Vxs_x())*dxdx->element(indx);
	jacmat(i,j,k,1) = a21+(1.0/dy.Vxs_x())*dydx->element(indx);
	jacmat(i,j,k,2) = a31+(1.0/dz.Vxs_x())*dzdx->element(indx);
	jacmat(i,j,k,3) = a12+(1.0/dx.Vxs_y())*dxdy->element(indx);
        jacmat(i,j,k,4) = a22+(1.0/dy.Vxs_y())*dydy->element(indx);
        jacmat(i,j,k,5) = a32+(1.0/dz.Vxs_y())*dzdy->element(indx);
	jacmat(i,j,k,6) = a13+(1.0/dx.Vxs_z())*dxdz->element(indx);
        jacmat(i,j,k,7) = a23+(1.0/dy.Vxs_z())*dydz->element(indx);
	jacmat(i,j,k,8) = a33+(1.0/dz.Vxs_z())*dzdz->element(indx);
	indx++;
      }
    }
  }
  return;
}

void add_or_remove_affine_part(const NEWMAT::Matrix&     aff,
                               unsigned int              indx,
                               bool                      add,
                               NEWIMAGE::volume<float>&  warps)
{
  if (indx > 2) throw FnirtFileReaderException("add_affine_part: indx out of range");
  if ((aff-IdentityMatrix(4)).MaximumAbsoluteValue() > 1e-6) {
    Matrix        M = (aff.i() - IdentityMatrix(4)) * warps.sampling_mat();
    ColumnVector  mr(4);
    for (unsigned int i=1; i<=4; i++) mr(i) = M(indx+1,i);
    ColumnVector  xv(4);
    int zs = warps.zsize(), ys = warps.ysize(), xs = warps.xsize();
    xv(4) = 1.0;
    for (int z=0; z<zs; z++) {
      xv(3) = double(z);
      for (int y=0; y<ys; y++) {
        xv(2) = double(y);
        for (int x=0; x<xs; x++) {
          xv(1) = double(x);
          if (add) warps(x,y,z) += DotProduct(mr,xv);
          else warps(x,y,z) -= DotProduct(mr,xv);
	}
      }
    }
  }
}

void add_affine_part(const NEWMAT::Matrix&     aff,
                     unsigned int              indx,
                     NEWIMAGE::volume<float>&  warps)
{
  add_or_remove_affine_part(aff,indx,true,warps);
}

void remove_affine_part(const NEWMAT::Matrix&     aff,
                        unsigned int              indx,
                        NEWIMAGE::volume<float>&  warps)
{
  add_or_remove_affine_part(aff,indx,false,warps);
}

/////////////////////////////////////////////////////////////////////
//
// Estimates an affine component as an "average" of the non-linear
// warps. Useful when one need to divide a "non-fnirt" field into
// an affine and a non-linear part.
// This can be coded MUCH more efficiently if it turns out to
// take a significant time/memory.
//
/////////////////////////////////////////////////////////////////////
NEWMAT::Matrix estimate_affine_part(NEWIMAGE::volume4D<float>&  warps,
                                    unsigned int                every)
{
  NEWMAT::Matrix B = warps.sampling_mat();
  double b11, b12, b13, b14;
  double b21, b22, b23, b24;
  double b31, b32, b33, b34;
  b11=B(1,1); b12=B(1,2); b13=B(1,3); b14=B(1,4);
  b21=B(2,1); b22=B(2,2); b23=B(2,3); b24=B(2,4);
  b31=B(3,1); b32=B(3,2); b33=B(3,3); b34=B(3,4);

  NEWMAT::Matrix aff(4,4);
  aff = 0.0;
  aff(4,4) = 1.0;
  // Create "design matrix"
  NEWMAT::Matrix X(warps.xsize()*warps.ysize()*warps.zsize(),4);
  NEWMAT::RowVector yp(warps.xsize()*warps.ysize()*warps.zsize());
  for (int k=0, n=0; k<warps.zsize(); k+=every) {
    for (int j=0; j<warps.ysize(); j+=every) {
      for (int i=0; i<warps.xsize(); i+=every, n++) {
        X(n+1,1)=b11*i+b12*j+b13*k+b14;
        X(n+1,2)=b21*i+b22*j+b23*k+b24;
        X(n+1,3)=b31*i+b32*j+b33*k+b34;
        X(n+1,4)=1.0;
      }
    }
  }
  NEWMAT::Matrix iXtX = (X.t()*X).i();

  // Solve for x, y and z "data vectors".
  for (int indx=0; indx<3; indx++) {
    for (int k=0, n=0; k<warps.zsize(); k+=every) {
      for (int j=0; j<warps.ysize(); j+=every) {
        for (int i=0; i<warps.xsize(); i+=every, n++) {
          switch (indx) {
	  case 0:
	    yp(n+1)=b11*i+b12*j+b13*k+b14+warps[0](i,j,k);
	    break;
	  case 1:
	    yp(n+1)=b21*i+b22*j+b23*k+b24+warps[1](i,j,k);
	    break;
	  case 2:
	    yp(n+1)=b31*i+b32*j+b33*k+b34+warps[2](i,j,k);
	    break;
	  default:
	    break;
	  }
	}
      }
    }
    aff.Row(indx+1) = (yp*X)*iXtX;
  }
  aff = aff.i();   // It is really the inverse we want

  return(aff);
}

void fin_diff_gradient_on_voxel_centres(const NEWIMAGE::volume<float>& warp,
					NEWIMAGE::volume4D<float>&     grad)
{
  for (int k=0; k<warp.zsize(); k++) {
    for (int j=0; j<warp.ysize(); j++) {
      for (int i=0; i<warp.xsize(); i++) {
        // x-dir
	if (i==0) grad(i,j,k,0) = warp(i+1,j,k) - warp(i,j,k);
	else if (i==(warp.xsize()-1)) grad(i,j,k,0) = warp(i,j,k) - warp(i-1,j,k);
	else grad(i,j,k,0) = (warp(i+1,j,k) - warp(i-1,j,k)) / 2.0;
	// y-dir
	if (j==0) grad(i,j,k,1) = warp(i,j+1,k) - warp(i,j,k);
	else if (j==(warp.ysize()-1)) grad(i,j,k,1) = warp(i,j,k) - warp(i,j-1,k);
	else grad(i,j,k,1) = (warp(i,j+1,k) - warp(i,j-1,k)) / 2.0;
	// z-dir
	if (k==0) grad(i,j,k,2) = warp(i,j,k+1) - warp(i,j,k);
	else if (k==(warp.zsize()-1)) grad(i,j,k,2) = warp(i,j,k) - warp(i,j,k-1);
	else grad(i,j,k,2) = (warp(i,j,k+1) - warp(i,j,k-1)) / 2.0;
      }
    }
  }
}

void fin_diff_gradient2jacobian(const NEWIMAGE::volume4D<float>& xw,
				const NEWIMAGE::volume4D<float>& yw,
				const NEWIMAGE::volume4D<float>& zw,
				NEWIMAGE::volume<float>&         jac)
{
  float xvxs_i = 1.0/xw.xdim();  // Used to convert gradient from mm/voxel to mm/mm
  float yvxs_i = 1.0/xw.ydim();  // Used to convert gradient from mm/voxel to mm/mm
  float zvxs_i = 1.0/xw.zdim();  // Used to convert gradient from mm/voxel to mm/mm
  for (int k=0; k<xw.zsize(); k++) {
    for (int j=0; j<xw.ysize(); j++) {
      for (int i=0; i<xw.xsize(); i++) {
	jac(i,j,k) = (1.0+xvxs_i*xw(i,j,k,0)) * (1.0+yvxs_i*yw(i,j,k,1)) * (1.0+zvxs_i*zw(i,j,k,2));
        jac(i,j,k) += yvxs_i*xw(i,j,k,1) * zvxs_i*yw(i,j,k,2) * xvxs_i*zw(i,j,k,0);
	jac(i,j,k) += zvxs_i*xw(i,j,k,2) * xvxs_i*yw(i,j,k,0) * yvxs_i*zw(i,j,k,1);
        jac(i,j,k) -= xvxs_i*zw(i,j,k,0) * (1.0+yvxs_i*yw(i,j,k,1)) * zvxs_i*xw(i,j,k,2);
        jac(i,j,k) -= yvxs_i*zw(i,j,k,1) * zvxs_i*yw(i,j,k,2) * (1.0+xvxs_i*xw(i,j,k,0));
        jac(i,j,k) -= (1.0+zvxs_i*zw(i,j,k,2)) * xvxs_i*yw(i,j,k,0) * yvxs_i*xw(i,j,k,1);
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////
//
// Here starts private helper routines
//
/////////////////////////////////////////////////////////////////////

void FnirtFileReader::common_read(const string& fname, AbsOrRelWarps wt, bool verbose)
{
  // Read volume indicated by fname
  volume4D<float>   vol;
  read_volume4D(vol,fname);
  if (vol.tsize() != 3) throw FnirtFileReaderException("FnirtFileReader: Displacement fields must contain 3 volumes");

  Matrix qform;

  // Take appropriate action depending on intent code of volume
  switch (vol.intent_code()) {
  case FSL_CUBIC_SPLINE_COEFFICIENTS:
  case FSL_QUADRATIC_SPLINE_COEFFICIENTS:
  case FSL_DCT_COEFFICIENTS:                                 // Coefficients generated by FSL application (e.g. fnirt)
    read_orig_volume4D(vol,fname);                           // Re-read coefficients "raw"
    _aff = vol.sform_mat();                                  // Affine part of transform
    _aor = RelativeWarps;                                    // Relative warps
    _coef_rep = read_coef_file(vol,verbose);
    if (vol.intent_code() == FSL_CUBIC_SPLINE_COEFFICIENTS ||
        vol.intent_code() == FSL_QUADRATIC_SPLINE_COEFFICIENTS) _type = FnirtSplineDispType;
    else if (vol.intent_code() == FSL_DCT_COEFFICIENTS) _type = FnirtDCTDispType;
    break;
  case FSL_FNIRT_DISPLACEMENT_FIELD:                                    // Field generated by fnirt
    _type = FnirtFieldDispType;
    _aor = RelativeWarps;                                               // Relative warps
    _aff = estimate_affine_part(vol);                                   // Get (possible) affine component
    for (int i=0; i<3; i++) {
      ShadowVolume<float> voli(vol[i]);
      remove_affine_part(_aff,i,voli);          // Siphon off the affine component
    }
    _vol_rep = std::shared_ptr<volume4D<float> >(new volume4D<float>(vol));  // Represent as volume
    break;
  default:                                                              // Field generated by "unknown" application
    _type = UnknownDispType;
    _aor =wt;                                                           // Trust the user
    _vol_rep = std::shared_ptr<volume4D<float> >(new volume4D<float>(vol));  // Represent as volume
    _aff = IdentityMatrix(4);                                           // Affine part already included
    // Convert into relative warps (if neccessary)
    if (wt==AbsoluteWarps) convertwarp_abs2rel(*_vol_rep);
    else if (wt==UnknownWarps) {
      if (verbose) cout << "Automatically determining absolute/relative warp convention" << endl;
      float stddev0 = (*_vol_rep)[0].stddev()+(*_vol_rep)[1].stddev()+(*_vol_rep)[2].stddev();
      convertwarp_abs2rel(*_vol_rep);
      float stddev1 = (*_vol_rep)[0].stddev()+(*_vol_rep)[1].stddev()+(*_vol_rep)[2].stddev();
      // assume that relative warp always has less stddev
      if (stddev0>stddev1) {
        // the initial one (greater stddev) was absolute
        if (verbose) cout << "Assuming warps was absolute" << endl;
      }
      else {
        // the initial one was relative
        if (verbose) cout << "Assuming warps was relative" << endl;
        convertwarp_rel2abs(*_vol_rep);  // Restore to relative, which is what we want
      }
      // By now it will be in a relative form, which allows us to estimate the affine component.
      _aff = estimate_affine_part(*_vol_rep);
      for (int i=0; i<3; i++) {
      ShadowVolume<float> voli((*_vol_rep)[i]);
	remove_affine_part(_aff,i,voli);
      }
    }
    break;
  }
}

vector<std::shared_ptr<basisfield> > FnirtFileReader::read_coef_file(const volume4D<float>&   vcoef,
                                                                     bool                     verbose) const
{
  // Collect info that we need to create the fields
  Matrix        qform = vcoef.qform_mat();
  if (verbose) cout << "qform = " << qform << endl;
  vector<unsigned int>   sz(3,0);
  vector<double>         vxs(3,0.0);
  for (int i=0; i<3; i++) {
    sz[i] = static_cast<unsigned int>(qform(i+1,4));
    vxs[i] = static_cast<double>(vcoef.intent_param(i+1));
  }
  if (verbose) cout << "Matrix size: " << sz[0] << "  " << sz[1] << "  " << sz[2] << endl;
  if (verbose) cout << "Voxel size: " << vxs[0] << "  " << vxs[1] << "  " << vxs[2] << endl;

  vector<std::shared_ptr<basisfield> >   fields(3);

  if (vcoef.intent_code() == FSL_CUBIC_SPLINE_COEFFICIENTS ||
      vcoef.intent_code() == FSL_QUADRATIC_SPLINE_COEFFICIENTS) {        // Interpret as spline coefficients
    if (verbose) cout << "Interpreting file as spline coefficients" << endl;
    vector<unsigned int>   ksp(3,0);
    unsigned int           order = 3;
    if (vcoef.intent_code() == FSL_QUADRATIC_SPLINE_COEFFICIENTS) order = 2;
    ksp[0] = static_cast<unsigned int>(vcoef.xdim() + 0.5);
    ksp[1] = static_cast<unsigned int>(vcoef.ydim() + 0.5);
    ksp[2] = static_cast<unsigned int>(vcoef.zdim() + 0.5);
    if (verbose) cout << "Knot-spacing: " << ksp[0] << "  " << ksp[1] << "  " << ksp[2] << endl;
    if (verbose) cout << "Size of coefficient matrix: " << vcoef.xsize() << "  " << vcoef.ysize() << "  " << vcoef.zsize() << endl;
    for (int i=0; i<3; i++) {
      fields[i] = std::shared_ptr<splinefield>(new splinefield(sz,vxs,ksp,order));
    }
    // Sanity check
    if (fields[0]->CoefSz_x() != static_cast<unsigned int>(vcoef.xsize()) ||
        fields[0]->CoefSz_y() != static_cast<unsigned int>(vcoef.ysize()) ||
        fields[0]->CoefSz_z() != static_cast<unsigned int>(vcoef.zsize())) {
      throw FnirtFileReaderException("read_coef_file: Coefficient file not self consistent");
    }
  }
  else if (vcoef.intent_code() == FSL_DCT_COEFFICIENTS) {   // Interpret as DCT coefficients
    if (verbose) cout << "Interpreting file as DCT coefficients" << endl;
    std::vector<unsigned int>   order(3);
    order[0] = static_cast<unsigned int>(vcoef.xsize());
    order[1] = static_cast<unsigned int>(vcoef.ysize());
    order[2] = static_cast<unsigned int>(vcoef.zsize());
    if (verbose) cout << "Size of coefficient matrix: " << vcoef.xsize() << "  " << vcoef.ysize() << "  " << vcoef.zsize() << endl;
    for (int i=0; i<3; i++) {
      fields[i] = std::shared_ptr<dctfield>(new dctfield(sz,vxs,order));
    }
  }

  // Set the coefficients from the file
  for (int i=0; i<3; i++) {
    fields[i]->SetCoef(vcoef[i].vec());
  }

  // Return vector of fields

  return(fields);
}

/*
void FnirtFileReader::add_affine_part(Matrix aff, unsigned int indx, volume<float>& warps) const
{
  if (indx > 2) throw FnirtFileReaderException("add_affine_part: indx out of range");
  if ((aff-IdentityMatrix(4)).MaximumAbsoluteValue() > 1e-6) {
    Matrix        M = (aff.i() - IdentityMatrix(4)) * warps.sampling_mat();
    ColumnVector  mr(4);
    for (unsigned int i=1; i<=4; i++) mr(i) = M(indx+1,i);
    ColumnVector  xv(4);
    int zs = warps.zsize(), ys = warps.ysize(), xs = warps.xsize();
    xv(4) = 1.0;
    for (int z=0; z<zs; z++) {
      xv(3) = double(z);
      for (int y=0; y<ys; y++) {
        xv(2) = double(y);
        for (int x=0; x<xs; x++) {
          xv(1) = double(x);
          warps(x,y,z) += DotProduct(mr,xv);
	}
      }
    }
  }
}
*/

} // End namespace NEWIMAGE
